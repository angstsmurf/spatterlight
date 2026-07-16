// aslx-runtime.hh -- Quest 5 script + expression evaluator (milestone 2).
//
// Runs a hand-written .aslx (msg / if / set / while / for / foreach / functions)
// on top of the milestone-1 element tree, with no Core library. The expression
// grammar and script-command semantics mirror QuestViva's src/Engine
// (QuestNCalcLogicalExpressionParser, ScriptFactory + Scripts/, ExpressionOwner
// / StringFunctions). See TODO-quest5.md §2.

#ifndef GEAS_ASLX_RUNTIME_HH
#define GEAS_ASLX_RUNTIME_HH

#include "aslx.hh"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace aslx {

// Per-invocation execution state: local variables (Quest's Context.Parameters),
// the pending return value, and the return flag (v550+ `return` stops the block).
struct Context {
    std::map<std::string, Value> locals;
    Value return_value;
    bool returned = false;
};

// Deterministic RNG: xoshiro128** seeded via SplitMix32, byte-identical to
// Spatterlight's erkyrath_random() (terps/common_utils/randomness.c). Used for
// GetRandomInt / GetRandomDouble so the native engine matches the oracle.
struct Rng {
    uint32_t s[4];
    void seed(uint32_t seed);
    uint32_t next();
    // Inclusive [lo, hi], per terps/scarier/a5rand.cpp a5rand_between.
    long between(long lo, long hi);
    double next_double();
};

// AST forward declarations (defined in the .cc).
struct Expr;
struct Stmt;

// Thrown by a synchronous `play sound` to abandon the remainder of the turn:
// QuestViva parks the whole async chain on the wait slot until the UI reports
// the sound finished, and headless (like the oracle) nothing ever does, so
// everything after the statement -- including the rest of HandleCommand and
// FinishTurn -- silently never runs. Not an error (script boundaries pass it
// through un-reported; deliberately not a std::exception). The engine catches
// it at its own turn boundaries (send_command / prompt callbacks / tick);
// hosts that call call_function or run_script directly must catch it too.
struct TurnSuspended {};

// A pending engine-level prompt callback: the compiled callback body (owned by
// the never-freed script cache, so the pointer is stable) plus a snapshot of
// the locals at prompt time. Mirrors the (IScript, Context) pair QuestViva's
// prompt scripts capture in AwaitResponseAndRunCallbackAsync.
struct PendingCallback {
    const std::vector<Stmt> *body = nullptr;
    Context ctx;
};

// An active `show menu` prompt, exposed to the host so it can render the
// options and answer with set_menu_response (QuestViva's MenuData).
struct MenuData {
    std::string caption;
    // key -> display text, in authored order. A stringlist option is its own
    // key; a stringdictionary provides distinct keys and texts.
    std::vector<std::pair<std::string, std::string>> options;
    bool allow_cancel = false;
};

// One row of a UI pane list (QuestViva's ListData): the pane label (already
// run through GetListDisplayAlias, so it may carry {}-processed markup), the
// object's display verbs for a verb menu, the element id, and the plain
// display alias (what a typed command would call it).
struct ListData {
    std::string text;
    std::vector<std::string> verbs;
    std::string element_name;
    std::string display_alias;
};

// A .NET-flavoured regex compiled for std::regex, plus the ordered names of its
// capture groups (empty string for an unnamed group). Defined in the .cc; the
// parser primitives (IsRegexMatch/GetMatchStrength/Populate) use it. Mirrors
// QuestViva's RegexCache entry (System.Text.RegularExpressions.Regex).
struct CompiledRegex;

// One logged state change (the IUndoAction family in UndoLogger.cs / Fields.cs
// / ObjectFactory.cs / QuestList.cs). A transaction's actions are applied in
// reverse on rollback.
struct UndoAction {
    enum class Kind {
        FieldSet,     // own attribute written: restore old (or remove if added)
        FieldRemove,  // own attribute removed (v530+ null assignment): restore
        SortIndex,    // a parent write bumped sort_index: restore the old one
        Create,       // element created: unregister its name again
        Destroy,      // element destroyed: re-register the (kept-alive) storage
        ListAdd,      // entry appended to a shared list backing: erase at index
        ListRemove,   // entry removed: re-insert at index
        DictAdd,      // entry added to a shared dict backing: remove the key
        DictRemove,   // entry removed: re-insert at index
        FirstTime,    // a firsttime block ran: clear its flag (UndoFirstTime)
    };
    Kind kind;
    std::string element;    // FieldSet/FieldRemove/SortIndex/Create/Destroy:
                            // element name, re-resolved at undo time
                            // (UndoFieldSet stores names, not references)
    std::string attr;       // field name, or the dictionary key
    Value old_value;        // old field value / list item / dict value -- kept
                            // verbatim (same backing), like SetFromUndo
    bool added = false;     // FieldSet: no own attribute existed before
    long index = 0;         // list/dict position, or the old sort_index
    // List/dict actions hold the live backing itself (QuestViva's UndoListAdd
    // keeps the IQuestList reference), so they hit the right collection even
    // after the owning field was overwritten and restored.
    std::shared_ptr<std::vector<Value>> list_backing;
    std::shared_ptr<std::vector<Value::DictEntry>> dict_backing;
    Element *element_ptr = nullptr;   // Destroy: storage to re-register (the
                                      // element object stays alive, like
                                      // CreateDestroyLogEntry)
    std::shared_ptr<bool> ran;        // FirstTime: the flag to clear
};

// UndoLogger.Transaction: everything one player command changed. The
// `previous` chain mirrors PreviousTransaction, which RollbackTransaction
// steps back through so the next `start transaction` re-commits the right
// record (including QuestViva's re-commit-after-undo quirk).
struct UndoTransaction {
    std::string description;          // the command text, echoed by UndoTurn
    std::vector<UndoAction> actions;
    std::shared_ptr<UndoTransaction> previous;
};

// The interpreter: owns the loaded World, an output sink, and the RNG.
class Interp {
public:
    explicit Interp(World &world);
    ~Interp();

    // Output sink -- defaults to appending to `output()`. Override to route
    // msg/print through Glk in the real app.
    std::function<void(const std::string &)> print;

    const std::string &output() const { return output_; }
    void clear_output() { output_.clear(); }

    std::vector<std::string> &errors() { return world_.errors; }
    // The RNG stream of the expression currently evaluating (QuestViva has one
    // fresh seed-1234 RNG per compiled expression); rng_ is the fallback for
    // draws made outside any expression.
    Rng &active_rng() { return current_rng_ ? *current_rng_ : rng_; }
    // Raise a runtime script error: appends the innermost executing <function>
    // name ("... (in HandleCommand)") and THROWS, aborting the current script
    // body -- QuestViva semantics, where expression/script exceptions unwind to
    // the nearest RunScriptAsync, which logs "Error running script: ..." and
    // lets the outer script continue. run_script is that boundary here.
    [[noreturn]] void error(const std::string &message);
    Rng &rng() { return rng_; }

    // Run a raw script source string (a function body / command script) in ctx.
    void run_script(const std::string &source, Context &ctx);

    // Evaluate an expression source string in ctx.
    Value eval(const std::string &source, Context &ctx);

    // Call a game <function> by name with already-evaluated args.
    Value call_function(const std::string &name, std::vector<Value> args,
                        Context *caller);

    // Flush queued `on ready` callbacks, FIFO. With the QuestViva pending-
    // callback model an `on ready` runs immediately unless a prompt callback
    // (show menu / ask / get input / wait) is outstanding, so this only has
    // work to do -- and only runs -- while no prompt is pending; resolving the
    // prompt flushes the queue itself.
    void drain_on_ready();
    bool has_pending_on_ready() const { return !on_ready_.empty(); }

    // -- input model (TODO §3) ------------------------------------------------
    // Engine-level prompts are fire-and-forget (QuestViva): the script command
    // registers a callback and execution continues; the host later resolves it.
    // At the prompt, the host should check, in this order: pending_menu() ->
    // set_menu_response, pending_question() -> set_question_response,
    // pending_wait() -> finish_wait (no player text), else send_command (which
    // also feeds a pending `get input`).

    // Player input entry point (WorldModel.SendCommand): routes the line to a
    // pending `get input` callback if one is registered (command override),
    // otherwise echoes (pre-v520 games) and calls Core's HandleCommand, then
    // FinishTurn for pre-v580 games.
    void send_command(const std::string &command);

    // Active `show menu` prompt, or nullptr.
    const MenuData *pending_menu() const { return menu_pending_ ? &menu_ : nullptr; }
    // Resolve it: key must be an option key, or nullptr to cancel (only
    // meaningful when allow_cancel; QuestViva passes null through regardless).
    void set_menu_response(const std::string *key);

    // Active `ask` prompt caption, or nullptr. The engine does not print the
    // caption -- displaying the question is the host's job (PlayerUI.ShowQuestion).
    const std::string *pending_question() const {
        return question_pending_ ? &question_ : nullptr;
    }
    void set_question_response(bool response);

    // Active `wait` prompt (PlayerUI.DoWait). finish_wait() = the keypress.
    bool pending_wait() const { return wait_pending_; }
    void finish_wait();

    // True when a `get input` callback will consume the next send_command line.
    bool command_override() const { return command_override_; }

    // ASLEvent bridge (WorldModel.SendEventCore): call the named <function>
    // with one parameter -- how the reference player routes hyperlink onclicks
    // (Core ShowMenu options). Prints "Error - no handler ..." when the
    // function is missing; on v540+ runs FinishTurn (pre-v580) and refreshes
    // the panes afterwards, exactly like a real command.
    void send_event(const std::string &name, const std::string &param);

    // -- pane updates (WorldModel.UpdateListsAsync) ---------------------------
    // After every turn boundary (send_command / menu / question / wait / tick;
    // hosts add the boot and post-restore calls, like Begin does) the engine
    // refreshes the UI panes: the places-and-objects / inventory / exits lists
    // (only when an update_list subscriber exists -- QuestViva skips the whole
    // computation when the UpdateList event is null, and the oracle never
    // subscribes) and then Core's UpdateStatusAttributes, which runs REGARDLESS
    // of subscribers (it ends in JS.updateStatus). Any failure is
    // LogException-only: logged, never printed, never fed to the breaker.
    void update_lists();

    // Host hook for the pane lists. listtype is "placesobjects" (Quest's
    // "Places and Objects": GetPlacesObjectsList scope plus ALL exits -- the
    // UI filters compass directions out so they only show in the compass),
    // "inventory" (ScopeInventory), or "exits" (ScopeExits/GetExitsList,
    // including compass directions). Unset, the lists are never computed.
    std::function<void(const char *listtype,
                       const std::vector<ListData> &)> update_list;

    // Host hook for JS.updateStatus -- Core's UpdateStatusAttributes joins the
    // configured status attributes ("Score: 3" etc.) with "<br/>" and sends
    // them here every turn. Unset, the call is ignored (arg unevaluated).
    std::function<void(const std::string &html)> update_status;

    // Host hook for JS.updateLocation -- Core sends
    // CapFirst(GetDisplayName(game.pov.parent)) at InitInterface and with
    // every room description; the reference player shows it above the
    // transcript. Old (v500-era) games with embedded libraries never call it.
    // Unset, the call is ignored (arg unevaluated).
    std::function<void(const std::string &text)> update_location;

    // -- undo (UndoLogger port) ----------------------------------------------
    // The logger only records while a transaction is open. Core's parser opens
    // one per successfully-parsed player command ("start transaction
    // (game.pov.currentcommand)" in CoreParser.aslx, skipped for <isundo/>
    // commands), and the open transaction is committed lazily by the NEXT
    // `start transaction` or by `undo` -- there is no end-of-turn commit.
    // Boot/StartGame changes are therefore never undoable, exactly like
    // QuestViva (m_logging is false until the first command).

    // `start transaction (cmd)` -- UndoLogger.RollTransaction: commit the open
    // transaction (dropped silently if it logged nothing) and open a new one.
    void roll_transaction(const std::string &command);
    // `undo` -- UndoScript -> UndoLogger.RollbackTransaction: commit the open
    // transaction, roll back ONE transaction (printing the UndoTurn dynamic
    // template, or NothingToUndo when the stack is empty), and step the
    // current-transaction chain back.
    void rollback_transaction(Context &ctx);

    // -- save/restore (TODO §5) ------------------------------------------------
    // v1 snapshot format (not Quest's ASLX re-serialization -- that is the
    // optional native-compat milestone): a restore RELOADS the original game
    // file into a fresh World/Interp and applies the snapshot, so only the
    // dynamic state is serialized -- every live element's identity (elem_type,
    // inherits, sort_index) and own fields (full recursive Values: nested
    // lists/dicts, scripts as source, object refs by name), plus which
    // load-time elements were destroyed, runtime-created elements, and the
    // `firsttime` flags (keyed by script source text, preorder). Undo history
    // and RNG streams are not saved, like QuestViva. The host must follow the
    // reference's saved-game boot: skip StartGame and begin_timers, re-run
    // InitInterface, print "Loaded saved game".

    // Serialize the current state. `original_file` is recorded like the
    // <asl original=""> marker (informational).
    std::string save_game(const std::string &original_file);
    // Apply a snapshot onto this freshly-loaded, not-yet-booted Interp.
    // Returns false with `err` set on malformed data or a wrong-game save.
    bool restore_game(const std::string &data, std::string &err);
    // Cheap sniff for "is this buffer one of our saves?".
    static bool is_save_data(const char *data, size_t len);

    // Host hook for `request (RequestSave, ...)` / `requestsave` (Core's
    // `save` command): the UI prompts for a file and calls save_game. Unset,
    // the request warns once like the other unsupported UI requests.
    std::function<void()> request_save;

    // Host hook for `picture`. QuestViva's PictureScript has two paths: on
    // v540+ it PRINTS an <img src="..."> through Core's OutputText (the HTML
    // renderer displays it), while pre-540 it calls the UI directly
    // (PlayerUi.ShowPictureAsync) -- that direct path lands here, with the
    // evaluated filename. When set, the media warn_once for `picture` is
    // suppressed on both paths (the host renders images). Unset, pre-540
    // pictures no-op with the one-time warning as before.
    std::function<void(const std::string &filename)> show_picture;

    // Host hook for JS.setPanelContents -- Quest's picture frame, a
    // persistent panel above the transcript in the reference player. Core's
    // SetFramePicture(file) passes '<img src="URL" onload=.../>' (URL = the
    // filename here, like GetFileURL) and ClearFramePicture passes "";
    // OnEnterRoom drives it from the room's `picture` attribute. A paneless
    // host can render the picture inline when it changes. Unset, the call is
    // ignored with its argument unevaluated, as before.
    std::function<void(const std::string &html)> set_panel_contents;

    // Host hooks for `play sound` / `stop sound` (PlaySoundScript /
    // StopSoundScript -> IPlayer.PlaySound/StopSound). play_sound receives
    // the evaluated filename plus the synchronous and loop flags; a
    // synchronous play must BLOCK until playback finishes (QuestViva awaits
    // the wait slot the statement claims -- the enclosing script, and the
    // rest of the turn, resume when the sound ends). Unset, the commands
    // keep their headless semantics: a one-time warning, and a synchronous
    // play abandons the rest of the turn (TurnSuspended), exactly like the
    // oracle with no UI to report the sound finished.
    std::function<void(const std::string &filename, bool synchronous,
                       bool loop)> play_sound;
    std::function<void()> stop_sound;

    // Synchronous provider for the EXPRESSION form of ShowMenu
    // (ExpressionOwner.ShowMenu, which AWAITS the response mid-expression and
    // returns the selected key). A synchronous host must supply the answer in
    // place: the next script line in harnesses, a nested prompt loop in Glk.
    // Return true with `key` set for a selection, false for cancel. Unset,
    // the expression form reports an error.
    std::function<bool(const MenuData &, std::string &key)> menu_provider;

    // -- timers (TimerRunner port, TODO §3) -----------------------------------
    // Quest timers are <timer> elements with `enabled`/`interval`/`trigger`/
    // `script` fields against a game.timeelapsed clock; the enable/disable/
    // SetTimeout layer is Core ASLX (CoreTimers.aslx). The engine only needs
    // the clock: the host advances it with tick(seconds) -- at prompt level in
    // the real app (glk_request_timer_events), deterministically in harnesses.

    // Arm every enabled timer for its first interval (TimerRunner's
    // initialise). Call once at game start, before InitInterface/StartGame;
    // a restored game skips it.
    void begin_timers();

    // Advance game.timeelapsed by `seconds` and run every enabled timer that
    // comes due, each with `this` = the timer and its own error boundary
    // (WorldModel.Tick -> TimerRunner.TickAndGetScripts).
    void tick(int seconds);

    // Seconds until the earliest enabled timer fires, capped at 60; 0 when no
    // timer is enabled (TimerRunner.GetTimeUntilNextTimerRuns -- the value
    // RequestNextTimerTick hands the host after every engine call).
    int next_timer_seconds();

    // True while a self-destructing SetTimeout timer (named "timeout*" by
    // Core's SetTimeoutID) is enabled. Headless harnesses drain only these,
    // so recurring authored timers never loop (oracle PendingTimeoutSeconds).
    bool has_enabled_timeout();

    // Field access with inheritance resolution (own fields, then inherited
    // types most-recent-first). Returns nullptr if unresolved.
    const Value *resolve_field(Element *e, const std::string &name);

    World &world() { return world_; }

    // True once the 20-error circuit breaker has fired (scriptErrorsFatal):
    // the game was force-finished because every script was failing. Hosts use
    // it to distinguish a wedged session from a genuine `finish` (the oracle
    // reads QuestViva's private _scriptErrorsFatal by reflection for the same
    // purpose and reports "[state=Wedged]").
    bool script_errors_fatal() const { return script_errors_fatal_; }

    // Value/runtime helpers (also used by tests).
    static std::string to_string(const Value &v);
    static bool truthy(const Value &v);

private:
    World &world_;
    std::string output_;
    Rng rng_;
    Rng *current_rng_ = nullptr;  // active per-expression stream (eval_expr)

    // Compiled-statement cache, keyed by source string (Quest caches too).
    std::map<std::string, std::shared_ptr<std::vector<Stmt>>> script_cache_;
    std::map<std::string, std::shared_ptr<Expr>> expr_cache_;

    // Compiled-regex cache, keyed by the caller's cacheID (Quest's RegexCache
    // keys on cacheID alone -- the command name -- ignoring the pattern text on
    // a hit). The 2-arg IsRegexMatch/etc forms with no cacheID compile fresh.
    std::map<std::string, std::shared_ptr<CompiledRegex>> regex_cache_;
    // Compile `pattern`, or return the cache entry for `*cache_id` if present.
    std::shared_ptr<CompiledRegex> compiled_regex(const std::string &pattern,
                                                  const std::string *cache_id);

    std::shared_ptr<std::vector<Stmt>> compile_script(const std::string &src);
    std::shared_ptr<Expr> compile_expr(const std::string &src);

    void exec_block(const std::vector<Stmt> &stmts, Context &ctx);
    void exec_stmt(const Stmt &s, Context &ctx);
    Value eval_expr(const Expr &e, Context &ctx);
    Value eval_expr_node(const Expr &e, Context &ctx);

    Value call_builtin(const std::string &name, std::vector<Value> &args,
                       bool &handled, Context &ctx);
    Value resolve_variable(const std::string &name, Context &ctx, bool &found);

    // Reserved statement-position commands (do / invoke / create / destroy /
    // set / list add / dictionary add / error / finish / undo / JS.*). Returns
    // true if `name` was one and it was executed. Checked before function and
    // built-in dispatch, matching Quest's ScriptFactory keyword precedence.
    bool exec_statement_command(const std::string &name,
                                const std::vector<std::shared_ptr<Expr>> &args,
                                Context &ctx);

    // Resolve an lvalue expression (local var or obj.attr) to the mutable Value
    // backing it (copy-on-write for inherited attrs). Used by the list/
    // dictionary mutator commands. nullptr if not an assignable location.
    Value *lvalue_of(const Expr &e, Context &ctx);

    // Storage for merged listextend reads: resolve_field builds the merged
    // list into a stable per-(element,attr) slot and returns a pointer to it
    // (rebuilt on every read, like QuestViva's Fields.GetMergedResult).
    std::map<std::string, Value> extend_cache_;

    // Record a one-per-key notice in world.warnings (e.g. "picture no-opped
    // headless") without polluting world.errors.
    void warn_once(const std::string &key, const std::string &message);
    std::vector<std::string> warned_;

    // Names of the <function> elements currently executing (call_function
    // pushes/pops); the innermost is appended to error() messages.
    std::vector<std::string> frames_;

    // Script-error accounting, mirroring WorldModel.RunScriptAsync: each error
    // is logged + printed once; after kMaxScriptErrors the session is declared
    // wedged (scriptErrorsFatal -> FinishGame) and scripts stop running. The
    // depth cap guards infinite script recursion.
    static constexpr int kMaxScriptDepth = 200;   // MaxScriptExecutionDepth
    static constexpr int kMaxScriptErrors = 20;   // MaxScriptErrors
    void report_script_error(const std::string &what);
    // WorldModel's LogException-ONLY wrappers (PrintAsync -> OutputText,
    // HandleCommandAsyncInternal, TryFinishTurnAsync, TickAsyncInternal,
    // UpdateLists/UpdateStatusAttributes): the error is logged for diagnostics
    // but neither printed nor counted toward the breaker. Only throws that
    // BYPASS RunScriptAsync's own catch reach these -- above all the depth-cap
    // throw, which fires before the callee's try block -- so a deep `msg` or
    // FinishTurn quietly does nothing instead of wedging the session (The
    // Bony King's pov.parent=null scene relies on this to recover).
    void log_exception(const std::string &what);

    // -- pane update internals (UpdateListsAsync) -----------------------------
    // GetObjectsInScopeAsync: run the named scope <function>, expect a list of
    // object refs. Throws (caught by update_lists) when the function is missing.
    std::vector<Element *> objects_in_scope(const std::string &scope);
    // One pane row (the ListData constructor calls): GetListDisplayAlias /
    // GetDisplayAlias / GetDisplayVerbs, with the pre-v520 (or Core-less)
    // fallback onto the inventoryverbs/displayverbs fields.
    ListData list_data_for(Element *obj, bool inventory);
    // GetExitsListDataAsync: ScopeExits, or GetExitsList on v530+.
    std::vector<ListData> exits_list_data();
    // UpdateStatusVariablesAsync: run Core's UpdateStatusAttributes.
    void update_status_variables();

    int script_depth_ = 0;
    int script_error_count_ = 0;
    bool script_errors_fatal_ = false;
    bool reporting_error_ = false;

    // Deferred `on ready` callbacks: a pointer to the compiled callback body
    // (owned by the never-freed script cache) plus a snapshot of the locals at
    // queue time. Only populated while a prompt callback is outstanding
    // (WorldModel._onReadyQueue); otherwise `on ready` runs immediately.
    std::vector<std::pair<const std::vector<Stmt> *, Context>> on_ready_;

    // -- input model internals -----------------------------------------------
    // WorldModel._pendingCallbackCount: >0 while a prompt callback (show menu /
    // ask / get input / wait) is registered but unresolved; `on ready` defers
    // until it returns to zero, at which point the queue flushes.
    int pending_callback_count_ = 0;
    void begin_pending_callback() { ++pending_callback_count_; }
    void end_pending_callback();
    // AddOnReady: run now (count == 0) or queue.
    void add_on_ready(const std::vector<Stmt> *body, const Context &ctx);
    // Run a prompt/on-ready callback as its own script boundary (errors are
    // reported and the engine carries on), like drain_on_ready always did.
    void run_callback_boundary(const std::vector<Stmt> *body, Context &ctx);
    // PrintAsync: route through Core's OutputText on v540+ when present (so the
    // {...} text processor runs), else the raw sink. Failures are reported and
    // swallowed, like PrintAsync's own catch.
    void print_via_core(const std::string &text, Context &ctx);

    // Element.SetFieldAsync: after a script assignment (obj.attr = v / set()),
    // run the element's "changed<attr>" script if one resolves (inheritance
    // included), with `oldvalue` bound and `this` = the element. This is what
    // triggers OnEnterRoom via the player's changedparent.
    void fire_changed_script(Element *e, const std::string &attr,
                             const Value &oldval);

    // -- undo internals (UndoLogger.cs) ----------------------------------------
    bool undo_logging_ = false;                          // m_logging
    std::shared_ptr<UndoTransaction> current_txn_;       // m_currentTransaction
    std::vector<std::shared_ptr<UndoTransaction>> undo_stack_;
    // Populated by rollbacks and cleared on every commit (EndTransaction).
    // Nothing in play mode pops it -- redo is an editor feature -- but keeping
    // it mirrors the reference lifecycle.
    std::vector<std::shared_ptr<UndoTransaction>> redo_stack_;
    void add_undo(UndoAction a);                         // AddUndoAction
    void start_transaction(const std::string &command);  // StartTransaction
    void end_transaction();                              // EndTransaction
    void undo_once(Context &ctx);                        // Undo()
    void apply_undo_action(UndoAction &a);
    // Fields.Set's undo hook: called BEFORE the own attribute is written
    // (or removed, for the v530+ null assignment); logs only when the own
    // attribute actually changes, with `added` recording whether it existed.
    void log_field_set(Element *e, const std::string &attr,
                       const Value &newval, bool removing);
    // The SortIndex metafield bump a parent write performs: log the old value
    // before overwriting (UpdateElementSortOrder's Fields.Set is undoable).
    void log_sort_index(Element *e);
    // Element creation/destruction (CreateDestroyLogEntry).
    void log_create(Element *e);
    void log_destroy(Element *e);

    // -- timer internals (TimerRunner port) -----------------------------------
    std::vector<Element *> live_timers();
    long field_int(Element *e, const char *name);
    bool timer_enabled(Element *t);
    long time_elapsed();
    void set_time_elapsed(long t);
    void increment_time(int seconds);

    // One slot per prompt kind (WorldModel's _commandInputTcs/_menuTcs/
    // _questionTcs/_waitTcs). Registering a new prompt of a kind that already
    // pends cancels the old one (BeginPrompt's TrySetCanceled): its callback is
    // dropped and its pending-callback count released.
    bool command_override_ = false;
    PendingCallback command_cb_;
    bool menu_pending_ = false;
    MenuData menu_;
    PendingCallback menu_cb_;
    bool question_pending_ = false;
    std::string question_;
    PendingCallback question_cb_;
    bool wait_pending_ = false;
    PendingCallback wait_cb_;
};

}  // namespace aslx

#endif  // GEAS_ASLX_RUNTIME_HH
