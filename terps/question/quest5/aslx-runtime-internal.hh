/*
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// aslx-runtime-internal.hh -- what the runtime's translation units share with
// each other and with nobody else: the script AST, and the helpers one unit
// defines and another calls. The runtime is five units:
//
//   aslx-runtime.cc           the interpreter proper (Interp's core)
//   aslx-runtime-parse.cc     script source text -> Expr / Stmt trees
//   aslx-runtime-builtins.cc  Interp::call_builtin, the expression functions
//   aslx-state.cc             save/restore, the compact v1 snapshot
//   aslx-savenative.cc        save/restore, Quest's own .quest-save format
//
// Hosts (the Glk frontend, the test drivers) include aslx-runtime.hh instead.

#ifndef QUESTION_ASLX_RUNTIME_INTERNAL_HH
#define QUESTION_ASLX_RUNTIME_INTERNAL_HH

#include "aslx-runtime.hh"

#include <cctype>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace aslx {

// ===========================================================================
// Small value and string helpers
// ===========================================================================

inline bool is_number(const Value &v) {
    return v.type == Value::Type::Int || v.type == Value::Type::Double;
}
inline double as_double(const Value &v) {
    if (v.type == Value::Type::Int) return (double)v.integer;
    if (v.type == Value::Type::Double) return v.dbl;
    if (v.type == Value::Type::Boolean) return v.boolean ? 1 : 0;
    return 0;
}
inline bool is_list(const Value &v) {
    return v.type == Value::Type::StringList || v.type == Value::Type::ObjectList;
}
inline bool is_dict(const Value &v) {
    return v.type == Value::Type::StringDict ||
           v.type == Value::Type::ObjectDict ||
           v.type == Value::Type::ScriptDict;
}

// Case-insensitive ASCII compare (NCalc's true/false literals and the "null"
// parameter lookup are case-insensitive).
inline bool rt_iequals(const std::string &a, const char *b) {
    size_t n = 0;
    for (; n < a.size() && b[n]; ++n)
        if (std::tolower((unsigned char)a[n]) != std::tolower((unsigned char)b[n]))
            return false;
    return n == a.size() && b[n] == 0;
}

inline std::string rt_trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

// ===========================================================================
// Expression + statement AST (built by aslx-runtime-parse.cc)
// ===========================================================================

struct Expr {
    enum class Kind {
        Num, Str, Bool, Null, Var, Member, Index, Call, Unary, Binary, Ternary,
        List,  // "(a, b, ...)" / "()": NCalc's LogicalExpressionList, in `args`
        // An expression whose COMPILE failed, kept as a node that raises the
        // parse error (in `str`) only when EVALUATED: QuestViva's Expression<T>
        // hands the raw text to NCalc, which parses lazily at Evaluate time, so
        // an unparsable never-reached condition is harmless (Serpent's Eye has
        // a literal empty `else if ()` in the Shopkeeper's giveto script).
        ParseError
    };
    Kind kind;
    // literals
    double num = 0; bool is_int = false;
    std::string str;      // Str / Var name / Member name / Call name / op
    bool boolean = false;
    // children
    std::shared_ptr<Expr> a, b, c;
    std::vector<std::shared_ptr<Expr>> args;
    // Original source text, set on the ROOT node of each compiled expression
    // only. A non-empty src marks the QuestViva Expression<T> boundary where a
    // runtime failure is wrapped as "Error evaluating expression '<src>': ...".
    std::string src;
    // Per-expression RNG stream (lazily created at the root on first draw).
    // Through beta.57 QuestViva's NcalcExpressionEvaluator constructed its OWN
    // ExpressionOwner -- and thus its own seed-1234 ErkyrathRandom -- per
    // compiled expression (the oracle's patch_questviva.py section 16 keeps
    // that convention now upstream shares one), so every expression's
    // GetRandomInt/GetRandomDouble sequence starts fresh and advances only
    // when THAT expression evaluates. (Verified against the
    // oracle: EFMB's PickOneString stream restarts at the seed.)
    std::shared_ptr<Rng> rng;
};

using ExprP = std::shared_ptr<Expr>;

struct Stmt {
    enum class Kind {
        Msg, If, While, For, ForEach, Assign, Call, Return, Comment,
        Switch, FirstTime, OnReady, Wait, GetInput, ShowMenu, Ask,
        // A statement whose COMPILE failed: the rest of the body still loads
        // (QuestViva parses statement-by-statement, lazily), and the error --
        // kept in `name` -- surfaces only if this statement actually RUNS.
        // EFMB has a `MoveObject (coin, )` behind an always-false guard.
        ParseError
    };
    Kind kind;
    std::string name;              // Assign var/prop, For/ForEach var, Call name
    ExprP expr;                    // Msg/Return value, While/If cond, Assign value,
                                   // Switch selector
    // "x => { script }" (SetScriptScript): the RHS is a script literal, stored
    // as source text and assigned as a Script value. expr is null in that case.
    std::string script_text;
    ExprP obj;                     // Assign target object (before last dot), or null
    ExprP from, to, step;          // For
    ExprP list;                    // ForEach
    std::vector<Stmt> body;        // block (also FirstTime first / OnReady callback)
    // If: chained else-if/else
    std::vector<std::pair<ExprP, std::vector<Stmt>>> elseifs;
    std::vector<Stmt> else_body;   // also Switch `default`, FirstTime `otherwise`
    bool has_else = false;
    std::vector<ExprP> call_args;  // Call
    // Switch: each case is (one-or-more match exprs) -> body.
    std::vector<std::pair<std::vector<ExprP>, std::vector<Stmt>>> cases;
    // FirstTime: per-compiled-instance "has it run yet" flag. The compiled Stmt
    // is cached and reused across invocations, so this shared flag persists,
    // matching QuestViva's per-FirstTimeScript m_hasRun.
    std::shared_ptr<bool> ran;
};

// ===========================================================================
// aslx-runtime-parse.cc
// ===========================================================================

// The tokenizer helpers, mirroring QuestViva's Utility.cs. The native save
// writer re-splits script source with the same ones the statement parser uses,
// so its firsttime bake walks statements in the parser's order.

// Replace the contents of every double-quoted string with '-', preserving
// length, so structural scans (braces, newlines, "//") ignore string bodies.
std::string obscure_strings(const std::string &in);
// The text inside the first balanced "(...)"; `after` is what follows it.
std::string get_parameter(const std::string &script, std::string &after,
                          bool &found);
// Split a parameter list on its top-level commas (Utility.SplitParameter).
std::vector<std::string> split_parameters(const std::string &text);
// Pull the next statement off the front of `script`; `after` is the rest.
std::string get_script(const std::string &script, std::string &after);
std::string remove_surrounding_braces(const std::string &input);
// Strip // line comments, respecting string literals.
std::string remove_comments(const std::string &input);
// True if `line` starts with `kw` and the keyword ends there (no identifier
// character follows it).
bool starts_with_word(const std::string &line, const std::string &kw);
// Substring after the first occurrence of `kw` (all of `s` if there is none).
std::string text_after(const std::string &s, const std::string &kw);

// A keyword that opens a statement carrying a script block, and where that
// block sits. The parser dispatches on this, and so does the save-time
// firsttime bake (aslx-savenative.cc), which has to find every block in a
// script's source exactly where the parser did: a keyword the two told apart
// differently would hand the firsttime flags to the wrong blocks in a save.
struct BlockKeyword {
    enum class Shape {
        Bare,       // keyword { block }
        Parameter,  // keyword (parameter) { block }
        Switch,     // switch (expression) { case (values) { block } ... }
    };
    const char *word;
    Stmt::Kind kind;
    Shape shape;
};
// The block keyword `stmt` opens with, or null. (`else` and `otherwise` are
// not among them: parse_statements folds those into the statement before.)
const BlockKeyword *block_keyword(const std::string &stmt);

// Compile an expression source string to an AST; throws std::runtime_error
// ("<what> in [<src>]") if it does not parse.
ExprP compile_expr_str(const std::string &src);
// Compile, deferring a failure to evaluation time (see Expr::Kind::ParseError).
ExprP compile_expr_str_deferred(const std::string &src);
// Compile a block of script source into a statement list. `interp` only
// collects the load-time diagnostics (Interp::errors()).
std::vector<Stmt> parse_statements(const std::string &src, Interp &interp);

// ===========================================================================
// aslx-runtime.cc
// ===========================================================================

// A .NET-flavoured regex compiled for std::regex (see the rewrite_dotnet_regex
// note in aslx-runtime.cc; Interp::compiled_regex builds and caches these).
struct CompiledRegex {
    std::regex re;
    // names[k] is the name of capture group k+1 ("" for an unnamed group).
    std::vector<std::string> names;
    bool valid = false;
};
// The named groups of a match as (name, value) pairs, in first-appearance
// order, with .NET's coalescing of groups that share a name.
std::vector<std::pair<std::string, std::string>> named_groups(
        const CompiledRegex &cr, const std::smatch &m);
// Total length captured by the named groups (Utility.GetMatchStrengthInternal).
int named_group_len(const CompiledRegex &cr, const std::smatch &m);

// Quest's equality (the `=` operator; also ListContains, switch/case, ...).
bool values_equal(const Value &a, const Value &b);
// Utility.SafeXML -- also the safexml builtin.
std::string safe_xml_escape(const std::string &s);
// The element an object-reference value names, or nullptr.
Element *interp_eval_element(Interp &in, const Value &v);
// Shared body of the `rundelegate` statement and the RunDelegateFunction
// built-in.
Value interp_run_delegate(Interp &in, Element *obj, const std::string &delname,
                          const std::vector<Value> &params);

// ===========================================================================
// aslx-runtime-builtins.cc
// ===========================================================================

// The shape int.Parse / int.TryParse accept (invariant culture): surrounding
// whitespace, an optional sign, digits -- and the value must fit an Int32.
enum class IntParse { Ok, BadFormat, OutOfRange };
IntParse parse_int32_text(const std::string &raw, long &out);

// Template lookups: the text of the LAST <template> / <dynamictemplate> of
// that name, or nullptr.
const std::string *find_template(World &w, const std::string &name);
const std::string *find_dyn_template(World &w, const std::string &name);

// ===========================================================================
// aslx-state.cc
// ===========================================================================

// True for the element types a save records (the mutable object family).
bool save_family(const std::string &elem_type);
// A live element of that family: what a save writes.
inline bool is_saved_element(const Element &e) {
    return e.registered && save_family(e.elem_type);
}
// Unregister every such element not named in `keep`: a save lists all the
// surviving ones, so anything absent was destroyed before it was written.
void drop_unsaved_elements(World &w, const std::set<std::string> &keep);

// The script cache key: "<scope>\x1F<source>" for a script run from an owning
// attribute (see Interp::scope_key), the bare source text otherwise.
inline std::string script_cache_key(const std::string &scope,
                                    const std::string &src) {
    return scope.empty() ? src : scope + '\x1F' + src;
}
inline void split_script_cache_key(const std::string &key, std::string &scope,
                                   std::string &src) {
    size_t sep = key.find('\x1F');
    scope = sep == std::string::npos ? std::string() : key.substr(0, sep);
    src = sep == std::string::npos ? key : key.substr(sep + 1);
}

// The `firsttime` ran-flags of a compiled body, in preorder.
void collect_firsttime(const std::vector<Stmt> &body,
                       std::vector<std::shared_ptr<bool>> &out);
// Whether any of them is set.
bool any_firsttime_ran(const std::vector<std::shared_ptr<bool>> &flags);
// Set a compiled body's flags from saved bits, as far as both reach.
void apply_firsttime(const std::vector<Stmt> &body,
                     const std::vector<bool> &bits);
// The root expressions of a compiled body, in preorder.
void collect_expr_roots(std::vector<Stmt> &body, std::vector<ExprP> &out);

}  // namespace aslx

#endif  // QUESTION_ASLX_RUNTIME_INTERNAL_HH
