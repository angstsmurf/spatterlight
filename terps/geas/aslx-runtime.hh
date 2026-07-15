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
    Rng &rng() { return rng_; }

    // Run a raw script source string (a function body / command script) in ctx.
    void run_script(const std::string &source, Context &ctx);

    // Evaluate an expression source string in ctx.
    Value eval(const std::string &source, Context &ctx);

    // Call a game <function> by name with already-evaluated args.
    Value call_function(const std::string &name, std::vector<Value> args,
                        Context *caller);

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

    std::shared_ptr<std::vector<Stmt>> compile_script(const std::string &src);
    std::shared_ptr<Expr> compile_expr(const std::string &src);

    void exec_block(const std::vector<Stmt> &stmts, Context &ctx);
    void exec_stmt(const Stmt &s, Context &ctx);
    Value eval_expr(const Expr &e, Context &ctx);

    Value call_builtin(const std::string &name, std::vector<Value> &args,
                       bool &handled, Context &ctx);
    Value resolve_variable(const std::string &name, Context &ctx, bool &found);
};

}  // namespace aslx

#endif  // GEAS_ASLX_RUNTIME_HH
