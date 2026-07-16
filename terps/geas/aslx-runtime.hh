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

// A .NET-flavoured regex compiled for std::regex, plus the ordered names of its
// capture groups (empty string for an unnamed group). Defined in the .cc; the
// parser primitives (IsRegexMatch/GetMatchStrength/Populate) use it. Mirrors
// QuestViva's RegexCache entry (System.Text.RegularExpressions.Regex).
struct CompiledRegex;

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

    // Run every callback queued by `on ready` since the last drain, FIFO.
    // Callbacks queued while draining run in the same pass (Quest flushes the
    // whole ready queue before returning control to the player).
    void drain_on_ready();
    bool has_pending_on_ready() const { return !on_ready_.empty(); }

    // Field access with inheritance resolution (own fields, then inherited
    // types most-recent-first). Returns nullptr if unresolved.
    const Value *resolve_field(Element *e, const std::string &name);

    World &world() { return world_; }

    // Value/runtime helpers (also used by tests).
    static std::string to_string(const Value &v);
    static bool truthy(const Value &v);

private:
    World &world_;
    std::string output_;
    Rng rng_;

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
    int script_depth_ = 0;
    int script_error_count_ = 0;
    bool script_errors_fatal_ = false;
    bool reporting_error_ = false;

    // Deferred `on ready` callbacks: a pointer to the compiled callback body
    // (owned by the never-freed script cache) plus a snapshot of the locals at
    // queue time.
    std::vector<std::pair<const std::vector<Stmt> *, Context>> on_ready_;
};

}  // namespace aslx

#endif  // GEAS_ASLX_RUNTIME_HH
