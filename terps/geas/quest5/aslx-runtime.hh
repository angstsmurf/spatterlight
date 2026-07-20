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

#include <array>
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

// Thrown by a synchronous `play sound` to park the remainder of the turn:
// QuestViva's PlaySoundScript does BeginPrompt(ref _waitTcs) and awaits the
// TCS, so the whole async chain -- including the rest of HandleCommand and
// FinishTurn -- suspends on the wait slot. Headless nothing reports the sound
// finished, but the park is NOT permanent: the next claim on the wait slot
// (a `wait` or another synchronous sound calling BeginPrompt, or a host
// FinishWait) cancels/completes the TCS, whose continuation then runs INLINE
// (default TaskCompletionSource; the OperationCanceledException is swallowed
// in PlaySoundScript), resuming everything after the sound statement. The
// unwind captures that continuation here as frames: each exec_block records
// its not-yet-run statements (innermost first), and each script boundary
// (run_script / run_callback_boundary) claims the frames below it with a
// snapshot of that script's context. Not an error (deliberately not a
// std::exception). The engine parks it at its own turn boundaries
// (send_command / prompt callbacks / tick) and resumes via
// resume_parked_tail; hosts that call call_function or run_script directly
// must catch it too.
struct TurnSuspended {
    struct Frame {
        // null stmts = script-boundary marker: `ctx` is the shared context
        // snapshot for all (unclaimed) frames pushed before it.
        const std::vector<Stmt> *stmts = nullptr;
        size_t next = 0;  // first not-yet-run statement of the block
        Context ctx;
    };
    std::vector<Frame> frames;
};

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

// One grid-map drawing command (the JS.ShowGrid / JS.Grid_* vocabulary that
// CoreGrid.aslx sends to grid.js in the reference player). The layout is all
// engine-side script -- CoreGrid computes every coordinate in grid units and
// only ever asks the UI to paint -- so a host that retains these commands in
// a display list can render the map without knowing anything about rooms.
// Coordinates are doubles in grid units (Grid_SetScale supplies the pixels
// per unit); colours are HTML names or #rrggbb, as authored.
struct GridDraw {
    enum class Op {
        Show,     // JS.ShowGrid: h = pane height in pixels (<= 0 hides it)
        Scale,    // Grid_SetScale: w = pixels per grid unit (game.mapscale)
        Box,      // Grid_DrawBox: x,y top-left, w,h size, z layer, border +
                  // borderwidth, fill, sides = which borders to draw, an
                  // NESW bitmask (8=N top, 4=E right, 2=S bottom, 1=W left);
                  // the fill always covers the full rectangle
        Label,    // Grid_DrawLabel: x,y = centred text baseline, z, text,
                  // fill = colour (grid.js PointText, centre-justified)
        Line,     // Grid_DrawLine: x,y -> x2,y2, border + borderwidth. No z:
                  // grid.js draws on the layer the last Box/Label/Player
                  // activated, so the renderer tracks that state
        Player,   // Grid_DrawPlayer: x,y centre, z, w = radius IN PIXELS
                  // (not grid units), border + borderwidth, fill. There is
                  // one player marker: a later call moves it
        Clear,    // Grid_ClearAllLayers (ChangePOV wipes the map)
    };
    Op op = Op::Clear;
    double x = 0, y = 0, x2 = 0, y2 = 0, w = 0, h = 0;
    int z = 0, borderwidth = 0, sides = 15;
    std::string border, fill, text;
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

    // The in-scope objects a QuestViva object-name hyperlink could target,
    // each carrying its display alias and the verb menu clicking it would show:
    // inventory objects with their inventoryverbs, room/place objects with
    // their displayverbs, exactly as the side pane builds them (exits are
    // excluded -- compass directions have no verb menu). Backs the front-end's
    // VERBS command; computed on demand, independent of any update_list hook.
    std::vector<ListData> verb_menu_objects();

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

    // Host hook for JS.uiShow/uiHide("#txtCommandDiv") -- the reference
    // player's command box. Core's InitInterface hides it when
    // game.showcommandbar is off, and GamebookCore hides it always (a
    // gamebook is played by clicking page links, with no parser at all),
    // re-showing it only around a `get input`. A host with no separate input
    // box should show no prompt and take no typed line while it is hidden.
    // Unset, the call is ignored (arg unevaluated).
    std::function<void(bool shown)> show_command_bar;

    // Host hook for JS.disableAllCommandLinks -- GamebookCore calls it once a
    // page choice has been taken, so the options printed above stop being
    // clickable (the reference player strips their anchors). A host that
    // keeps its transcript on screen must retire those links itself.
    std::function<void()> disable_command_links;

    // Host hooks for JS.StartOutputSection / EndOutputSection /
    // HideOutputSection (CoreFunctions.aslx:339-355). The reference player
    // wraps the enclosed output in a <div id="sectionN"> it can later hide,
    // which is how a gamebook removes the previous page's option links
    // (GamebookCore's DoPage hides game.optionsoutputsection) and how Core
    // retires a numbered menu once answered. Section names are unique and
    // sequential ("section1", "section2", ...). Unset, all three are ignored
    // (arg unevaluated) and the output simply stays on screen, which is the
    // headless/golden behaviour -- so transcripts are untouched.
    std::function<void(const std::string &name)> start_output_section;
    std::function<void(const std::string &name)> end_output_section;
    std::function<void(const std::string &name)> hide_output_section;

    // Host hook for script errors ("Error running script: ..."). WorldModel's
    // RunScript catch boundary (WorldModel.cs:1094) Prints them, and the
    // headless transcripts / goldens contain them at that spot -- but the
    // reference WEB player does not put them on the page: they surface only in
    // the browser's JavaScript console (verified against The Zen Garden's
    // duplicate-'touch' dictionary error on textadventures.co.uk). With this
    // hook set the message is routed here INSTEAD of being printed, so a
    // player-facing host can match the reference player's console-only
    // behaviour. Unset, the message prints as before -- headless output and
    // every golden stay byte-identical.
    std::function<void(const std::string &message)> script_error;

    // Host hook for the browser's JS callback bridge -- an escape valve for a
    // JS.* call this engine does not implement. Games write their own <js>
    // animations and end them by calling ASLEvent(completion), naming a game
    // function to run when the animation finishes: spondre's title screen
    // fades in through JS.FadeInTitle(..., "TitleDone"), and TitleDone is what
    // prints its "click to continue". With no JS engine those callbacks never
    // fire and the game stalls forever on its title. When this hook is set,
    // an unhandled JS.* call hands its LAST argument to the host, which
    // decides whether it names a game function worth firing -- and fires it
    // BETWEEN turns, never reentrantly mid-script, which is the closest we get
    // to the setTimeout the browser would have used. This is a heuristic: an
    // argument that merely looks like a function name will fire it. Unset, the
    // call is ignored and the argument is not even evaluated, so headless
    // output stays oracle-exact.
    std::function<void(const std::string &fn, const std::string &last_arg)>
        js_event_bridge;

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
    // Would rollback_transaction find a turn to roll back?  Mirrors what it
    // does: the transaction the last command opened is still uncommitted
    // (there is no end-of-turn commit), so it counts when it logged anything.
    // Lets a host offer undo only when it will work -- the engine itself
    // never needs to ask, since Core's undo command prints NothingToUndo.
    bool undo_available() const {
        return !undo_stack_.empty() ||
               (undo_logging_ && current_txn_ && !current_txn_->actions.empty());
    }

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
    // Auto-detects the format: a native `.quest-save` (`<asl original=...>`
    // ASLX re-serialization, Quest/QuestViva's own format) routes to
    // restore_game_native; otherwise the v1 snapshot above.
    bool restore_game(const std::string &data, std::string &err);
    // Cheap sniff for "is this buffer one of our (v1) saves?".
    static bool is_save_data(const char *data, size_t len);

    // -- Spatterlight autosave: exact RNG capture/restore ---------------------
    // The deterministic RNG streams: the fallback stream (empty key) plus
    // every compiled expression's lazily-created stream, keyed by expression
    // source (expr_cache_ dedups by source, so the key identifies the stream
    // exactly).  restore recompiles each source into the cache and overwrites
    // its stream, so the next draw continues where the capture left off;
    // expressions absent from the capture keep their fresh lazy seed.  Used
    // only by the Glk frontend's autosave -- a QuestViva save carries no RNG.
    void capture_rng_streams(
        std::vector<std::pair<std::string, std::array<uint32_t, 4>>> &out);
    void restore_rng_streams(
        const std::vector<std::pair<std::string, std::array<uint32_t, 4>>> &in);

    // -- native .quest-save (Quest/QuestViva GameSaver format) ----------------
    // Interoperable with the desktop Quest player and QuestViva: an ASLX
    // document (`<asl version=".." original="game.quest"> ... </asl>`) that
    // re-serializes the mutable object family (object/game/command/verb/exit/
    // turnscript/timer) with every own field and inherit. A restore reloads
    // the original game (done by the host before this Interp) and overlays the
    // save, each element DISPLACING its freshly-loaded twin (QuestViva's
    // Elements.Add override); family elements the save omits are dropped
    // (destroyed at save time). Static elements (functions/types/delegates/
    // templates) come back from the reload untouched -- they never change at
    // runtime -- so, unlike Quest, we do not re-emit them; the result is still
    // a valid Quest saved-game document.
    std::string save_game_native(const std::string &original_file);
    bool restore_game_native(const std::string &data, std::string &err);
    // Return `src` with already-run `firsttime` blocks baked out (a run
    // firsttime becomes its `otherwise` body, or nothing) -- QuestViva's
    // FirstTimeScript.Save behaviour, reproduced so an exported native save does
    // not re-fire one-time text. `src` unchanged when nothing has run.
    std::string bake_firsttime_source(const std::string &src);
    // Sniff for a native ASLX save (leading XML whose first element is <asl
    // ... original=...>, or our "Saved by Geas" marker comment).
    static bool is_native_save_data(const char *data, size_t len);

    // Host hook for `request (RequestSave, ...)` / `requestsave` (Core's
    // `save` command): the UI prompts for a file and calls save_game. Unset,
    // the request warns once like the other unsupported UI requests.
    std::function<void()> request_save;

    // Host hook for the restart channel. Quest has no Restart request enum:
    // Core's `restart` command (after its Ask confirmation) runs
    // `JS.eval ("window.location.reload();")` -- the web player reloads the
    // page, i.e. reboots the session from the game file; older Cores probe a
    // desktop-player RestartGame() first in the same eval. When set, JS.eval
    // scripts naming either are routed here; the host is expected to tear
    // down and boot a fresh session (the rest of the turn still runs, as it
    // does server-side under a browser reload). Unset, JS.eval stays ignored
    // with its argument unevaluated, as before.
    std::function<void()> request_restart;

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

    // Host hook for the grid map (game.gridmap): CoreGrid.aslx does all the
    // layout engine-side and sends bare paint commands (JS.ShowGrid /
    // JS.Grid_DrawBox / ...) -- see GridDraw. Arguments are evaluated only
    // when the hook is set, so headless transcripts are untouched. The
    // custom-layer vocabulary (Grid_DrawSquare, SVG, images, shapes) stays
    // silently ignored either way, like every other unclaimed JS.* call.
    std::function<void(const GridDraw &)> grid_draw;

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

    // Host hooks for the pre-v540/v550 blocking `request (Wait)` /
    // `request (Pause, ms)` (RequestScript -> WorldModel.DoWaitAsync /
    // DoPauseAsync -> IPlayer.DoWait / DoPause). Unlike the fire-and-forget
    // `wait` script command, these genuinely block mid-script in QuestViva:
    // the enclosing script -- and the rest of the turn -- resume only when the
    // wait/pause slot completes, so a synchronous host BLOCKS in the hook and
    // returns (do_wait until the player presses a key; do_pause for `ms`
    // milliseconds), after which the tail runs inline. Unset, both are a
    // silent no-op: the tail continues inline, which is byte-identical to the
    // oracle's immediate AutoAdvance (FinishWait/FinishPause) since neither
    // request prints anything and the tail runs before the turn's FinishTurn
    // either way. The version gate (Wait throws for v540+, Pause for v550+)
    // fires regardless of whether a hook is installed.
    std::function<void()> do_wait;
    std::function<void(int ms)> do_pause;

    // Synchronous provider for the EXPRESSION form of ShowMenu
    // (ExpressionOwner.ShowMenu, which AWAITS the response mid-expression and
    // returns the selected key). A synchronous host must supply the answer in
    // place: the next script line in harnesses, a nested prompt loop in Glk.
    // Return true with `key` set for a selection, false for cancel. Unset,
    // the expression form reports an error.
    std::function<bool(const MenuData &, std::string &key)> menu_provider;

    // Synchronous provider for the EXPRESSION form of Ask
    // (ExpressionOwner.Ask, which likewise AWAITS mid-expression and returns
    // the yes/no answer). Same contract as menu_provider: the question caption
    // is NOT printed -- rendering the prompt is presentation, exactly as the
    // `ask` statement leaves it to the host. Return true with `answer` set;
    // returning false means "no answer available" and yields false, matching
    // QuestViva's default-false TaskCompletionSource. Unset, the expression
    // form reports an error.
    std::function<bool(const std::string &question, bool &answer)> ask_provider;

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
    // Harness knob for the `#!errorlimit=N` script directive (default 20).
    void set_max_script_errors(int n) { max_script_errors_ = n; }

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
    void exec_block_from(const std::vector<Stmt> &stmts, size_t start,
                         Context &ctx);
    void exec_stmt(const Stmt &s, Context &ctx);

    // Parked synchronous `play sound` (see TurnSuspended): store the captured
    // continuation at a turn boundary / resume it when the wait slot is next
    // claimed (new `wait` or sync sound = BeginPrompt cancel; host
    // finish_wait = TrySetResult).
    void park_suspension(TurnSuspended &ts, bool owes_update, int owes_endcb,
                         bool owes_finishturn = false);
    void resume_parked_tail();
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
    // MaxScriptErrors. Mutable (default 20) so harnesses can honour a
    // `#!errorlimit=N` script directive, mirroring qvh's QVH_ERROR_LIMIT —
    // legacy Quest had no breaker at all (patch_questviva.py §4).
    int max_script_errors_ = 20;
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

    // Parked synchronous `play sound` continuation (the wait slot held by
    // PlaySoundScript's awaited TCS). parked_owes_update_/parked_owes_endcb_
    // record the C++-side tail the suspended chain still owes (pane refresh /
    // EndPendingCallback finallys), replayed after the frames on resume.
    // NOT captured by save/undo snapshots: a park pends only between two
    // consecutive turns, and snapshots are taken at turn boundaries with no
    // park outstanding in practice.
    bool sound_parked_ = false;
    std::vector<TurnSuspended::Frame> parked_frames_;
    bool parked_owes_update_ = false;
    int parked_owes_endcb_ = 0;
    // A COMMAND turn (run_command, pre-v580) calls FinishTurn -- which runs the
    // turnscripts -- as a step AFTER HandleCommand returns. When HandleCommand
    // suspends on a synchronous `play sound`, that FinishTurn is never reached,
    // so the parked continuation still owes it: QuestViva's awaited command
    // chain resumes THROUGH TryFinishTurnAsync when the wait slot is claimed, so
    // the turnscripts DO tick for that turn (just deferred). Replayed after the
    // frames, before the owed pane refresh. Core's RunTurnScripts self-guards on
    // IsGameRunning(), so an owed FinishTurn on a finished game no-ops the ticks.
    void run_deferred_finish_turn();

    bool parked_owes_finishturn_ = false;

    // A `wait` registered during this command's HandleCommand, so the turn is
    // parked on the player rather than finished. Legacy Quest 5 runs the game
    // on its own thread and blocks it inside `wait`, so the stack survives and
    // FinishTurn -- every turnscript -- runs AFTER the wait callback; the async
    // model returns from `wait` immediately and would race FinishTurn ahead of
    // it, firing turnscripts in a room the callback is about to leave. Set here
    // and discharged by finish_wait (pre-v580 only, matching the version gate
    // on the FinishTurn call itself).
    bool finish_turn_deferred_ = false;
};

}  // namespace aslx

#endif  // GEAS_ASLX_RUNTIME_HH
