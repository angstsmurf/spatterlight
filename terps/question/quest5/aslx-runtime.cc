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

// aslx-runtime.cc -- Quest 5 script + expression evaluator. See aslx-runtime.hh.

#include "aslx-runtime-internal.hh"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace aslx {

// ===========================================================================
// Regex support for the parser primitives (IsRegexMatch/GetMatchStrength/
// Populate). Quest patterns are .NET regexes with named groups, e.g. the
// simplepattern "take #object#" compiles to "^take (?<object>.*)$". std::regex
// (ECMAScript) has no named-group syntax, so we rewrite "(?<name>" to a plain
// "(" and record the group name by capture index. Lookahead/non-capturing
// groups are passed through untouched and do not consume a capture index.
// See QuestViva Utility.IsRegexMatch/GetMatchStrength/Populate + RegexCache.cs.
// ===========================================================================

// Rewrite a .NET pattern into an ECMAScript one, filling `names`. Distinguishes
// the named-group "(?<name>" / "(?'name'" from lookbehind "(?<=" / "(?<!" and
// from non-capturing/lookahead "(?:" "(?=" "(?!". Skips escapes and character
// classes so a literal "(" inside "[...]" or after "\" is not miscounted.
static std::string rewrite_dotnet_regex(const std::string &pat,
                                        std::vector<std::string> &names) {
    std::string out;
    size_t i = 0, n = pat.size();
    bool in_class = false;
    while (i < n) {
        char c = pat[i];
        if (c == '\\') {  // escape: copy the pair verbatim
            out += c;
            if (i + 1 < n) out += pat[i + 1];
            i += 2;
            continue;
        }
        if (in_class) {
            out += c;
            if (c == ']') in_class = false;
            ++i;
            continue;
        }
        if (c == '[') { in_class = true; out += c; ++i; continue; }
        if (c == '(') {
            if (i + 1 < n && pat[i + 1] == '?') {
                // (?<name>  or  (?'name'  -> a named capture group
                bool angle = (i + 2 < n && pat[i + 2] == '<' && i + 3 < n &&
                              pat[i + 3] != '=' && pat[i + 3] != '!');
                bool quote = (i + 2 < n && pat[i + 2] == '\'');
                if (angle || quote) {
                    char close = angle ? '>' : '\'';
                    size_t j = i + 3;
                    std::string name;
                    while (j < n && pat[j] != close) name += pat[j++];
                    names.push_back(name);
                    out += '(';
                    i = (j < n) ? j + 1 : j;  // skip the closing '>' / '\''
                    continue;
                }
                // (?: (?= (?! (?<= (?<! : pass through, no capture index
                out += c;
                ++i;
                continue;
            }
            names.push_back("");  // plain capturing group
            out += c;
            ++i;
            continue;
        }
        out += c;
        ++i;
    }
    return out;
}

static std::shared_ptr<CompiledRegex> build_regex(const std::string &pattern,
                                                  std::vector<std::string> &errors) {
    auto cr = std::make_shared<CompiledRegex>();
    std::string rewritten = rewrite_dotnet_regex(pattern, cr->names);
    try {
        cr->re.assign(rewritten, std::regex::ECMAScript | std::regex::icase);
        cr->valid = true;
    } catch (const std::regex_error &e) {
        errors.push_back("Invalid regex '" + pattern + "': " + e.what());
    }
    return cr;
}

// Resolve the named groups to (name, value) pairs in first-appearance order,
// with .NET's semantics for repeated names: two groups sharing a name (as in a
// "take (?<object>.*)|get (?<object>.*)" alternation) are one logical group
// whose value is the last successful capture. std::regex keeps them as distinct
// numbered groups, so we coalesce here.
std::vector<std::pair<std::string, std::string>> named_groups(
        const CompiledRegex &cr, const std::smatch &m) {
    std::vector<std::pair<std::string, std::string>> out;
    for (size_t k = 0; k < cr.names.size(); ++k) {
        const std::string &name = cr.names[k];
        if (name.empty()) continue;
        auto it = std::find_if(out.begin(), out.end(),
                               [&](const auto &p) { return p.first == name; });
        // Only overwrite a previously-stored value when this group matched
        // (last successful capture wins; an unmatched later group is ignored).
        if (it == out.end()) out.emplace_back(name, m[k + 1].str());
        else if (m[k + 1].matched) it->second = m[k + 1].str();
    }
    return out;
}

// Sum the lengths captured by the (coalesced) named groups, matching
// Utility.GetMatchStrengthInternal (unnamed/numbered groups are excluded).
int named_group_len(const CompiledRegex &cr, const std::smatch &m) {
    int total = 0;
    for (const auto &g : named_groups(cr, m)) total += (int)g.second.size();
    return total;
}

// ===========================================================================
// Deterministic RNG (xoshiro128** + SplitMix32), matching erkyrath_random().
// ===========================================================================

void Rng::seed(uint32_t seed) {
    for (int i = 0; i < 4; i++) {
        seed += 0x9E3779B9u;
        uint32_t z = seed;
        z ^= z >> 15; z *= 0x85EBCA6Bu;
        z ^= z >> 13; z *= 0xC2B2AE35u;
        z ^= z >> 16;
        s[i] = z;
    }
}

uint32_t Rng::next() {
    const uint32_t t1x5 = s[1] * 5u;
    const uint32_t result = ((t1x5 << 7) | (t1x5 >> (32 - 7))) * 9u;
    const uint32_t t1s9 = s[1] << 9;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t1s9;
    const uint32_t t3 = s[3];
    s[3] = (t3 << 11) | (t3 >> (32 - 11));
    return result;
}

// ASLX_TRACE_RAND=1: log every draw to stderr for stream-parity debugging
// against the oracle's QVH_TRACE_RAND (same numbering, same format).
static bool rng_trace_enabled() {
    static int on = -1;
    if (on < 0) {
        const char *env = std::getenv("ASLX_TRACE_RAND");
        on = (env && *env && *env != '0') ? 1 : 0;
    }
    return on == 1;
}
static long rng_trace_seq = 0;

long Rng::between(long lo, long hi) {
    if (hi <= lo) return lo;
    // Unsigned throughout: `hi - lo + 1` as signed overflows (UB) for extreme
    // spans reachable through wrapped script arithmetic, and the full-range
    // span wraps to 0 (`next() % 0` is a SIGFPE).
    unsigned long span = (unsigned long)hi - (unsigned long)lo + 1;
    long r = span == 0 ? (long)next()
                       : (long)((unsigned long)lo + next() % span);
    if (rng_trace_enabled())
        fprintf(stderr, "[rand %ld] between(%ld,%ld)=%ld\n",
                ++rng_trace_seq, lo, hi, r);
    return r;
}

double Rng::next_double() {
    double r = next() * (1.0 / 4294967296.0);
    if (rng_trace_enabled())
        fprintf(stderr, "[rand %ld] double=%.17g\n", ++rng_trace_seq, r);
    return r;
}

// ===========================================================================
// Value coercions
// ===========================================================================

static std::string fmt_double(double d) {
    if (std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 1e15) {
        // .NET prints an integral double without a decimal point.
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", (long long)d);
        return buf;
    }
    std::ostringstream o;
    o << d;
    return o.str();
}

std::string Interp::to_string(const Value &v) {
    switch (v.type) {
    case Value::Type::Null: return "";
    case Value::Type::String:
    case Value::Type::Script:
    case Value::Type::ObjectRef: return v.str;
    case Value::Type::Int: return std::to_string(v.integer);
    case Value::Type::Double: return fmt_double(v.dbl);
    case Value::Type::Boolean: return v.boolean ? "True" : "False";
    case Value::Type::StringList:
    case Value::Type::ObjectList: {
        std::string s;
        for (size_t i = 0; i < v.list().size(); ++i) {
            if (i) s += ", ";
            s += to_string(v.list()[i]);
        }
        return s;
    }
    default: return "";
    }
}

bool Interp::truthy(const Value &v) {
    if (v.type == Value::Type::Boolean) return v.boolean;
    if (v.type == Value::Type::Null) return false;
    if (is_number(v)) return as_double(v) != 0;
    return true;
}

// A wrapped expression-evaluation failure (NcalcExpressionEvaluator's catch).
// `original` keeps the innermost message so a nested failure (Eval()) re-wraps
// with the OUTER source but the ORIGINAL message, exactly like QuestViva's
// `ex.InnerException?.Message ?? ex.Message`.
struct EvalError : std::runtime_error {
    std::string original;
    EvalError(const std::string &src, const std::string &orig)
        : std::runtime_error("Error evaluating expression '" + src + "': " + orig),
          original(orig) {}
};

// ===========================================================================
// Interp
// ===========================================================================

// Seed for every RNG stream: 1234, or ASLX_SEED (the oracle's QVH_SEED analog).
static uint32_t aslx_default_seed() {
    const char *env = std::getenv("ASLX_SEED");
    return env ? (uint32_t)std::strtoul(env, nullptr, 10) : 1234u;
}

Interp::Interp(World &world) : world_(world) {
    rng_.seed(aslx_default_seed());
    print = [this](const std::string &s) { output_ += s; };
}
Interp::~Interp() = default;

std::shared_ptr<CompiledRegex> Interp::compiled_regex(const std::string &pattern,
                                                      const std::string *cache_id) {
    if (cache_id) {
        auto it = regex_cache_.find(*cache_id);
        if (it != regex_cache_.end()) return it->second;  // hit: pattern ignored
        auto cr = build_regex(pattern, world_.errors);
        regex_cache_[*cache_id] = cr;
        return cr;
    }
    return build_regex(pattern, world_.errors);  // no cacheID: compile fresh
}

std::shared_ptr<Expr> Interp::compile_expr(const std::string &src) {
    auto it = expr_cache_.find(src);
    if (it != expr_cache_.end()) return it->second;
    ExprP e = compile_expr_str(src);
    expr_cache_[src] = e;
    return e;
}

void Interp::capture_rng_streams(
    std::vector<std::pair<std::string, std::array<uint32_t, 4>>> &out)
{
    auto state = [](const Rng &r) {
        return std::array<uint32_t, 4>{r.s[0], r.s[1], r.s[2], r.s[3]};
    };
    out.clear();
    out.emplace_back(std::string(), state(rng_));
    for (const auto &kv : expr_cache_) {
        const ExprP &e = kv.second;
        if (e && e->rng) out.emplace_back(kv.first, state(*e->rng));
    }
    // Expressions embedded in compiled script bodies (msg (...), the RHS of an
    // assignment, an if condition, ...) are compiled by the statement parser,
    // not through expr_cache_, so each is a separate stream even when its text
    // matches a cached one.  Key: "\x01<script key>\x1D<ordinal>".
    for (auto &kv : script_cache_) {
        if (!kv.second) continue;
        std::vector<ExprP> roots;
        collect_expr_roots(*kv.second, roots);
        for (size_t i = 0; i < roots.size(); i++) {
            const ExprP &e = roots[i];
            if (!e->rng) continue;
            out.emplace_back('\x01' + kv.first + '\x1D' + std::to_string(i),
                             state(*e->rng));
        }
    }
}

void Interp::restore_rng_streams(
    const std::vector<std::pair<std::string, std::array<uint32_t, 4>>> &in)
{
    for (const auto &entry : in) {
        if (entry.first.empty()) {
            for (int i = 0; i < 4; i++)
                rng_.s[i] = entry.second[i];
            continue;
        }
        ExprP e;
        if (entry.first[0] == '\x01') {
            // A script-body root: recompile the script (same cache key the
            // capture walked) and pick the root by ordinal.
            size_t sep = entry.first.rfind('\x1D');
            if (sep == std::string::npos) continue;
            std::string key = entry.first.substr(1, sep - 1);
            size_t ordinal = (size_t) std::strtoul(entry.first.c_str() + sep + 1,
                                                   nullptr, 10);
            std::string scope, src;
            split_script_cache_key(key, scope, src);
            std::shared_ptr<std::vector<Stmt>> body;
            try {
                body = compile_script(src, scope);
            } catch (const std::exception &) {
                continue;
            }
            if (!body) continue;
            std::vector<ExprP> roots;
            collect_expr_roots(*body, roots);
            if (ordinal >= roots.size()) continue;
            e = roots[ordinal];
        } else {
            try {
                e = compile_expr(entry.first);
            } catch (const std::runtime_error &) {
                continue;  // captured from a source that no longer compiles
            }
        }
        if (!e)
            continue;
        if (!e->rng)
            e->rng = std::make_shared<Rng>();
        for (int i = 0; i < 4; i++)
            e->rng->s[i] = entry.second[i];
    }
}

Value Interp::eval(const std::string &source, Context &ctx) {
    std::string s = rt_trim(source);
    if (s.empty()) return vnull();
    // Failures THROW (wrapped by the root-expression boundary in eval_expr),
    // unwinding to the nearest script boundary, which logs and reports --
    // QuestViva semantics. The Eval() builtin relies on the throw so the
    // outer expression re-wraps with its own source text.
    ExprP e = compile_expr(s);
    return eval_expr(*e, ctx);
}

std::shared_ptr<std::vector<Stmt>> Interp::compile_script(const std::string &src,
                                                          const std::string &scope) {
    // QuestViva compiles each script ATTRIBUTE into its own IScript tree, so
    // two attributes with identical text hold two FirstTimeScript instances
    // (each with its own m_hasRun). Victorian Detective's 24 gamebook pages
    // all carry `firsttime { IncreaseCounter ("DR") }`; sharing one compiled
    // body by source text ran the block once for the whole game. Key by the
    // owning attribute when the caller knows it.
    std::string key = script_cache_key(scope, src);
    auto it = script_cache_.find(key);
    if (it != script_cache_.end()) return it->second;
    auto v = std::make_shared<std::vector<Stmt>>(parse_statements(src, *this));
    if (!scope.empty()) {
        // A save written before scoping recorded the flags by source alone;
        // seed this instance from it so restored one-time text stays spent.
        auto lg = legacy_firsttime_.find(src);
        if (lg != legacy_firsttime_.end()) apply_firsttime(*v, lg->second);
    }
    script_cache_[key] = v;
    return v;
}

// RunScriptAsync's MaxScriptExecutionDepth message.
static const char kDepthExceeded[] =
    "Script execution depth exceeded 200 - this usually means a script is "
    "recursing infinitely (e.g. a \"changed<field>\" script that sets the "
    "field it's watching)";

// The script boundary (QuestViva RunScriptAsync): a parse or runtime throw
// aborts THIS script body only; it is logged and reported, and the calling
// script carries on with its next statement. The cap throw fires BEFORE the
// guarded region, so it propagates to the ENCLOSING boundary, exactly as
// RunScriptAsync's entry check does.
template <typename Body>
void Interp::script_boundary(Context &ctx, Body body) {
    if (script_depth_ >= kMaxScriptDepth)
        throw std::runtime_error(kDepthExceeded);
    ++script_depth_;
    try {
        body();
    } catch (TurnSuspended &ts) {
        // Not an error: a synchronous `play sound` parked the turn. The
        // suspension passes through every script boundary to the turn
        // boundary (QuestViva's await chain suspends RunScriptAsync itself).
        // This IS a script boundary, so claim the frames unwound so far with
        // a snapshot of this script's context; resume_parked_tail re-runs
        // them inside an equivalent boundary.
        ts.frames.push_back(TurnSuspended::Frame{nullptr, 0, ctx});
        --script_depth_;
        throw;
    } catch (const std::exception &err) {
        report_script_error(err.what());
    }
    --script_depth_;
}

void Interp::run_script(const std::string &source, Context &ctx,
                        const std::string &scope) {
    if (script_errors_fatal_) return;
    script_boundary(ctx, [&] {
        auto stmts = compile_script(source, scope);
        exec_block(*stmts, ctx);
    });
}

// -- execution --------------------------------------------------------------

void Interp::exec_block(const std::vector<Stmt> &stmts, Context &ctx) {
    exec_block_from(stmts, 0, ctx);
}

void Interp::exec_block_from(const std::vector<Stmt> &stmts, size_t start,
                             Context &ctx) {
    for (size_t i = start; i < stmts.size(); ++i) {
        try {
            exec_stmt(stmts[i], ctx);
        } catch (TurnSuspended &ts) {
            // Park capture: the not-yet-run remainder of this block joins the
            // suspended continuation, innermost block first. The enclosing
            // script boundary claims these frames with a context snapshot.
            // (Known gap: a while/for/foreach loop's remaining ITERATIONS are
            // C++ loop state and are not captured -- a sync sound inside a
            // loop body resumes the rest of that body only.)
            ts.frames.push_back(TurnSuspended::Frame{&stmts, i + 1, Context{}});
            throw;
        }
        if (ctx.returned) return;
    }
}

void Interp::exec_stmt(const Stmt &s, Context &ctx) {
    switch (s.kind) {
    case Stmt::Kind::Comment: return;
    case Stmt::Kind::Msg: {
        Value v = eval_expr(*s.expr, ctx);
        print_via_core(to_string(v), ctx);
        return;
    }
    case Stmt::Kind::Return: {
        ctx.return_value = s.expr ? eval_expr(*s.expr, ctx) : vnull();
        ctx.returned = true;
        return;
    }
    case Stmt::Kind::If: {
        if (truthy(eval_expr(*s.expr, ctx))) { exec_block(s.body, ctx); return; }
        for (const auto &ei : s.elseifs)
            if (truthy(eval_expr(*ei.first, ctx))) { exec_block(ei.second, ctx); return; }
        if (s.has_else) exec_block(s.else_body, ctx);
        return;
    }
    case Stmt::Kind::While: {
        while (truthy(eval_expr(*s.expr, ctx))) {
            exec_block(s.body, ctx);
            if (ctx.returned) break;
        }
        return;
    }
    case Stmt::Kind::For: {
        long from = (long)std::llround(as_double(eval_expr(*s.from, ctx)));
        long to = (long)std::llround(as_double(eval_expr(*s.to, ctx)));
        long step = s.step ? (long)std::llround(as_double(eval_expr(*s.step, ctx))) : 1;
        ctx.locals[s.name] = vint(from);
        for (long count = from; (step > 0 && count <= to) || (step < 0 && count >= to);
             count += step) {
            ctx.locals[s.name] = vint(count);
            exec_block(s.body, ctx);
            if (ctx.returned) break;
            const Value &nv = ctx.locals[s.name];
            if (nv.type == Value::Type::Int) count = nv.integer;
            else break;
        }
        return;
    }
    case Stmt::Kind::ForEach: {
        Value lst = eval_expr(*s.list, ctx);
        // Iterate a snapshot: list entries are typed Values and bind verbatim;
        // dictionary iteration binds the KEYS (object refs for an ObjectDict).
        std::vector<Value> items;
        if (is_list(lst)) items = lst.list();
        else if (is_dict(lst)) {
            bool obj = (lst.type == Value::Type::ObjectDict);
            for (auto &kv : lst.dict())
                items.push_back(obj ? vobj(kv.first) : vstr(kv.first));
        } else {
            error("Cannot foreach over non-list");
            return;
        }
        for (const Value &item : items) {
            ctx.locals[s.name] = item;
            exec_block(s.body, ctx);
            if (ctx.returned) break;
        }
        return;
    }
    case Stmt::Kind::Assign: {
        // "x => { ... }" (no expression) assigns the script literal itself.
        Value val = s.expr ? eval_expr(*s.expr, ctx) : vscript(s.script_text);
        if (!s.obj) {
            ctx.locals[s.name] = val;
            return;
        }
        Element *e = interp_eval_element(*this, eval_expr(*s.obj, ctx));
        if (!e)
            error("Assignment to attribute '" + s.name + "' of a non-object");
        assign_field(e, s.name, std::move(val));
        return;
    }
    case Stmt::Kind::Switch: {
        std::string sel = to_string(eval_expr(*s.expr, ctx));
        for (const auto &c : s.cases) {
            for (const auto &m : c.first) {
                if (to_string(eval_expr(*m, ctx)) == sel) {
                    exec_block(c.second, ctx);
                    return;
                }
            }
        }
        if (!s.else_body.empty()) exec_block(s.else_body, ctx);
        return;
    }
    case Stmt::Kind::FirstTime: {
        if (s.ran && !*s.ran) {
            *s.ran = true;
            // FirstTimeScript logs UndoFirstTime, so undoing the turn lets
            // the firsttime text fire again.
            if (undo_logging_) {
                UndoAction a;
                a.kind = UndoAction::Kind::FirstTime;
                a.ran = s.ran;
                add_undo(std::move(a));
            }
            exec_block(s.body, ctx);
        } else if (s.has_else) {
            exec_block(s.else_body, ctx);
        }
        return;
    }
    case Stmt::Kind::OnReady: {
        add_on_ready(&s.body, ctx);
        return;
    }
    case Stmt::Kind::ParseError:
        error(s.name);
        return;
    // The four prompt commands are fire-and-forget (QuestViva's ExecuteAsync
    // starts AwaitResponseAndRunCallbackAsync and returns): the callback is
    // registered, execution of the enclosing script continues, and the host
    // resolves the prompt later (send_command / set_menu_response /
    // set_question_response / finish_wait). Registering a new prompt of a kind
    // that already pends cancels the old one (BeginPrompt's TrySetCanceled).
    case Stmt::Kind::Wait: {
        // WaitScript.ExecuteAsync: DoWait, then BeginPrompt(ref _waitTcs) --
        // which first cancels whatever holds the slot. A parked synchronous
        // `play sound` resumes inline HERE, before this wait registers.
        resume_parked_tail();
        cancel_prompt(wait_pending_, wait_cb_);
        begin_prompt(wait_pending_, wait_cb_, s.body, ctx);
        return;
    }
    case Stmt::Kind::GetInput: {
        cancel_prompt(command_override_, command_cb_);
        begin_prompt(command_override_, command_cb_, s.body, ctx);
        return;
    }
    case Stmt::Kind::Ask: {
        // PlayerUI.ShowQuestion: the caption is handed to the host, not
        // printed -- rendering the yes/no prompt is presentation. (A v600+
        // game draws it inline; see show_inline_prompt.)
        std::string caption = to_string(eval_expr(*s.expr, ctx));
        cancel_prompt(question_pending_, question_cb_);
        if (inline_prompts()) show_inline_question(caption, ctx);
        question_ = caption;
        begin_prompt(question_pending_, question_cb_, s.body, ctx);
        return;
    }
    case Stmt::Kind::ShowMenu: {
        std::string caption = to_string(eval_expr(*s.call_args[0], ctx));
        Value options = eval_expr(*s.call_args[1], ctx);
        bool allow_cancel = truthy(eval_expr(*s.call_args[2], ctx));
        MenuData md;
        md.caption = caption;
        md.allow_cancel = allow_cancel;
        // ShowMenuScript: a stringlist's entries are their own keys; a
        // stringdictionary supplies key -> display text.
        if (options.type == Value::Type::StringList) {
            for (const Value &e : options.list()) {
                std::string s2 = to_string(e);
                md.options.emplace_back(s2, s2);
            }
        } else if (options.type == Value::Type::StringDict) {
            for (const auto &kv : options.dict())
                md.options.emplace_back(kv.first, to_string(kv.second));
        } else {
            error("Unknown menu options type");
        }
        if (md.options.empty()) error("No menu options specified");
        // The caption is printed (PrintAsync) before the menu goes to the UI;
        // a v600+ game draws the whole menu inline instead.
        if (inline_prompts()) {
            std::vector<std::string> texts;
            for (const auto &kv : md.options) texts.push_back(kv.second);
            show_inline_prompt(&caption, texts, ctx);
        } else {
            print_via_core(caption, ctx);
        }
        cancel_prompt(menu_pending_, menu_cb_);
        menu_ = std::move(md);
        begin_prompt(menu_pending_, menu_cb_, s.body, ctx);
        return;
    }
    case Stmt::Kind::Call: {
        // Reserved statement commands (do/invoke/create/set/list add/...) take
        // precedence, then game functions, then built-ins.
        if (exec_statement_command(s.name, s.call_args, ctx)) return;
        std::vector<Value> args;
        for (const auto &a : s.call_args) args.push_back(eval_expr(*a, ctx));
        // Trailing "{ script }" block: one extra script-literal argument.
        if (!s.script_text.empty()) args.push_back(vscript(s.script_text));
        if (world_.find_function(s.name)) {
            call_function(s.name, std::move(args), &ctx);
        } else {
            bool handled = false;
            call_builtin(s.name, args, handled, ctx);
            if (!handled)
                error("Function not found: '" + s.name + "'");
        }
        return;
    }
    }
}

void Interp::error(const std::string &message) {
    // No "(in function)" suffix: QuestViva prints exception messages verbatim
    // ("Error running script: " + ex.Message), and the golden transcripts
    // contain the bare form. frames_ stays for diagnostics elsewhere.
    throw std::runtime_error(message);
}

void Interp::report_script_error(const std::string &what) {
    // The LOG copy carries the innermost executing function for diagnostics;
    // the player-facing print below stays bare (QuestViva prints ex.Message).
    log_exception(what);
    std::string msg = "Error running script: " + what;
    if (++script_error_count_ >= max_script_errors_) {
        // Every script is failing the same way; the session is wedged. Stop
        // running scripts and end the game (WorldModel's scriptErrorsFatal).
        script_errors_fatal_ = true;
        world_.finished = true;
    } else if (script_error) {
        // A host that surfaces errors out-of-band (the reference web player's
        // JavaScript console) takes them instead of the transcript. Nothing is
        // printed, so Core's OutputText never sees them -- see the hook's note.
        script_error(msg);
    } else if (!reporting_error_) {
        // The message also goes to the player, once (no recursive reports if
        // printing itself errors), through the same channel as any other text:
        // PrintAsync(SafeXML(message)) -- so on v540+ it passes through Core's
        // OutputText like the oracle's transcripts.
        reporting_error_ = true;
        Context ctx;
        print_via_core(safe_xml_escape(msg), ctx);
        reporting_error_ = false;
    }
}

void Interp::log_exception(const std::string &what) {
    // See the header note: the LogException-only boundary. The log half of
    // report_script_error: no print, no breaker feed.
    std::string msg = "Error running script: " + what;
    if (!frames_.empty()) msg += " [in " + frames_.back() + "]";
    errors().push_back(std::move(msg));
}

void Interp::warn_once(const std::string &key, const std::string &message) {
    for (const auto &k : warned_)
        if (k == key) return;
    warned_.push_back(key);
    world_.warnings.push_back(message);
}

void Interp::run_callback_boundary(const std::vector<Stmt> *body, Context &ctx) {
    if (!body || body->empty() || script_errors_fatal_) return;
    // Each callback is its own script boundary (RunScriptAsync) with FULL
    // depth accounting: it occupies a stack frame (AddOnReady and the on-ready
    // flush loop both run callbacks through RunScriptAsync), so nested
    // `on ready` chains contribute to the 200 cap exactly like the oracle --
    // which decides which sub-calls die during an error-cascade unwind (The
    // Bony King's beforeenter spiral). A failure inside is reported and the
    // engine carries on; a TurnSuspended (synchronous `play sound`) abandons
    // the callback silently.
    script_boundary(ctx, [&] { exec_block(*body, ctx); });
}

void Interp::begin_dormant_suspension() {
    // WorldModel.BeginDormantSuspension: pending + awaiting.
    begin_pending_callback();
    ++awaiting_resolution_count_;
}

void Interp::signal_callback_resolving() {
    // WorldModel.SignalCallbackResolving: the suspension is no longer dormant;
    // its callback is about to run (or was cancelled without running).
    if (awaiting_resolution_count_ > 0) --awaiting_resolution_count_;
}

void Interp::cancel_dormant_suspension() {
    // Drop a still-dormant prompt without running its callback (BeginPrompt
    // TrySetCanceled / play-sound claiming the wait slot).
    signal_callback_resolving();
    end_pending_callback();
}

void Interp::cancel_prompt(bool &pending, PendingCallback &cb) {
    if (!pending) return;
    // Clear the slot BEFORE end_pending_callback, whose on-ready flush may
    // itself reach a prompt statement -- with the flag still set that nested
    // statement would end the SAME callback again, driving the pending count
    // negative (permanent wedge).
    pending = false;
    cb = PendingCallback{};
    // (AwaitResponseAndRunCallbackAsync's finally still runs on a cancelled
    // prompt, so end_pending_callback discharges any FinishTurn deferred past
    // it rather than stranding it.)
    cancel_dormant_suspension();
}

void Interp::begin_prompt(bool &pending, PendingCallback &cb,
                          const std::vector<Stmt> &body, const Context &ctx) {
    pending = true;
    cb.body = &body;
    cb.ctx = ctx;
    cb.ctx.returned = false;
    begin_dormant_suspension();
}

PendingCallback Interp::take_prompt(bool &pending, PendingCallback &cb) {
    pending = false;
    PendingCallback taken = std::move(cb);
    cb = PendingCallback{};
    return taken;
}

void Interp::claim_wait_slot() {
    resume_parked_tail();
    cancel_prompt(wait_pending_, wait_cb_);
}

void Interp::resolve_prompt(PendingCallback &cb, bool inline_prompt) {
    // SignalCallbackResolving before the body (#2177): AddOnReady during the
    // callback runs now -- chained MoveObjects run each room's cascade before
    // the next move -- not deferred behind later statements.
    signal_callback_resolving();
    try {
        run_callback_boundary(cb.body, cb.ctx);
    } catch (TurnSuspended &ts) {
        // The callback parked on a sync sound; its finally (EndPendingCallback
        // -- which discharges any FinishTurn the command deferred past this
        // prompt) and the pane refresh are owed by the parked continuation.
        park_suspension(ts, /*owes_update=*/true, /*owes_endcb=*/1);
        return;
    }
    // WorldModel.FinishWait / SetMenuResponse / SetQuestionResponse: the turn
    // boundary -- end_pending_callback runs the FinishTurn the command
    // deferred past this prompt (turnscripts tick against the room the
    // callback left the player in, not the one it left) -- then refresh the
    // panes once the callback chain resolved. A park inside either defers the
    // rest to the resume.
    try {
        if (inline_prompt) end_inline_prompt(cb.ctx);
        end_pending_callback();
        if (!world_.finished) update_lists();
    } catch (TurnSuspended &ts) {
        park_suspension(ts, /*owes_update=*/false, /*owes_endcb=*/0);
    }
}

void Interp::add_on_ready(const std::vector<Stmt> *body, const Context &ctx) {
    // WorldModel.AddOnReady (#2177 / #2182): dormancy wins over nesting
    // (Quest 5 AnyOutstanding). Nested trampoline is Viva-only (#1779).
    if (awaiting_resolution_count_ > 0) {
        deferred_on_ready_.emplace_back(body, ctx);
        return;
    }
    if (is_running_on_ready_) {
        nested_on_ready_.emplace_back(body, ctx);
        return;
    }
    is_running_on_ready_ = true;
    begin_pending_callback();
    Context c = ctx;
    try {
        try {
            run_callback_boundary(body, c);
        } catch (...) {
            // Still drain whatever nested items were queued before rethrowing
            // (OnEnterRoom cascade must not strand arrival text).
            try {
                run_nested_on_ready_queue();
            } catch (...) {
            }
            throw;
        }
        run_nested_on_ready_queue();
    } catch (...) {
        is_running_on_ready_ = false;
        end_pending_callback();
        throw;
    }
    is_running_on_ready_ = false;
    end_pending_callback();
}

void Interp::run_nested_on_ready_queue() {
    // WorldModel.RunNestedOnReadyQueueAsync.
    while (!nested_on_ready_.empty() && !world_.finished) {
        if (awaiting_resolution_count_ > 0) {
            // A nested item opened a new dormant suspension -- hand the rest
            // to the turn-boundary queue rather than running ahead or stranding.
            deferred_on_ready_.insert(deferred_on_ready_.end(),
                                      nested_on_ready_.begin(),
                                      nested_on_ready_.end());
            nested_on_ready_.clear();
            return;
        }
        auto item = nested_on_ready_.front();
        nested_on_ready_.erase(nested_on_ready_.begin());
        Context ctx = item.second;
        run_callback_boundary(item.first, ctx);
    }
}

void Interp::end_pending_callback() {
    --pending_callback_count_;
    // WorldModel.EndPendingCallbackAsync: only at the true turn boundary --
    // pending back at zero -- flush the deferred on-ready queue, then run any
    // FinishTurn a command/event deferred past the suspension that just
    // resolved (or was cancelled: BeginPrompt's cancel reaches the same
    // finally, so a deferred FinishTurn is never stranded).
    if (pending_callback_count_ == 0) {
        flush_deferred_on_ready_queue();
        run_deferred_finish_turn();
    }
}

void Interp::flush_deferred_on_ready_queue() {
    // WorldModel.FlushDeferredOnReadyQueueAsync.
    if (is_flushing_deferred_on_ready_) return;
    is_flushing_deferred_on_ready_ = true;
    try {
        while (!deferred_on_ready_.empty() && awaiting_resolution_count_ == 0 &&
               !world_.finished) {
            auto item = deferred_on_ready_.front();
            deferred_on_ready_.erase(deferred_on_ready_.begin());
            Context ctx = item.second;
            run_callback_boundary(item.first, ctx);
        }
    } catch (...) {
        is_flushing_deferred_on_ready_ = false;
        throw;
    }
    is_flushing_deferred_on_ready_ = false;
}

void Interp::drain_on_ready() {
    // Manual flush for hosts/tests. Anything still deferred while no prompt
    // pends runs now; with a prompt outstanding this is a no-op -- resolving
    // the prompt flushes the queue itself.
    //
    // A FINISHED game drops its queue instead of flushing it, matching
    // EndPendingCallbackAsync's own `!world_.finished` guard above. Quest's
    // OnEnterRoom does its room description from inside nested `on ready`
    // blocks, so a MoveObject that runs after `finish` (Defeating The Monster
    // kills you with the same blow that wins the game) must NOT go on to
    // print the new room -- QuestViva abandons those callbacks.
    if (pending_callback_count_ > 0 || world_.finished) {
        if (world_.finished) {
            deferred_on_ready_.clear();
            nested_on_ready_.clear();
        }
        return;
    }
    flush_deferred_on_ready_queue();
    // Nested items should not linger with nothing running; drain defensively.
    while (!nested_on_ready_.empty() && !world_.finished) {
        auto item = nested_on_ready_.front();
        nested_on_ready_.erase(nested_on_ready_.begin());
        Context ctx = item.second;
        run_callback_boundary(item.first, ctx);
        if (script_errors_fatal_) break;
    }
}

void Interp::assign_field(Element *e, const std::string &attr, Value val) {
    const Value *prev = resolve_field(e, attr);
    Value old = prev ? *prev : vnull();
    // Fields.Set CLONES a list/dictionary on any assignment that changes the
    // OWN attribute (QuestList.RequiresCloning is unconditionally true; the
    // changed-check consults own attributes only, so localising an inherited
    // list -- "newPOV.pov_alt = newPOV.pov_alt" -- copies the type's backing
    // instead of aliasing it. Basilica's possession mechanic relies on every
    // body getting its own alt list).
    const Value *own = e->field(attr);
    // Fields.Set's `changed` -- computed against the OWN attribute (absent own
    // => changed unless writing null), and before any clone, exactly as v5
    // orders it.
    bool changed = own ? !values_equal(*own, val)
                       : val.type != Value::Type::Null;
    bool same_backing = own && own->list_store == val.list_store &&
                        own->dict_store == val.dict_store;
    if (!same_backing) val.detach();
    // v530+: assigning null REMOVES the own attribute (Fields.Set), so
    // HasAttribute goes false and Core re-init paths
    // ("player.grid_coordinates = null" then Grid_Redraw) work. Older games
    // store the null.
    bool removing = world_.asl_version >= 530 && val.type == Value::Type::Null;
    log_field_set(e, attr, val, removing);
    if (removing) e->remove_field(attr);
    else e->set_field(attr, val);
    // EVERY parent write moves the element to the end of its parent's
    // children -- even a same-value one. QuestViva's Fields.Set calls
    // SetParentFromFields unconditionally, which removes + re-appends in the
    // children index (the `changed` check only guards the saver-facing
    // SortIndex metafield). WearGarment's redundant `object.parent = game.pov`
    // really does reorder the inventory.
    if (attr == "parent") {
        log_sort_index(e);
        e->sort_index = world_.next_sort_index++;
        world_.note_containment_change();
    }
    fire_changed_script(e, attr, old, changed);
}

void Interp::fire_changed_script(Element *e, const std::string &attr,
                                 const Value &oldval, bool changed) {
    // Quest 5 (branch v5) fires changed<attr> from Fields_AttributeChanged
    // (Element.cs), and Fields.Set raises that event ONLY when the value
    // actually changed:
    //     changed = value == null ? oldValue != null : !value.Equals(oldValue)
    //     if (changed && AttributeChanged != null) AttributeChanged(...)
    // The modern rewrite lost that guard -- QuestViva's Element.SetFieldAsync
    // fires unconditionally -- which turns any "enter script re-moves the
    // player into the room it just entered" idiom into infinite recursion.
    // L Too's Pool (enter -> enter_pool -> MoveObject(player, Pool)) wedges
    // QuestViva outright; on v5 it terminates after two levels. We follow v5:
    // a same-value write fires nothing.
    if (!changed) return;
    const Value *scr = resolve_field(e, "changed" + attr);
    if (!scr || scr->type != Value::Type::Script) return;
    Context local;
    local.locals["oldvalue"] = oldval;
    local.locals["this"] = vobj(e->name);
    run_script(scr->str, local, field_scope(e, "changed" + attr));
}

void Interp::show_inline_prompt(const std::string *caption,
                                const std::vector<std::string> &option_texts,
                                Context &ctx) {
    if (caption) print_via_core(*caption, ctx);
    auto is_fn = [&](const char *name) {
        return world_.find_function(name) != nullptr;
    };
    std::string section;
    bool have_section = false;
    if (is_fn("StartNewOutputSection")) {
        Value v = call_function("StartNewOutputSection", {}, &ctx);
        if (v.type == Value::Type::String) {
            section = v.str;
            have_section = true;
        }
    }
    // The option text is the {command:} directive's own last segment, so the
    // text processor renders it as a link sending the option's number.
    for (size_t i = 0; i < option_texts.size(); i++) {
        std::string n = std::to_string(i + 1);
        print_via_core(n + ": {command:" + n + ":" + option_texts[i] + "}", ctx);
    }
    if (have_section && is_fn("EndOutputSection"))
        call_function("EndOutputSection", {vstr(section)}, &ctx);
    inline_prompt_active_ = true;
    inline_prompt_section_ = have_section ? section : std::string();
}

void Interp::show_inline_question(const std::string &caption, Context &ctx) {
    // ShowInlineQuestionAsync: the [Yes]/[No] templates, resolved here because
    // runtime-built text gets no load-time template substitution.
    auto tmpl = [&](const char *name) {
        const std::string *t = find_template(world_, name);
        return t ? *t : std::string(name);
    };
    show_inline_prompt(&caption, {tmpl("Yes"), tmpl("No")}, ctx);
}

void Interp::end_inline_prompt(Context &ctx) {
    if (!inline_prompt_active_) return;
    inline_prompt_active_ = false;
    std::string section = std::move(inline_prompt_section_);
    inline_prompt_section_.clear();
    if (!section.empty() && !world_.finished &&
        world_.find_function("HideOutputSection"))
        call_function("HideOutputSection", {vstr(section)}, &ctx);
}

void Interp::print_via_core(const std::string &text, Context &ctx) {
    // WorldModel.PrintAsync: v540+ routes through Core's OutputText so the
    // {...} text processor runs; failures inside its body report at the
    // script boundary as usual, while bypass throws (depth cap) are logged
    // only. Without Core (unit tests) or on older games, print directly.
    // OutputText ends at JS.addText -> print.
    if (world_.asl_version >= 540 && world_.find_function("OutputText")) {
        try {
            call_function("OutputText", {vstr(text)}, &ctx);
        } catch (const std::exception &err) {
            // PrintAsync's catch is LogException-ONLY: a throw that escaped
            // OutputText's own script boundary (i.e. the depth-cap throw) is
            // swallowed silently and the caller carries on -- a deep msg
            // just prints nothing.
            log_exception(err.what());
        }
    } else {
        // Pre-540 PrintText wraps the text in <output>...</output>, so an
        // EMPTY print (the pre-v520 echo's leading blank) strips to nothing
        // downstream -- emit nothing, like the oracle transcripts.
        //
        // qvh's Emit strips FIRST and then appends '\n' only when the
        // stripped chunk lacks one, so a paragraph's TRAILING <br/> yields a
        // single newline (no blank line: The Zen Garden's 5.3-era
        // ShowRoomDescription ends every autodescription paragraph with one),
        // while <br/><br/> keeps its blank. Appending '\n' to the RAW text
        // here (the old behaviour) turned every trailing <br/> into a blank
        // line. Mirror qvh: only terminate chunks that don't already end in a
        // newline-producing tail. qvh's "<br\s*/?>" regex is lowercase-only,
        // so an uppercase <BR> tail is NOT newline-producing there -- but it
        // strips to nothing, and the appended '\n' then produces the same
        // single newline either way, so lowercase-only matching stays exact.
        //
        // The tail may be a <br/> FOLLOWED by closing tags that strip to
        // nothing -- Nearco II's PrintCentered ends "...<br/></align>",
        // which qvh strips to "...\n" (no extra append). Walk backwards over
        // trailing tags: any non-br tag strips to nothing and is skipped; a
        // br tag produces the newline; anything else means no newline.
        if (!text.empty()) {
            size_t e = text.size();
            bool ends_nl = false;
            for (;;) {
                if (e == 0) break;
                char c = text[e - 1];
                if (c == '\n') { ends_nl = true; break; }
                if (c != '>') break;
                size_t lt = text.rfind('<', e - 1);
                if (lt == std::string::npos) break;
                std::string tag = text.substr(lt + 1, e - lt - 2);
                // An <img> strips to nothing under qvh too, but it IS visible
                // content for the Glk front end (v540+ `picture`), so it
                // keeps its line like text would.
                if (tag.compare(0, 3, "img") == 0 &&
                    (tag.size() == 3 || !std::isalnum((unsigned char)tag[3])))
                    break;
                size_t j = 2;
                if (tag.compare(0, 2, "br") == 0) {
                    while (j < tag.size() && (tag[j] == ' ' || tag[j] == '\t')) j++;
                    if (j < tag.size() && tag[j] == '/') j++;
                    if (j == tag.size()) { ends_nl = true; break; }
                }
                // any other tag strips to nothing under qvh's Strip -- skip
                // it and keep looking at what precedes it.
                e = lt;
            }
            // The whole text was tags that strip to nothing (a gamebook
            // option with an empty display text: `<command input="Murder2">
            // </command>`, Fun Tiemz / minecraft adventure). qvh's Emit
            // skips an empty stripped chunk outright -- no '\n' appended --
            // so pass the markup through without the line break rather than
            // print a blank line.
            if (e == 0 && !ends_nl) { print(text); return; }
            print(ends_nl ? text : text + "\n");
        }
    }
}

// Utility.SafeXML -- also the safexml builtin.
std::string safe_xml_escape(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

// -- input model (TODO §3): host entry points --------------------------------

void Interp::send_command(const std::string &command) {
    // HandleCommandAsyncInternal. A pending `get input` consumes the line
    // (command override) instead of the parser.
    if (command_override_) {
        PendingCallback cb = take_prompt(command_override_, command_cb_);
        cb.ctx.locals["result"] = vstr(command);
        // SignalCallbackResolving before the body: AddOnReady during the
        // callback runs now (classic Pop), not deferred behind later statements.
        signal_callback_resolving();
        try {
            run_callback_boundary(cb.body, cb.ctx);
        } catch (TurnSuspended &ts) {
            // Parked on a sync sound (see resolve_prompt).
            park_suspension(ts, /*owes_update=*/false, /*owes_endcb=*/1);
            return;
        }
        try {
            // The turn boundary: end_pending_callback runs the FinishTurn the
            // `get input` command deferred (legacy RunCallbackAndFinishTurn).
            end_pending_callback();
        } catch (TurnSuspended &ts) {
            park_suspension(ts, /*owes_update=*/false, /*owes_endcb=*/0);
        }
        return;
    }
    Context ctx;
    // Pre-v580 run_command calls FinishTurn as a step AFTER HandleCommand. If
    // HandleCommand suspends (sync sound), that FinishTurn is owed by the park
    // (see resume_parked_tail); cleared the moment we actually reach the call.
    bool owes_ft = world_.asl_version < 580;
    try {
        if (world_.asl_version < 520) {
            // Pre-v520 games expect the engine to echo the command.
            print_via_core("", ctx);
            print_via_core("> " + safe_xml_escape(command), ctx);
        }
        if (world_.find_function("HandleCommand")) {
            try {
                call_function("HandleCommand", {vstr(command), vnull()}, &ctx);
            } catch (const std::exception &err) {
                // HandleCommandAsyncInternal's catch: LogException-only.
                log_exception(err.what());
            }
        }
        // Pre-v580 Core relies on the engine calling FinishTurn after the
        // command (TryFinishTurnAsync); v580+ Core runs it from HandleCommand
        // itself.
        if (world_.asl_version < 580) {
            owes_ft = false;  // reached the call; if FinishTurn itself parks,
                              // its frames carry the tail (not re-owed here)
            try_finish_turn_or_defer(ctx);
        }
        // HandleCommandAsyncInternal ends the turn with a pane refresh (its
        // std::exception throws are LogException'd inside update_lists). A
        // deferred FinishTurn owes the refresh too (run_deferred_finish_turn).
        if (!world_.finished && !finish_turn_deferred_) update_lists();
    } catch (TurnSuspended &ts) {
        // Synchronous `play sound` mid-command: the rest of the turn --
        // including FinishTurn -- parks on the wait slot, resumed when the
        // slot is next claimed (see resume_parked_tail). A suspend from within
        // HandleCommand still owes the deferred FinishTurn; one from within
        // FinishTurn does not (owes_ft already cleared).
        park_suspension(ts, /*owes_update=*/true, /*owes_endcb=*/0, owes_ft);
    }
}

void Interp::set_menu_response(const std::string *key) {
    if (!menu_pending_) return;
    MenuData md = std::move(menu_);
    menu_ = MenuData{};
    PendingCallback cb = take_prompt(menu_pending_, menu_cb_);
    if (key) {
        // ShowMenuScript echoes the chosen option's display text.
        for (const auto &kv : md.options) {
            if (kv.first == *key) {
                print_via_core(" - " + kv.second, cb.ctx);
                break;
            }
        }
        cb.ctx.locals["result"] = vstr(*key);
    } else {
        cb.ctx.locals["result"] = vnull();  // cancelled
    }
    resolve_prompt(cb, /*inline_prompt=*/true);
}

void Interp::set_question_response(bool response) {
    if (!question_pending_) return;
    question_.clear();
    PendingCallback cb = take_prompt(question_pending_, question_cb_);
    cb.ctx.locals["result"] = vbool(response);
    resolve_prompt(cb, /*inline_prompt=*/true);
}

void Interp::try_finish_turn(Context &ctx) {
    // WorldModel.TryFinishTurnAsync: LogException-only. Core's RunTurnScripts
    // self-guards on IsGameRunning(), so this no-ops once the game has finished.
    if (!world_.find_function("FinishTurn")) return;
    try {
        call_function("FinishTurn", {}, &ctx);
    } catch (const std::exception &err) {
        log_exception(err.what());
    }
}

void Interp::try_finish_turn_or_defer(Context &ctx) {
    // The post-handler step of a pre-v580 command/event turn (callers gate on
    // the version). While a wait / get input / ask / show menu this turn
    // registered is still outstanding, FinishTurn is deferred to the turn
    // boundary -- end_pending_callback at zero -- rather than run now against
    // state the callback hasn't reached (see finish_turn_deferred_).
    if (pending_callback_count_ > 0) {
        finish_turn_deferred_ = true;
        return;
    }
    try_finish_turn(ctx);
}

void Interp::run_deferred_finish_turn() {
    // WorldModel.RunDeferredFinishTurnAsync: the FinishTurn a command/event
    // deferred past a suspension (see finish_turn_deferred_), then the pane
    // refresh that turn skipped. Called from end_pending_callback once the
    // pending count is back to zero, after the deferred on-ready flush --
    // legacy TryFinishTurn ran TryRunOnFinallyScripts first, the same way
    // round.
    if (!finish_turn_deferred_) return;
    finish_turn_deferred_ = false;
    if (world_.asl_version < 580) {
        Context tctx;
        try_finish_turn(tctx);
    }
    if (!world_.finished) update_lists();
}

void Interp::finish_wait() {
    if (!wait_pending_) {
        // WorldModel.FinishWait is `_waitTcs?.TrySetResult()`: if the wait
        // slot is held by a parked synchronous `play sound` instead of a
        // wait, completing the TCS resumes that continuation.
        resume_parked_tail();
        return;
    }
    PendingCallback cb = take_prompt(wait_pending_, wait_cb_);
    resolve_prompt(cb, /*inline_prompt=*/false);
}

void Interp::park_suspension(TurnSuspended &ts, bool owes_update,
                             int owes_endcb, bool owes_finishturn) {
    // Store the captured continuation of a synchronous `play sound` at a turn
    // boundary. Frames arrive innermost-first; every script boundary claimed
    // its frames with a context snapshot during the unwind. A suspension can
    // only reach here with no previous park outstanding (the play-sound
    // statement itself resumes any parked tail before throwing).
    if (!ts.frames.empty() && ts.frames.back().stmts) {
        // Defensive: a suspension outside any script boundary -- claim the
        // stragglers with an empty context so resume still consumes them.
        ts.frames.push_back(TurnSuspended::Frame{});
    }
    if (sound_parked_) {
        // A park is already outstanding: the play-sound statement resumed the
        // old tail inline and that tail re-parked before THIS unwind landed.
        // The old remainder logically precedes this continuation, so queue
        // the new frame groups behind it instead of overwriting (which would
        // silently drop whichever side lost the move).
        for (auto &f : ts.frames)
            parked_frames_.push_back(std::move(f));
    } else {
        sound_parked_ = true;
        parked_frames_ = std::move(ts.frames);
    }
    parked_owes_update_ = parked_owes_update_ || owes_update;
    parked_owes_endcb_ += owes_endcb;
    parked_owes_finishturn_ = parked_owes_finishturn_ || owes_finishturn;
}

void Interp::resume_parked_tail() {
    // The wait slot held by a parked synchronous `play sound` has been
    // claimed (BeginPrompt TrySetCanceled from a new `wait`/sync sound) or
    // completed (host FinishWait): the parked continuation runs INLINE, right
    // here, before the caller registers its own prompt -- exactly like the
    // default TaskCompletionSource running the awaiter synchronously. Each
    // boundary group re-runs inside an equivalent script boundary: an error
    // aborts that group only (RunScriptAsync semantics), and a NEW sync sound
    // re-parks the not-yet-run remainder.
    if (!sound_parked_) return;
    sound_parked_ = false;
    std::vector<TurnSuspended::Frame> frames = std::move(parked_frames_);
    parked_frames_.clear();
    bool owes_update = parked_owes_update_;
    int owes_endcb = parked_owes_endcb_;
    bool owes_finishturn = parked_owes_finishturn_;
    parked_owes_update_ = false;
    parked_owes_endcb_ = 0;
    parked_owes_finishturn_ = false;
    if (script_errors_fatal_) return;
    size_t i = 0;
    while (i < frames.size()) {
        size_t b = i;
        while (b < frames.size() && frames[b].stmts) ++b;
        if (b >= frames.size()) break;  // defensive: no boundary, drop
        Context ctx = std::move(frames[b].ctx);
        if (script_depth_ >= kMaxScriptDepth) {
            report_script_error(kDepthExceeded);
            break;
        }
        ++script_depth_;
        size_t j = i;
        try {
            for (; j < b; ++j) {
                exec_block_from(*frames[j].stmts, frames[j].next, ctx);
                if (ctx.returned) break;
            }
            --script_depth_;
        } catch (TurnSuspended &ts) {
            --script_depth_;
            // Parked again mid-resume: the new unwind captured the inner
            // frames; claim them with this group's context, then re-append
            // the untouched remainder of the original tail.
            for (size_t k = j + 1; k < b; ++k)
                ts.frames.push_back(std::move(frames[k]));
            ts.frames.push_back(TurnSuspended::Frame{nullptr, 0, ctx});
            for (size_t k = b + 1; k < frames.size(); ++k)
                ts.frames.push_back(std::move(frames[k]));
            sound_parked_ = true;
            parked_frames_ = std::move(ts.frames);
            parked_owes_update_ = owes_update;
            parked_owes_endcb_ = owes_endcb;
            parked_owes_finishturn_ = owes_finishturn;
            return;
        } catch (const std::exception &err) {
            --script_depth_;
            report_script_error(err.what());
        }
        i = b + 1;
    }
    // The C++-side tail the suspended chain still owed, in QuestViva's order:
    // the prompt-callback finallys (EndPendingCallback), then -- for a parked
    // COMMAND turn -- the deferred FinishTurn (turnscripts), then the pane
    // refresh. Each can itself park again. The FinishTurn matches QuestViva's
    // command chain resuming through TryFinishTurnAsync: Core's RunTurnScripts
    // self-guards on IsGameRunning(), so it no-ops when the game has finished.
    try {
        while (owes_endcb > 0) {
            --owes_endcb;
            end_pending_callback();
        }
        if (owes_finishturn) {
            owes_finishturn = false;
            if (world_.asl_version < 580) {
                Context tctx;
                try_finish_turn_or_defer(tctx);
            }
        }
        if (owes_update && !world_.finished && !finish_turn_deferred_)
            update_lists();
    } catch (TurnSuspended &ts) {
        park_suspension(ts, owes_update, owes_endcb, owes_finishturn);
    }
}

void Interp::send_event(const std::string &name, const std::string &param) {
    // WorldModel.SendEventCore -- the ASLEvent bridge (hyperlink onclicks).
    Context ctx;
    if (!world_.find_function(name)) {
        print_via_core("Error - no handler for event '" + name + "'", ctx);
        return;
    }
    try {
        call_function(name, {vstr(param)}, &ctx);
        // SendEventCore's version switch: pre-540 stops after the handler;
        // 540..579 runs TryFinishTurnAsync (LogException-only, like
        // send_command); then the panes refresh.
        if (world_.asl_version < 540) return;
        if (world_.asl_version < 580) try_finish_turn_or_defer(ctx);
        if (!world_.finished && !finish_turn_deferred_) update_lists();
    } catch (TurnSuspended &ts) {
        // synchronous `play sound`: the rest of the event chain parks on the
        // wait slot (FinishTurn and the pane refresh are owed by the resume)
        park_suspension(ts, /*owes_update=*/true, /*owes_endcb=*/0);
    } catch (const std::exception &) {
        // reported at the script boundary already; SendEventCore has no catch
        // of its own, so the throw just kills this fire-and-forget chain
        // (FinishTurn and the pane refresh are skipped)
    }
}

// -- pane updates (WorldModel.UpdateListsAsync port) --------------------------

std::vector<Element *> Interp::objects_in_scope(const std::string &scope) {
    // GetObjectsInScopeAsync: throws when the scope function is missing
    // (update_lists' catch turns that into a LogException, like the callers'
    // catches in the reference).
    if (!world_.find_function(scope))
        throw std::runtime_error("No function '" + scope + "'");
    Context ctx;
    Value v = call_function(scope, {}, &ctx);
    std::vector<Element *> out;
    if (v.list_store)
        for (const Value &e : *v.list_store)
            if (e.type == Value::Type::ObjectRef)
                if (Element *el = world_.find(e.str))
                    out.push_back(el);
    return out;
}

ListData Interp::list_data_for(Element *obj, bool inventory) {
    ListData d;
    d.element_name = obj->name;
    Context ctx;

    // GetDisplayAliasAsync: Core function when present, element name otherwise.
    d.display_alias = world_.find_function("GetDisplayAlias")
        ? to_string(call_function("GetDisplayAlias", {vobj(obj->name)}, &ctx))
        : obj->name;

    // GetListDisplayAliasAsync: the pane label (may carry {}-processed markup).
    d.text = world_.find_function("GetListDisplayAlias")
        ? to_string(call_function("GetListDisplayAlias", {vobj(obj->name)}, &ctx))
        : d.display_alias;

    // Verbs: pre-v520 (or Core-less) reads the inventoryverbs/displayverbs
    // fields directly; later Core supplies GetDisplayVerbs.
    if (world_.asl_version <= 520 || !world_.find_function("GetDisplayVerbs")) {
        const Value *verbs =
            resolve_field(obj, inventory ? "inventoryverbs" : "displayverbs");
        if (verbs && verbs->list_store)
            for (const Value &e : *verbs->list_store)
                d.verbs.push_back(to_string(e));
    } else {
        Value verbs = call_function("GetDisplayVerbs", {vobj(obj->name)}, &ctx);
        if (verbs.list_store)
            for (const Value &e : *verbs.list_store)
                d.verbs.push_back(to_string(e));
    }
    return d;
}

std::vector<ListData> Interp::verb_menu_objects() {
    // Same objects and verb sources the pane uses (see update_lists), minus the
    // exits: inventory objects carry inventoryverbs, room/place objects carry
    // displayverbs. Inventory first, so a carried object wins a name clash.
    std::vector<ListData> out;
    for (Element *e : objects_in_scope("ScopeInventory"))
        out.push_back(list_data_for(e, true));
    for (Element *e : objects_in_scope("GetPlacesObjectsList"))
        out.push_back(list_data_for(e, false));
    return out;
}

bool Interp::verb_menu_for(const std::string &element_name, ListData &out) {
    Element *el = world_.find(element_name);
    if (!el || el->kind != ElemKind::Object) return false;
    // inventoryverbs when carried, displayverbs otherwise -- the same split
    // verb_menu_objects() makes, decided here by asking whether the object is
    // in ScopeInventory rather than by which list it came out of.
    bool held = false;
    for (Element *e : objects_in_scope("ScopeInventory"))
        if (e == el) { held = true; break; }
    out = list_data_for(el, held);
    return true;
}

std::vector<ListData> Interp::exits_list_data() {
    // GetExitsListDataAsync: ScopeExits, or GetExitsList on v530+.
    std::string scope = "ScopeExits";
    if (world_.asl_version >= 530 && world_.find_function("GetExitsList"))
        scope = "GetExitsList";
    std::vector<ListData> out;
    for (Element *e : objects_in_scope(scope))
        out.push_back(list_data_for(e, false));
    return out;
}

void Interp::update_status_variables() {
    // UpdateStatusVariablesAsync: Core's UpdateStatusAttributes ends in
    // JS.updateStatus. Its own catch is LogException-only.
    if (!world_.find_function("UpdateStatusAttributes")) return;
    Context ctx;
    try {
        call_function("UpdateStatusAttributes", {}, &ctx);
    } catch (const std::exception &err) {
        log_exception(err.what());
    }
}

void Interp::update_lists() {
    // UpdateListsAsync: the object/exit lists only exist for a subscriber
    // (QuestViva skips them when the UpdateList event is null -- the oracle
    // never subscribes, so headless parity costs nothing); the status
    // attributes run regardless. A throw from the list computations skips
    // the status refresh, mirroring the propagation in the reference, and is
    // logged-not-printed by the callers' LogException-only catches.
    // (ElementMenuVerbs -- live verb menus for inline {object:} links -- is
    // deliberately not ported: the Glk front-end has no pop-up verb menus.)
    try {
        if (update_list) {
            std::vector<ListData> places;
            for (Element *e : objects_in_scope("GetPlacesObjectsList"))
                places.push_back(list_data_for(e, false));
            // The "Places and Objects" list is generated by function, so the
            // exits are appended too; the UI is responsible for filtering the
            // compass directions out so they only show in the compass.
            for (ListData &d : exits_list_data())
                places.push_back(std::move(d));
            update_list("placesobjects", places);

            std::vector<ListData> inv;
            for (Element *e : objects_in_scope("ScopeInventory"))
                inv.push_back(list_data_for(e, true));
            update_list("inventory", inv);

            // UpdateExitsListAsync (computed twice, like the reference).
            update_list("exits", exits_list_data());
        }
    } catch (const std::exception &err) {
        log_exception(err.what());
        return;
    }
    update_status_variables();
}

// -- timers (TimerRunner port) ------------------------------------------------

// Live <timer> elements: still reachable by name. `destroy` only unregisters
// (the storage stays in world.elements), and a SetTimeout timer destroys
// itself after firing, so the name check is what retires it.
std::vector<Element *> Interp::live_timers() {
    std::vector<Element *> out;
    for (auto &up : world_.elements)
        if (up->kind == ElemKind::Timer && up->registered)
            out.push_back(up.get());
    return out;
}

long Interp::field_int(Element *e, const char *name) {
    const Value *v = resolve_field(e, name);
    if (!v) return 0;
    if (v->type == Value::Type::Int) return v->integer;
    if (v->type == Value::Type::Double) return (long)v->dbl;
    return 0;
}

bool Interp::timer_enabled(Element *t) {
    const Value *v = resolve_field(t, "enabled");
    return v && truthy(*v);
}

long Interp::time_elapsed() {
    Element *game = world_.find("game");
    return game ? field_int(game, "timeelapsed") : 0;
}

void Interp::set_time_elapsed(long t) {
    // TimerRunner's writes go through Fields.Set, so while a command's
    // transaction is still open (timer ticks happen at prompt level, after
    // `start transaction` and before the next one) they are undoable too --
    // hence set_field_logged here and below.
    if (Element *game = world_.find("game"))
        set_field_logged(game, "timeelapsed", vint(t));
}

void Interp::begin_timers() {
    for (Element *t : live_timers())
        if (timer_enabled(t))
            t->set_field("trigger", vint(field_int(t, "interval")));
}

void Interp::increment_time(int seconds) {
    set_time_elapsed(time_elapsed() + seconds);
    long now = time_elapsed();
    for (Element *t : live_timers()) {
        if (timer_enabled(t)) continue;
        // Disabled timers get their triggers pushed into the future, so they
        // will run if they become enabled (TimerRunner.IncrementTime).
        Value v = field_int(t, "trigger") < now
                      ? vint(now + field_int(t, "interval"))
                      : vint(field_int(t, "trigger") + seconds);
        set_field_logged(t, "trigger", v);
    }
}

void Interp::tick(int seconds) {
    if (world_.finished) return;
    if (seconds > 0) increment_time(seconds);
    // Collect every due timer first, bumping its next trigger, THEN run the
    // scripts (TickAndGetScripts returns the batch before any script runs) --
    // a SetTimeout script disables and destroys its own timer mid-run.
    long now = time_elapsed();
    std::vector<std::pair<Element *, std::string>> due;
    for (Element *t : live_timers()) {
        if (!timer_enabled(t)) continue;
        if (now >= field_int(t, "trigger")) {
            set_field_logged(
                t, "trigger",
                vint(field_int(t, "trigger") + field_int(t, "interval")));
            const Value *scr = resolve_field(t, "script");
            if (scr && scr->type == Value::Type::Script)
                due.emplace_back(t, scr->str);
        }
    }
    bool suspended = false;
    for (auto &d : due) {
        if (world_.finished) break;
        Context local;
        local.locals["this"] = vobj(d.first->name);
        try {
            run_script(d.second, local, field_scope(d.first, "script"));
        } catch (TurnSuspended &ts) {
            // A suspended timer script suspends the whole batch
            // (TickAsyncInternal's await chain) -- including the pane
            // refresh. The suspended script's own tail parks; the batch's
            // REMAINING timer scripts are C++ loop state and are dropped
            // (known gap, matches the pre-park behaviour for them).
            park_suspension(ts, /*owes_update=*/true, /*owes_endcb=*/0);
            suspended = true;
            break;
        }
    }
    // TickAsyncInternal refreshes the panes after the batch (inside the same
    // LogException-only try, which update_lists provides itself).
    if (!suspended) {
        try {
            update_lists();
        } catch (TurnSuspended &ts) {
            park_suspension(ts, /*owes_update=*/false, /*owes_endcb=*/0);
        }
    }
}

int Interp::next_timer_seconds() {
    long now = time_elapsed();
    long next = now + 60;
    bool any = false;
    for (Element *t : live_timers()) {
        if (!timer_enabled(t)) continue;
        any = true;
        if (field_int(t, "trigger") < next) next = field_int(t, "trigger");
    }
    return any ? (int)(next - now) : 0;
}

bool Interp::has_enabled_timeout() {
    for (Element *t : live_timers())
        if (t->name.compare(0, 7, "timeout") == 0 && timer_enabled(t))
            return true;
    return false;
}

Element *interp_eval_element(Interp &in, const Value &v) {
    if (v.type == Value::Type::ObjectRef) return in.world().find(v.str);
    return nullptr;
}

// Shared body of the `rundelegate` statement and the RunDelegateFunction
// built-in: look up the delegate implementation (a Script field whose
// declared_type names the <delegate> element), bind params by the delegate's
// paramnames, bind `this`, run, and return the script's return value.
Value interp_run_delegate(Interp &in, Element *obj, const std::string &delname,
                          const std::vector<Value> &params) {
    if (!obj) in.error("rundelegate: not an object");
    const Value *impl = in.resolve_field(obj, delname);
    if (!impl || impl->type != Value::Type::Script)
        in.error("Object '" + obj->name + "' has no delegate implementation '" +
                 delname + "'");
    Context local;
    // The delegate signature (parameter names) is identified by the delegate's
    // NAME == the field name used to invoke it, not by the impl field's
    // declared_type: Core declares implementations as `type="script"`, so
    // declared_type carries no delegate name. Fall back to a case-insensitive
    // lookup by delname (e.g. "addscript" -> <delegate name="AddScript">) so the
    // params bind -- without this, container_limited.addscript ran with `object`
    // unbound ("Unknown object or variable 'object'").
    Element *def = in.world().find(impl->declared_type);
    if (!def || def->kind != ElemKind::Delegate)
        def = in.world().find_delegate(delname);
    const Value *pn = def ? def->field("paramnames") : nullptr;
    for (size_t k = 0; k < params.size(); ++k) {
        if (pn && k < pn->list().size())
            local.locals[Interp::to_string(pn->list()[k])] = params[k];
    }
    Value self; self.type = Value::Type::ObjectRef; self.str = obj->name;
    local.locals["this"] = self;
    in.run_script(impl->str, local, in.field_scope(obj, delname));
    return local.return_value;
}

// -- undo (UndoLogger.cs port) ------------------------------------------------
//
// Transactions are lazy: Core's parser runs `start transaction (cmd)` for each
// successfully-parsed non-<isundo/> command, which COMMITS the previous
// command's transaction and opens this command's; the open transaction is only
// ever committed by the next command or by `undo` itself. Actions are recorded
// only while a transaction is open (AddUndoAction checks m_logging), so
// nothing from boot/StartGame is undoable.

void Interp::add_undo(UndoAction a) {
    if (!undo_logging_) return;
    current_txn_->actions.push_back(std::move(a));
}

void Interp::start_transaction(const std::string &command) {
    if (undo_logging_)
        error("Starting transaction when previous transaction not finished");
    undo_logging_ = true;
    // An empty (nothing-logged) transaction is skipped over in the chain.
    std::shared_ptr<UndoTransaction> prev;
    if (current_txn_)
        prev = !current_txn_->actions.empty() ? current_txn_
                                              : current_txn_->previous;
    current_txn_ = std::make_shared<UndoTransaction>();
    current_txn_->description = command;
    current_txn_->previous = prev;
}

void Interp::end_transaction() {
    undo_logging_ = false;
    if (current_txn_ && !current_txn_->actions.empty()) {
        undo_stack_.push_back(current_txn_);
        redo_stack_.clear();
    }
}

void Interp::roll_transaction(const std::string &command) {
    if (current_txn_)
        end_transaction();
    start_transaction(command);
}

void Interp::rollback_transaction(Context &ctx) {
    // RollbackTransaction: commit whatever the last command logged, undo one
    // transaction, and step the chain back so the next `start transaction`
    // re-commits the previous record (a genuine QuestViva quirk: after
    // undo-then-command-then-undo the same transaction can be rolled back
    // twice; the in-place reverse in undo_once makes the second application
    // run forward, also as in the reference).
    if (undo_logging_)
        end_transaction();
    undo_once(ctx);
    if (current_txn_)
        current_txn_ = current_txn_->previous;
}

void Interp::undo_once(Context &ctx) {
    // UndoLogger.Undo: an empty stack prints the NothingToUndo template, or
    // errors when the game has none (GamebookCore never starts transactions
    // but supplies the template).
    if (undo_stack_.empty()) {
        if (const std::string *t = find_template(world_, "NothingToUndo"))
            print_via_core(*t, ctx);
        else
            error("Nothing to undo");
        return;
    }
    std::shared_ptr<UndoTransaction> txn = undo_stack_.back();
    undo_stack_.pop_back();
    // Transaction.DoUndo: announce via the UndoTurn dynamic template
    // ("Undo: " + text in English.aslx), then apply the actions in reverse.
    if (const std::string *dt = find_dyn_template(world_, "UndoTurn")) {
        Context tc;
        tc.locals["text"] = vstr(txn->description);
        print_via_core(to_string(eval(*dt, tc)), ctx);
    }
    std::reverse(txn->actions.begin(), txn->actions.end());
    for (UndoAction &a : txn->actions)
        apply_undo_action(a);
    redo_stack_.push_back(txn);
}

void Interp::apply_undo_action(UndoAction &a) {
    // undo_logging_ is always false here (end_transaction ran first), so the
    // mutations below log nothing -- matching how QuestList.AddInternal's
    // re-logging is a no-op during DoUndo.
    switch (a.kind) {
    case UndoAction::Kind::FieldSet: {
        // UndoFieldSet.DoUndo: re-resolve by name; an added attribute is
        // removed, a changed one restored via SetFromUndo -- no changed-script
        // events, no cloning (the old backing pointer comes back verbatim,
        // which is what any list/dict actions of the same transaction hold).
        Element *e = world_.find(a.element);
        if (!e) return;
        if (a.added)
            e->remove_field(a.attr);
        else
            e->set_field(a.attr, a.old_value);
        if (a.attr == "parent") world_.note_containment_change();
        return;
    }
    case UndoAction::Kind::FieldRemove: {
        Element *e = world_.find(a.element);
        if (e) e->set_field(a.attr, a.old_value);
        if (a.attr == "parent") world_.note_containment_change();
        return;
    }
    case UndoAction::Kind::SortIndex: {
        Element *e = world_.find(a.element);
        if (e) e->sort_index = a.index;
        world_.note_containment_change();
        return;
    }
    case UndoAction::Kind::Create:
        // CreateDestroyLogEntry undo-of-create: Elements.Remove -- the storage
        // stays alive (like our destroy_element), only the name unregisters.
        world_.unregister_name(a.element);
        return;
    case UndoAction::Kind::Destroy:
        // Undo-of-destroy: re-register the kept-alive element, fields intact.
        world_.register_name(a.element, a.element_ptr);
        return;
    case UndoAction::Kind::ListAdd: {
        std::vector<Value> &v = *a.list_backing;
        if ((size_t)a.index < v.size())
            v.erase(v.begin() + a.index);
        return;
    }
    case UndoAction::Kind::ListRemove: {
        std::vector<Value> &v = *a.list_backing;
        size_t at = (size_t)a.index <= v.size() ? (size_t)a.index : v.size();
        v.insert(v.begin() + at, a.old_value);
        return;
    }
    case UndoAction::Kind::DictAdd: {
        std::vector<Value::DictEntry> &d = *a.dict_backing;
        for (auto it = d.begin(); it != d.end(); ++it)
            if (it->first == a.attr) { d.erase(it); break; }
        return;
    }
    case UndoAction::Kind::DictRemove: {
        std::vector<Value::DictEntry> &d = *a.dict_backing;
        size_t at = (size_t)a.index <= d.size() ? (size_t)a.index : d.size();
        d.insert(d.begin() + at, {a.attr, a.old_value});
        return;
    }
    case UndoAction::Kind::FirstTime:
        if (a.ran) *a.ran = false;
        return;
    }
}

void Interp::log_field_set(Element *e, const std::string &attr,
                           const Value &newval, bool removing) {
    if (!undo_logging_) return;
    const Value *own = e->field(attr);
    if (removing) {
        // Fields.RemoveField -> UndoFieldRemove; only a real removal logs.
        if (!own) return;
        UndoAction a;
        a.kind = UndoAction::Kind::FieldRemove;
        a.element = e->name;
        a.attr = attr;
        a.old_value = *own;
        add_undo(std::move(a));
        return;
    }
    // Fields.Set logs only when the OWN attribute changes (same check that
    // gates clone-on-set); `added` decides remove-vs-restore at undo time.
    bool added = !own;
    if (!added && values_equal(*own, newval))
        return;
    UndoAction a;
    a.kind = UndoAction::Kind::FieldSet;
    a.element = e->name;
    a.attr = attr;
    a.added = added;
    if (!added) a.old_value = *own;
    add_undo(std::move(a));
}

Value &Interp::set_field_logged(Element *e, const std::string &attr,
                                const Value &v) {
    log_field_set(e, attr, v, /*removing=*/false);
    return e->set_field(attr, v);
}

void Interp::log_sort_index(Element *e) {
    if (!undo_logging_) return;
    UndoAction a;
    a.kind = UndoAction::Kind::SortIndex;
    a.element = e->name;
    a.index = e->sort_index;
    add_undo(std::move(a));
}

void Interp::log_create(Element *e) {
    if (!undo_logging_) return;
    UndoAction a;
    a.kind = UndoAction::Kind::Create;
    a.element = e->name;
    add_undo(std::move(a));
}

void Interp::log_destroy(Element *e) {
    if (!undo_logging_) return;
    UndoAction a;
    a.kind = UndoAction::Kind::Destroy;
    a.element = e->name;
    a.element_ptr = e;
    add_undo(std::move(a));
}

void Interp::log_list_change(UndoAction::Kind kind, const Value &coll,
                             long index, const Value &entry) {
    if (!undo_logging_) return;
    UndoAction a;
    a.kind = kind;
    a.list_backing = coll.list_store;
    a.old_value = entry;
    a.index = index;
    add_undo(std::move(a));
}

void Interp::log_dict_change(UndoAction::Kind kind, const Value &coll,
                             long index, const std::string &key,
                             const Value &entry) {
    if (!undo_logging_) return;
    UndoAction a;
    a.kind = kind;
    a.dict_backing = coll.dict_store;
    a.attr = key;
    a.old_value = entry;
    a.index = index;
    add_undo(std::move(a));
}

// -- expression evaluation --------------------------------------------------

// Walk the field chain (own field, then inherited types most-recent-first,
// depth-first) collecting the base value -- the first NON-extend field -- and
// every `listextend` field encountered anywhere in the chain, in traversal
// order (most-derived first). Mirrors QuestViva Fields.Get: extendable fields
// live outside normal resolution (m_extendableFields) and merge on read.
static void collect_field_chain(World &w, Element *e, const std::string &name,
                                const Value *&base,
                                std::vector<const Value *> &exts,
                                std::vector<const Element *> &path) {
    // `path` holds the elements on the active recursion stack; a repeat means a
    // runtime inheritance cycle (a bad <inherit> in the file, or a runtime
    // parent/type edit), which would otherwise recurse until the C++ stack
    // overflows. Popping on exit keeps this a back-edge check only, so legal
    // diamond inheritance still visits a shared base once per path -- the
    // listextend accumulation below must not be deduplicated.
    //
    // path is a stack used strictly LIFO (push on entry, pop on exit), and
    // inheritance chains are short, so a plain vector with a linear back-edge
    // scan is cheaper than an unordered_set: no per-node hashing and no heap
    // churn (the set's insert/erase were a profile hotspot on Woo Rebooted).
    if (!e) return;
    for (const Element *p : path)
        if (p == e) return;
    path.push_back(e);
    if (const Value *own = e->field(name)) {
        if (own->list_extend) exts.push_back(own);
        else if (!base) base = own;
    }
    for (auto it = e->inherits.rbegin(); it != e->inherits.rend(); ++it)
        collect_field_chain(w, w.find(*it), name, base, exts, path);
    path.pop_back();
}

// The element whose OWN (non-extend) field `name` resolves to from `e`: the
// same traversal as collect_field_chain's base-value search.
static const Element *find_field_owner(World &w, Element *e, const std::string &name,
                                       std::vector<const Element *> &path) {
    if (!e) return nullptr;
    for (const Element *p : path)
        if (p == e) return nullptr;
    path.push_back(e);
    const Element *owner = nullptr;
    if (const Value *own = e->field(name); own && !own->list_extend)
        owner = e;
    for (auto it = e->inherits.rbegin(); !owner && it != e->inherits.rend(); ++it)
        owner = find_field_owner(w, w.find(*it), name, path);
    path.pop_back();
    return owner;
}

std::string Interp::field_scope(Element *e, const std::string &attr) {
    std::vector<const Element *> path;
    const Element *owner = find_field_owner(world_, e, attr, path);
    return owner ? scope_key(owner->name, attr) : std::string();
}

const Value *Interp::resolve_field(Element *e, const std::string &name) {
    if (!e) return nullptr;
    const Value *base = nullptr;
    std::vector<const Value *> exts;
    std::vector<const Element *> path;
    collect_field_chain(world_, e, name, base, exts, path);
    if (exts.empty()) return base;  // fast path: no listextend in the chain
    // Merge on read (Fields.GetMergedResult): the base list's entries first,
    // then each extension least-derived to most-derived (QuestList.MergeLists
    // accumulates parent-first). A non-list base is ignored, like QuestViva's
    // null extendableBaseField. The merged Value lives in a per-(element,attr)
    // slot so the returned pointer stays valid; it is rebuilt on every read
    // (QuestViva re-merges each Get too).
    Value merged;
    bool base_is_list = base && is_list(*base);
    merged.type = base_is_list ? base->type : exts.front()->type;
    if (base_is_list)
        for (const Value &v : base->list()) merged.list().push_back(v);
    for (auto it = exts.rbegin(); it != exts.rend(); ++it)
        for (const Value &v : (*it)->list()) merged.list().push_back(v);
    Value &slot = extend_cache_[e->name + "\x01" + name];
    slot = std::move(merged);
    return &slot;
}

Value Interp::resolve_variable(const std::string &name, Context &ctx, bool &found) {
    found = true;
    if (rt_iequals(name, "null")) return vnull();
    auto it = ctx.locals.find(name);
    if (it != ctx.locals.end()) return it->second;
    if (world_.find(name)) return vobj(name);
    found = false;
    return vnull();
}

bool values_equal(const Value &a, const Value &b) {
    if (a.type == Value::Type::Null || b.type == Value::Type::Null)
        return a.type == Value::Type::Null && b.type == Value::Type::Null;
    if (is_number(a) && is_number(b)) return as_double(a) == as_double(b);
    if (a.type == Value::Type::Boolean && b.type == Value::Type::Boolean)
        return a.boolean == b.boolean;
    // NCalc compares a bool against a number by converting the bool (true=1,
    // false=0): oracle-verified "false = 0" and "(6 and 8) = true" are True.
    if ((a.type == Value::Type::Boolean && is_number(b)) ||
        (is_number(a) && b.type == Value::Type::Boolean))
        return as_double(a) == as_double(b);
    if (a.type == Value::Type::ObjectRef && b.type == Value::Type::ObjectRef)
        return a.str == b.str;
    if ((a.type == Value::Type::String || a.type == Value::Type::ObjectRef) &&
        (b.type == Value::Type::String || b.type == Value::Type::ObjectRef))
        return a.str == b.str;
    // Lists and dictionaries compare by reference (shared backing), matching
    // .NET reference equality on QuestList/QuestDictionary instances.
    if (is_list(a) && is_list(b)) return a.list_store && a.list_store == b.list_store;
    if (a.dict_store && b.dict_store) return a.dict_store == b.dict_store;
    return false;
}

Value Interp::eval_expr(const Expr &e, Context &ctx) {
    // A root node (non-empty src) is QuestViva's Expression<T>.EvaluateAsync
    // boundary: a runtime failure below it is wrapped as "Error evaluating
    // expression '<src>': <msg>". A nested wrap (the Eval() builtin) re-wraps
    // with the outer source but keeps the innermost message, matching
    // `ex.InnerException?.Message ?? ex.Message`. Sub-expressions have empty
    // src and pass errors through untouched.
    if (!e.src.empty()) {
        // Activate this expression's own RNG stream (see Expr::rng) for the
        // duration of the evaluation; nested roots (function args, Eval) each
        // swap in their own.
        if (!e.rng) {
            const_cast<Expr &>(e).rng = std::make_shared<Rng>();
            e.rng->seed(aslx_default_seed());
        }
        Rng *prev = current_rng_;
        current_rng_ = e.rng.get();
        try {
            Value v = eval_expr_node(e, ctx);
            current_rng_ = prev;
            return v;
        } catch (const TurnSuspended &) {
            current_rng_ = prev;
            throw;
        } catch (const EvalError &err) {
            current_rng_ = prev;
            throw EvalError(e.src, err.original);
        } catch (const std::exception &err) {
            current_rng_ = prev;
            throw EvalError(e.src, err.what());
        }
    }
    return eval_expr_node(e, ctx);
}

Value Interp::eval_expr_node(const Expr &e, Context &ctx) {
    switch (e.kind) {
    case Expr::Kind::ParseError:
        throw std::runtime_error(e.str);
    case Expr::Kind::Num:
        return e.is_int ? vint((long)e.num) : vdouble(e.num);
    case Expr::Kind::Str: return vstr(e.str);
    case Expr::Kind::Bool: return vbool(e.boolean);
    case Expr::Kind::Null: return vnull();
    case Expr::Kind::Var: {
        // NCalc ships "e" and "pi" as built-in parameters, case-insensitively,
        // and they are looked up BEFORE the host's parameters -- a Quest local
        // called "e" cannot shadow Math.E (verified against the oracle).
        // Moquette's GetRandomPoisson relies on the constant existing at all.
        if (e.str.size() <= 2) {
            std::string lc;
            for (char c : e.str) lc += (char)std::tolower((unsigned char)c);
            if (lc == "e") return vdouble(2.718281828459045);
            if (lc == "pi") return vdouble(3.141592653589793);
        }
        bool found;
        Value v = resolve_variable(e.str, ctx, found);
        if (!found) {
            // QuestViva pre-encodes multi-word identifiers before parsing
            // (Utility.EncodeIdentifierSpaces), so its unknown-variable error
            // reports the ENCODED name ("The___SPACE___Mages___SPACE___..."
            // in The Last Hero's misspelled-room bugs). Mirror that exactly.
            std::string enc;
            for (char c : e.str) {
                if (c == ' ') enc += "___SPACE___";
                else enc += c;
            }
            error("Unknown object or variable '" + enc + "'");
            return vnull();
        }
        return v;
    }
    case Expr::Kind::Member: {
        Value ov = eval_expr(*e.a, ctx);
        // Property access on null throws in QuestViva (NcalcExpressionEvaluator:
        // "Property 'x' not found on '<receiver-type>'", where a null receiver
        // interpolates as '') -- spondre's InitInterface-era UpdatePlayerUI
        // reads game.pov.longtermtopics before StartGame assigns pov, and its
        // golden opens with exactly this error. Other unresolvable receivers
        // (dangling object refs) keep the silent-null behaviour the corpus
        // already depends on (The Last Hero's misspelled-room bugs).
        if (ov.type == Value::Type::Null)
            throw std::runtime_error("Property '" + e.str + "' not found on ''");
        Element *el = interp_eval_element(*this, ov);
        if (!el) return vnull();
        const Value *f = resolve_field(el, e.str);
        return f ? *f : vnull();
    }
    case Expr::Kind::Index: {
        Value coll = eval_expr(*e.a, ctx);
        Value idx = eval_expr(*e.b, ctx);
        if (is_list(coll)) {
            long i = (long)std::llround(as_double(idx));
            if (i < 0 || (size_t)i >= coll.list().size()) {
                error("List index out of range");
                return vnull();
            }
            return coll.list()[i];  // entries are typed
        }
        if (coll.type == Value::Type::StringDict ||
            coll.type == Value::Type::ObjectDict) {
            std::string key = to_string(idx);
            for (auto &kv : coll.dict())
                if (kv.first == key) return kv.second;
            error("Dictionary key not found: " + key);
            return vnull();
        }
        error("Cannot index a non-collection");
        return vnull();
    }
    case Expr::Kind::Call: {
        // NCalc's built-in if()/cast() need special argument handling, so they
        // sit before generic evaluation: if() evaluates only the taken branch;
        // cast()'s type argument is a bare identifier (cast(x, int)), not a
        // resolvable variable (the _evaluatingCastType dance in
        // NcalcExpressionEvaluator).
        if (rt_iequals(e.str, "if") && e.args.size() == 3) {
            return truthy(eval_expr(*e.args[0], ctx))
                       ? eval_expr(*e.args[1], ctx)
                       : eval_expr(*e.args[2], ctx);
        }
        if (rt_iequals(e.str, "cast") && e.args.size() == 2) {
            Value v = eval_expr(*e.args[0], ctx);
            std::string ty = (e.args[1]->kind == Expr::Kind::Var)
                                 ? e.args[1]->str
                                 : to_string(eval_expr(*e.args[1], ctx));
            if (rt_iequals(ty, "int") || rt_iequals(ty, "long"))
                return vint((long)as_double(v));   // truncation, like (int)double
            if (rt_iequals(ty, "double") || rt_iequals(ty, "single") ||
                rt_iequals(ty, "decimal"))
                return vdouble(v.type == Value::Type::String
                                   ? c_strtod(v.str.c_str()) : as_double(v));
            if (rt_iequals(ty, "string")) return vstr(to_string(v));
            if (rt_iequals(ty, "boolean") || rt_iequals(ty, "bool"))
                return vbool(truthy(v));
            if (rt_iequals(ty, "object")) return v;
            error("cast(): unknown type '" + ty + "'");
        }
        std::vector<Value> args;
        for (const auto &a : e.args) args.push_back(eval_expr(*a, ctx));
        bool handled = false;
        Value r = call_builtin(e.str, args, handled, ctx);
        if (handled) return r;
        if (world_.find_function(e.str))
            return call_function(e.str, std::move(args), &ctx);
        error("Unknown function '" + e.str + "'");
        return vnull();
    }
    case Expr::Kind::Unary: {
        Value v = eval_expr(*e.a, ctx);
        if (e.str == "-") {
            if (v.type == Value::Type::Int) return vint(-v.integer);
            return vdouble(-as_double(v));
        }
        return vbool(!truthy(v));  // not / !
    }
    case Expr::Kind::Ternary:
        return truthy(eval_expr(*e.a, ctx)) ? eval_expr(*e.b, ctx)
                                            : eval_expr(*e.c, ctx);
    case Expr::Kind::List: {
        // QuestViva yields a bare object[] that only `in`, foreach and
        // ListCount accept (TypeOf/ListContains/indexing error, msg prints
        // "System.Object[]"). Question makes it an ordinary list instead -- an
        // object list when every item is an object, else a value-holding list
        // like NewList() -- a deliberate deviation where the oracle errors.
        bool objects = !e.args.empty();
        std::vector<Value> items;
        for (const ExprP &a : e.args) {
            items.push_back(eval_expr(*a, ctx));
            if (items.back().type != Value::Type::ObjectRef) objects = false;
        }
        Value v = objects ? vobjlist({}) : vstrlist({});
        for (Value &item : items) v.list().push_back(std::move(item));
        return v;
    }
    case Expr::Kind::Binary: {
        const std::string &op = e.str;
        if (op == "and") return vbool(truthy(eval_expr(*e.a, ctx)) &&
                                      truthy(eval_expr(*e.b, ctx)));
        if (op == "or") return vbool(truthy(eval_expr(*e.a, ctx)) ||
                                     truthy(eval_expr(*e.b, ctx)));
        Value l = eval_expr(*e.a, ctx);
        Value r = eval_expr(*e.b, ctx);
        if (op == "xor") return vbool(truthy(l) != truthy(r));
        // NCalc's double-evaluation quirk, ported for side-effect parity
        // (AsyncEvaluationVisitor.Visit(BinaryExpression) + BinaryEventArgs):
        // for the operators QuestViva's EvaluateBinaryAsync intercepts
        // (+ - * / % = <>), the handler evaluates both operands, and when both
        // are "standard" NCalc types (null/number/bool/string) it sets no
        // result, so NCalc's native path evaluates the operands AGAIN --
        // BinaryEventArgs caches with `??=`, so only a NULL operand actually
        // re-runs. Side effects repeat: an erroring ASLX function (which
        // prints its error at its own script boundary and yields null) runs
        // once more per enclosing binary op, doubling each level -- 2^3 = 8
        // error prints for Whitefield's `Grid_Get(..) + a/2.0 - b/2.0 + c`
        // and a single print for a bare call, feeding the 20-error breaker
        // at exactly the oracle's rate.
        auto ncalc_standard = [](const Value &v) {
            switch (v.type) {
            case Value::Type::Null: case Value::Type::Int:
            case Value::Type::Double: case Value::Type::Boolean:
            case Value::Type::String: return true;
            default: return false;
            }
        };
        if ((op == "=" || op == "<>" || op == "+" || op == "-" ||
             op == "*" || op == "/" || op == "%") &&
            ncalc_standard(l) && ncalc_standard(r)) {
            if (l.type == Value::Type::Null) l = eval_expr(*e.a, ctx);
            if (r.type == Value::Type::Null) r = eval_expr(*e.b, ctx);
        }
        if (op == "=") return vbool(values_equal(l, r));
        if (op == "<>") return vbool(!values_equal(l, r));
        // "x in y": list membership, dictionary key lookup, or substring
        // (NCalc's In over QuestList / string; dictionary keys per Quest docs).
        if (op == "in" || op == "not in") {
            bool contains = false;
            if (is_list(r)) {
                for (auto &entry : r.list())
                    if (values_equal(entry, l)) { contains = true; break; }
            } else if (is_dict(r)) {
                std::string key = to_string(l);
                for (auto &kv : r.dict())
                    if (kv.first == key) { contains = true; break; }
            } else if (r.type == Value::Type::String) {
                contains = r.str.find(to_string(l)) != std::string::npos;
            } else {
                errors().push_back(
                    "'in' needs a list, dictionary or string on the right");
            }
            return vbool(op == "in" ? contains : !contains);
        }
        // Quest overloads +, -, * on lists (QuestList<T> operators): list+list
        // merges, list+elem appends, elem+list prepends, list-elem removes the
        // first occurrence, list*list unions. Handled before the scalar/string
        // paths, since list+objectref would otherwise be string-concatenated.
        if (is_list(l) || is_list(r)) {
            if (op == "+" && is_list(l) && is_list(r)) {
                Value out = l; out.detach();
                for (auto &e : r.list()) out.list().push_back(e);
                return out;
            }
            if (op == "+" && is_list(l)) {  // list + elem -> append (boxed)
                Value out = l; out.detach();
                out.list().push_back(r);
                return out;
            }
            if (op == "+") {  // elem + list -> prepend
                Value out = r; out.detach();
                out.list().insert(out.list().begin(), l);
                return out;
            }
            if (op == "-" && is_list(l)) {  // list - elem -> remove first
                Value out = l; out.detach();
                auto &v = out.list();
                for (auto i = v.begin(); i != v.end(); ++i)
                    if (values_equal(*i, r)) { v.erase(i); break; }
                return out;
            }
            if (op == "*" && is_list(l) && is_list(r)) {  // union
                Value out = l; out.detach();
                for (auto &e : r.list()) {
                    bool present = false;
                    for (auto &x : l.list())
                        if (values_equal(x, e)) { present = true; break; }
                    if (!present) out.list().push_back(e);
                }
                return out;
            }
        }
        if (op == "+") {
            if (l.type == Value::Type::String || r.type == Value::Type::String ||
                l.type == Value::Type::ObjectRef || r.type == Value::Type::ObjectRef)
                return vstr(to_string(l) + to_string(r));
            // MathHelper.Add: a null operand (not consumed by string concat
            // above) makes the whole sum null, silently.
            if (l.type == Value::Type::Null || r.type == Value::Type::Null)
                return vnull();
            if (l.type == Value::Type::Int && r.type == Value::Type::Int)
                return vint(l.integer + r.integer);
            return vdouble(as_double(l) + as_double(r));
        }
        // NCalc's Exponentiation: Math.Pow over both operands converted to
        // double, so the result is always a double (it never goes through
        // Quest's MathHelper int fast paths).
        if (op == "^") return vdouble(std::pow(as_double(l), as_double(r)));
        // NCalc's LeftShift/RightShift: Convert.ToUInt64(left) shifted by
        // Convert.ToInt32(right). Oracle-verified: "1 << 33" is 8589934592
        // (64-bit), "2.5 << 1" is 4 (banker's rounding to 2), "1 << \"3\"" is 8,
        // and a negative left operand throws "Value was either too large or
        // too small for a UInt64." The UInt64 result is kept as our Int
        // (TypeOf on it errors in the oracle -- nothing in the corpus asks).
        if (op == "<<" || op == ">>") {
            auto to_u64 = [&](const Value &v) -> unsigned long long {
                double d;
                if (v.type == Value::Type::Int) {
                    if (v.integer < 0)
                        error("Value was either too large or too small for a UInt64.");
                    return (unsigned long long)v.integer;
                }
                if (v.type == Value::Type::Double) d = v.dbl;
                else if (v.type == Value::Type::Boolean) return v.boolean ? 1 : 0;
                else if (v.type == Value::Type::String) {
                    try { d = std::stod(v.str); }
                    catch (...) { error("Input string was not in a correct format."); }
                } else if (v.type == Value::Type::Null) {
                    return 0;  // Convert.ToUInt64(null) is 0
                } else {
                    error("Object must implement IConvertible.");
                }
                d = std::nearbyint(d);  // Convert.ToUInt64(double) rounds to even
                if (d < 0 || d > 18446744073709551615.0)
                    error("Value was either too large or too small for a UInt64.");
                return (unsigned long long)d;
            };
            unsigned long long a = to_u64(l);
            int b = (int)to_u64(r);  // Convert.ToInt32(right); C# masks to 0..63
            unsigned long long out = op == "<<" ? a << (b & 63) : a >> (b & 63);
            return vint((long)out);
        }
        if (op == "-" || op == "*" || op == "%") {
            if (l.type == Value::Type::Null || r.type == Value::Type::Null)
                return vnull();  // MathHelper: null operand -> null result
            bool ints = l.type == Value::Type::Int && r.type == Value::Type::Int;
            double a = as_double(l), b = as_double(r);
            if (op == "-") return ints ? vint(l.integer - r.integer) : vdouble(a - b);
            if (op == "*") return ints ? vint(l.integer * r.integer) : vdouble(a * b);
            if (ints) {
                if (r.integer == 0) error("Attempted to divide by zero.");
                return vint(l.integer % r.integer);
            }
            return vdouble(std::fmod(a, b));
        }
        if (op == "/") {
            if (l.type == Value::Type::Null || r.type == Value::Type::Null)
                return vnull();
            if (l.type == Value::Type::Int && r.type == Value::Type::Int) {
                // HandleBinaryResult's integer-division intercept (FLEE
                // compiled to IL where int/int = int).
                if (r.integer == 0) error("Attempted to divide by zero.");
                return vint(l.integer / r.integer);
            }
            double b = as_double(r);
            return vdouble(b != 0 ? as_double(l) / b : 0);
        }
        // relational: a null on one side only is NCalc's "type conflict" --
        // every comparison except <> is false (TypeHelper.HasNullOrTypeConflict).
        if ((l.type == Value::Type::Null) != (r.type == Value::Type::Null))
            return vbool(false);
        // Booleans take part as 1/0 (NCalc converts to the common numeric
        // type): "true > 0" is True, "true < 2" is True, "false > 0" is False.
        // Deeper's "(player.keyring and (1 << n)) > 0" is exactly that shape.
        auto num_or_bool = [](const Value &v) {
            return is_number(v) || v.type == Value::Type::Boolean;
        };
        if (num_or_bool(l) && num_or_bool(r)) {
            double a = as_double(l), b = as_double(r);
            if (op == "<") return vbool(a < b);
            if (op == ">") return vbool(a > b);
            if (op == "<=") return vbool(a <= b);
            if (op == ">=") return vbool(a >= b);
        } else {
            const std::string &a = l.str, &b = r.str;
            if (op == "<") return vbool(a < b);
            if (op == ">") return vbool(a > b);
            if (op == "<=") return vbool(a <= b);
            if (op == ">=") return vbool(a >= b);
        }
        errors().push_back("Unsupported operator '" + op + "'");
        return vnull();
    }
    }
    return vnull();
}

// -- function calls ---------------------------------------------------------

Value Interp::call_function(const std::string &name, std::vector<Value> args,
                            Context *) {
    // ASLX_TRACE_CALLS=1: stream every function invocation with the current
    // script depth, matching the oracle's QVH_TRACE_CALLS=2 format, to diff
    // depth accounting frame-for-frame (the depth cap decides which sub-calls
    // die during error-cascade unwinds).
    static const bool trace_calls = std::getenv("ASLX_TRACE_CALLS") != nullptr;
    if (trace_calls)
        fprintf(stderr, "[call d%d] %s\n", script_depth_, name.c_str());
    Element *fn = world_.find_function(name);
    if (!fn) {
        error("Function not found: '" + name + "'");
        return vnull();
    }
    if (args.size() == 2 && name == "Grid_CalculateMapCoordinates")
        chart_uncharted_room(args[0], args[1]);
    Context local;
    const Value *pn = fn->field("paramnames");
    // A parameterless call to a function that declares parameters is an error
    // from ASL 520 on (WorldModel.RunProcedureAsync). Quest checks this here
    // rather than letting the body fail on the unbound name, so the message the
    // player sees names the function, not the missing variable — mirror that or
    // the transcript diverges (Fountain of Eternal Youth's bare `LockExit`).
    if (args.empty() && pn && !pn->list().empty() && world_.asl_version >= 520) {
        error("No parameters passed to " + name + " function - expected " +
              std::to_string(pn->list().size()) + " parameters");
        return vnull();
    }
    if (pn) {
        for (size_t i = 0; i < pn->list().size() && i < args.size(); ++i)
            local.locals[to_string(pn->list()[i])] = args[i];
    }
    const Value *body = fn->field("script");
    if (body && body->type == Value::Type::Script) {
        frames_.push_back(name);
        try {
            run_script(body->str, local, scope_key(fn->name, "script"));
        } catch (...) {
            // Only the depth-cap guard throws out of run_script; keep the
            // frame stack balanced on that path.
            frames_.pop_back();
            throw;
        }
        frames_.pop_back();
    }
    return local.return_value;
}

// Quest Viva issue #2189 (open, filed 2026-09-04): CoreGrid only ever assigns
// map coordinates by walking outward from a charted room -- exit.to for each
// VISIBLE exit, plus room.parent -- and seeds exactly one origin, the starting
// room. A room the player reaches any other way has no coordinates by any code
// path: a scripted MovePlayer with no exit pointing at it (The Acreage's
// TerminalRoom), or an exit revealed by MakeExitVisible after the player was
// already standing in its parent (Woo Rebooted). The arrival pass then errors
// out of Grid_GetGridCoordinateForPlayer eight times over (DictionaryItem on
// the empty dictionary Grid_GetPlayerCoordinatesForRoom just created) and
// paints nothing.
//
// Published games carry their own inlined copy of CoreGrid, so an upstream
// library fix would never reach them; this is a deliberate deviation in the
// engine instead (test/quest5/harness/oracle/README.md, "Oracle vs goldens").
// Hooked on the arrival pass -- OnEnterRoom's
// Grid_CalculateMapCoordinates(game.pov.parent, game.pov) -- for a room the
// player's coordinate dictionary does not chart yet: first re-run the pass on
// the room the player came from, so a now-visible exit charts the new room
// where it belongs; if that found nothing, seed the room at the origin of a
// fresh z layer (one above the highest charted), where it stands alone -- the
// map shows one floor at a time -- rather than on top of the starting room.
// Charted rooms and rooms the library seeds itself (the first pass, before
// grid_coordinates exists) take the ordinary path; a game without CoreGrid's
// functions is left alone.
void Interp::chart_uncharted_room(const Value &roomv, const Value &playerv) {
    Element *room = interp_eval_element(*this, roomv);
    Element *player = interp_eval_element(*this, playerv);
    if (!room || !player) return;
    // The arrival pass only: the library recurses on room.parent and calls
    // itself from its own seeding, always for a room it has just charted.
    const Value *pp = resolve_field(player, "parent");
    if (!pp || pp->type != Value::Type::ObjectRef || pp->str != room->name)
        return;
    std::string prev = last_charted_room_;
    last_charted_room_ = room->name;
    if (!resolve_field(player, "grid_coordinates") &&
        !resolve_field(player, "grid_coordinates_delegate"))
        return;  // first pass: Grid_GetPlayerCoordinateDictionary seeds it
    for (const char *f : {"Grid_GetPlayerCoordinateDictionary",
                          "Grid_SetGridCoordinateForPlayer"}) {
        if (!world_.find_function(f)) return;
    }
    // Charted = the room's entry carries all three coordinates (a lookup on
    // the way to an error leaves an EMPTY entry behind).
    auto charted = [&](const std::string &rname) {
        Context c;
        Value d = call_function("Grid_GetPlayerCoordinateDictionary",
                                {playerv}, &c);
        for (const auto &kv : d.dict()) {
            if (kv.first != rname) continue;
            int have = 0;
            for (const auto &c2 : kv.second.dict())
                if (c2.first == "x" || c2.first == "y" || c2.first == "z")
                    ++have;
            return have == 3;
        }
        return false;
    };
    if (charted(room->name)) return;
    if (!prev.empty() && prev != room->name && world_.find(prev) &&
        charted(prev)) {
        Context c;
        call_function("Grid_CalculateMapCoordinates", {vobj(prev), playerv},
                      &c);
        if (charted(room->name)) return;
    }
    double zmax = 0;
    {
        Context c;
        Value d = call_function("Grid_GetPlayerCoordinateDictionary",
                                {playerv}, &c);
        for (const auto &kv : d.dict())
            for (const auto &c2 : kv.second.dict())
                if (c2.first == "z") {
                    double z = c2.second.type == Value::Type::Int
                                   ? (double)c2.second.integer
                                   : c2.second.dbl;
                    if (z + 1 > zmax) zmax = z + 1;
                }
    }
    for (const auto &[axis, v] : {std::pair<const char *, double>{"x", 0.0},
                                  {"y", 0.0}, {"z", zmax}}) {
        Context c;
        call_function("Grid_SetGridCoordinateForPlayer",
                      {playerv, roomv, vstr(axis), vdouble(v)}, &c);
    }
}

// -- reserved statement commands --------------------------------------------

// Resolve an lvalue expression (a local variable or obj.attr) to the mutable
// Value backing it, creating a local if needed. Used by list/dictionary
// mutators, which change the collection in place. Returns nullptr for anything
// that is not an assignable location.
Value *Interp::lvalue_of(const Expr &e, Context &ctx) {
    if (e.kind == Expr::Kind::Var) {
        return &ctx.locals[e.str];
    }
    if (e.kind == Expr::Kind::Member) {
        Value ov = eval_expr(*e.a, ctx);
        Element *el = (ov.type == Value::Type::ObjectRef) ? world_.find(ov.str)
                                                          : nullptr;
        if (!el) return nullptr;
        // Own field if present; else copy the inherited value down so the
        // mutation lands on this element (copy-on-write), matching how Quest's
        // list/dictionary attributes become instance-owned once modified.
        if (const Value *own = el->field(e.str))
            return const_cast<Value *>(own);
        if (const Value *inh = resolve_field(el, e.str)) {
            // The copy-down creates an own attribute: log it as an added
            // field set so a rollback removes it (back to the inherited one).
            return &set_field_logged(el, e.str, *inh);
        }
        return &set_field_logged(el, e.str, Value{});
    }
    return nullptr;
}

// The grid-map paint vocabulary (CoreGrid.aslx -> grid.js), forwarded to the
// grid_draw hook as GridDraw commands; `args` are the call's evaluated
// arguments. Guarding numbers through as_double keeps a game that passes junk
// from crashing the bridge (grid.js would have silently drawn NaNs). Returns
// false, having done nothing, for a name that is not part of the vocabulary.
bool Interp::exec_grid_command(const std::string &fn,
                               const std::vector<Value> &args) {
    auto arg = [&](size_t i) { return i < args.size() ? args[i] : vnull(); };
    auto num = [&](size_t i) { return as_double(arg(i)); };
    auto str = [&](size_t i) { return to_string(arg(i)); };
    GridDraw g;
    if (fn == "ShowGrid") {
        g.op = GridDraw::Op::Show;
        g.h = num(0);
        grid_draw(g);
    } else if (fn == "Grid_SetScale") {
        g.op = GridDraw::Op::Scale;
        g.w = num(0);
        grid_draw(g);
    } else if (fn == "Grid_DrawBox") {
        g.op = GridDraw::Op::Box;
        g.x = num(0); g.y = num(1); g.z = (int)num(2);
        g.w = num(3); g.h = num(4);
        g.border = str(5);
        g.borderwidth = (int)num(6);
        g.fill = str(7);
        g.sides = (int)num(8);
        grid_draw(g);
    } else if (fn == "Grid_DrawLabel") {
        g.op = GridDraw::Op::Label;
        g.x = num(0); g.y = num(1); g.z = (int)num(2);
        g.text = str(3);
        g.fill = args.size() > 4 ? str(4) : "black";
        grid_draw(g);
    } else if (fn == "Grid_DrawLine") {
        g.op = GridDraw::Op::Line;
        g.x = num(0); g.y = num(1); g.x2 = num(2); g.y2 = num(3);
        g.border = str(4);
        g.borderwidth = (int)num(5);
        grid_draw(g);
    } else if (fn == "Grid_DrawPlayer") {
        g.op = GridDraw::Op::Player;
        g.x = num(0); g.y = num(1); g.z = (int)num(2);
        g.w = num(3);
        g.border = str(4);
        g.borderwidth = (int)num(5);
        g.fill = str(6);
        grid_draw(g);
    } else if (fn == "Grid_ClearAllLayers") {
        g.op = GridDraw::Op::Clear;
        grid_draw(g);
    } else if (fn == "setBackground" && !args.empty()) {
        /* SetBackgroundColour: the reference player tints the whole game
         * panel AND #gridPanel with it, and grid colours are authored
         * against that -- a game on a black background draws its exit lines
         * in white.  Only the map pane takes it here (the text window keeps
         * the Glk theme), so it rides the grid channel. */
        g.op = GridDraw::Op::Canvas;
        g.fill = str(0);
        grid_draw(g);
    } else {
        return false;
    }
    return true;
}

// JS.* -- the front-end bridge. Only JS.addText carries game text; route it
// to the output sink. JS.setPanelContents is the picture frame
// (SetFramePicture/ClearFramePicture wrap it, and OnEnterRoom sets it from
// the room's `picture` attribute) -- routed to the set_panel_contents host
// hook when one is installed, dropped otherwise.
// Everything else is a UI side effect we can ignore in the headless/native
// core (a later presentation milestone wires the rest).
void Interp::exec_js_command(const std::string &fn, const ExprList &args,
                             Context &ctx) {
    /* A JS.* call is an ordinary FunctionCallScript to QuestViva: it
     * evaluates EVERY argument, left to right, before it even looks at the
     * name -- so an argument that fails reports a script error whether or
     * not anything is listening at the other end. Cache them here and read
     * the cache below (through `ev`), so each argument is evaluated exactly
     * once, in order, on every path including the ones we ignore.
     *
     * Evaluating lazily instead -- only the arguments a handled case
     * actually reads -- silently dropped three "The given key 'x' was not
     * present in the dictionary" reports from The Acreage, whose CoreGrid
     * map paints an uncharted room: JS.Grid_DrawBox's first three
     * arguments each call Grid_GetGridCoordinateForPlayer on coordinates
     * the room does not have yet. See the `picture` note below for the
     * same rule on a statement we do implement. */
    std::vector<Value> jsargs;
    jsargs.reserve(args.size());
    for (const auto &a : args)
        jsargs.push_back(eval_expr(*a, ctx));
    auto ev = [&](size_t i) -> Value {
        return i < jsargs.size() ? jsargs[i] : vnull();
    };
    /* Hand an unimplemented JS.* call's last argument to the host, which
     * decides whether it names a game function to fire as an ASLEvent. */
    auto js_fallback = [&] {
        /* Zero-argument calls reach the host too, with an empty argument:
         * the name alone can be the signal (JS.HookClicks installs the
         * reference player's click-anywhere handler). */
        if (js_event_bridge)
            js_event_bridge(fn, args.empty() ? std::string()
                                             : to_string(ev(args.size() - 1)));
    };
    if (fn == "addText" && !args.empty())
        print(to_string(ev(0)));
    else if (fn == "setPanelContents" && !args.empty() && set_panel_contents)
        set_panel_contents(to_string(ev(0)));
    else if (fn == "updateStatus" && !args.empty() && update_status)
        update_status(to_string(ev(0)));
    else if (fn == "updateLocation" && !args.empty() && update_location)
        update_location(to_string(ev(0)));
    else if (fn == "disableAllCommandLinks" && disable_command_links)
        disable_command_links();
    else if (fn == "clearScreen" && clear_screen)
        // Core's ClearScreen (playercore.js clearScreen: wipe the
        // transcript, keep the panes and the picture frame).
        clear_screen();
    else if (fn == "panesVisible" && !args.empty() && panes_visible)
        panes_visible(truthy(ev(0)));
    else if ((fn == "TextFX.Typewriter" || fn == "TextFX.Unscramble") &&
             !args.empty() && textfx_text) {
        // playercore.js addFx: a styled span (with a trailing space) plus
        // a line break; the animation then fills the span with the text.
        // Args are (text, speed[, reveal], font, color, size) -- the
        // style triple sits at the tail either way.
        std::string html = "<span";
        if (args.size() >= 4)
            html += " style=\"font-family:" +
                    to_string(ev(args.size() - 3)) + ";color:" +
                    to_string(ev(args.size() - 2)) + ";font-size:" +
                    to_string(ev(args.size() - 1)) + "pt\"";
        html += ">" + to_string(ev(0)) + " </span><br/>";
        textfx_text(html);
    }
    else if (fn == "StartOutputSection" && !args.empty() &&
             start_output_section)
        start_output_section(to_string(ev(0)));
    else if (fn == "EndOutputSection" && !args.empty() &&
             end_output_section)
        end_output_section(to_string(ev(0)));
    else if (fn == "HideOutputSection" && !args.empty() &&
             hide_output_section)
        hide_output_section(to_string(ev(0)));
    else if ((fn == "uiShow" || fn == "uiHide") && !args.empty() &&
             (show_command_bar || panes_visible)) {
        // The command box and the panes (playercore.js uiShow/uiHide
        // special-case "#gamePanes" through panesVisible); the other ids
        // this channel carries ("#location") are pure layout in the
        // reference player's DOM and mean nothing here.
        std::string id = to_string(ev(0));
        if (id == "#txtCommandDiv" && show_command_bar)
            show_command_bar(fn == "uiShow");
        else if (id == "#gamePanes" && panes_visible)
            panes_visible(fn == "uiShow");
    }
    else if (fn == "eval" && !args.empty() && (request_restart || js_eval)) {
        /* The restart channel: Core's `restart` command evals
         * "window.location.reload();" (older Cores first probe the
         * desktop player's RestartGame()).  That reload IS the restart
         * -- route it to the host.  Everything else this eval channel
         * carries (transcript flags, jQuery pane tweaks) goes to js_eval
         * when the host has one, and is ignored otherwise. */
        std::string js = to_string(ev(0));
        if (js.find("location.reload") != std::string::npos ||
            js.find("RestartGame") != std::string::npos) {
            if (request_restart)
                request_restart();
        } else if (js_eval) {
            js_eval(js);
        }
    } else if (!grid_draw || !exec_grid_command(fn, jsargs)) {
        /* Not ours and not the grid map's (or no grid bridge at all):
         * everything unhandled goes to the JS callback bridge (see the hook's
         * note). */
        js_fallback();
    }
}

// play sound (file, synchronous, loop) -- PlaySoundScript.ExecuteAsync
// evaluates all three in that order, then hands them to the UI.
void Interp::exec_play_sound(const ExprList &args, Context &ctx) {
    auto ev = [&](size_t i) { return eval_arg(args, i, ctx); };
    std::string filename = !args.empty() ? to_string(ev(0)) : "";
    bool sync = args.size() >= 2 && truthy(ev(1));
    bool loop = args.size() >= 3 && truthy(ev(2));
    if (sync) {
        // A synchronous play claims the wait slot (BeginPrompt on
        // _waitTcs): a previously parked sync sound resumes inline; a
        // pending `wait` is cancelled -- its callback never runs. Note no
        // begin_pending_callback -- PlaySoundScript never counts itself,
        // so `on ready` is not deferred by the sound.
        claim_wait_slot();
    }
    if (play_sound) {
        // The host plays it; a synchronous host BLOCKS in the hook until
        // playback finishes, so the rest of the turn resumes exactly
        // where QuestViva's awaited wait slot would resume it.
        play_sound(filename, sync, loop);
        return;
    }
    warn_once("play sound", "'play sound' is not supported yet; ignored");
    if (sync) {
        // With no UI to report the sound finished, everything after this
        // statement parks on the wait slot: the unwind captures the
        // remainder as TurnSuspended frames, stored at the turn boundary
        // and resumed when the slot is next claimed (the next `wait` or
        // sync sound, or a host finish_wait) -- see resume_parked_tail.
        throw TurnSuspended{};
    }
}

// request (RequestType, data): a player-UI request (RequestScript). The
// first argument is a bare enum identifier (Speak, Quit, Show, ...) that is
// not a resolvable expression, so we must NOT evaluate the args -- headless
// ignores UI requests (a later presentation milestone wires the handful Core
// relies on). Same for `request` with a callback and RequestSave.
void Interp::exec_request(const std::string &name, const ExprList &args,
                          Context &ctx) {
    auto ev = [&](size_t i) { return eval_arg(args, i, ctx); };
    // The request's first arg is a bare enum identifier (never evaluated).
    // RequestSave is the one request with engine-side meaning: Core's
    // `save` command runs `request (RequestSave, "")` and the UI is
    // expected to capture + persist the game (PlayerUI.RequestSave). The
    // newer standalone `requestsave` command is the same request.
    std::string req =
        name == "requestsave"
            ? "RequestSave"
            : (!args.empty() && args[0]->kind == Expr::Kind::Var
                   ? args[0]->str : "");
    if (req == "Quit") {
        // RequestScript's Quit case: PlayerUi.Quit() (a headless no-op)
        // then WorldModel.Finish() -> FinishGame(). FinishGame sets
        // State=Finished FIRST, then TrySetCanceled()s the pending TCS --
        // which resumes any parked synchronous `play sound` continuation
        // inline (its remaining statements print, but Core's turnscripts
        // and pane refresh no-op now that the game is finished). Match that
        // order so a parked tail is flushed exactly as QuestViva flushes it.
        world_.finished = true;
        resume_parked_tail();
    } else if (req == "RequestSave") {
        if (request_save)
            request_save();
        else
            warn_once("requestsave", "Saving is not supported here.");
    } else if (req == "SetStatus" && update_status) {
        // The pre-JS status channel: games embedding an older Core send
        // `request (SetStatus, text)` (PlayerUI.SetStatusText) where the
        // modern one calls JS.updateStatus. The data joins lines with
        // real newlines ("\n" string escapes); normalise to <br/> so the
        // hook sees the JS contract. Data is only evaluated when a hook
        // will consume it (RequestScript always evaluates; headless
        // parity keeps the old ignore-unevaluated behaviour).
        std::string data = to_string(ev(1)), html;
        for (char c : data) {
            if (c == '\n') html += "<br/>";
            else html += c;
        }
        update_status(html);
    } else if (req == "ClearScreen" && clear_screen) {
        // RequestScript's ClearScreen case: PlayerUi.ClearScreen() -- the
        // pre-JS pairing for JS.clearScreen.
        clear_screen();
    } else if (req == "PanesVisible" && panes_visible) {
        // PlayerUI.SetPanesVisible(data) -- the pre-JS pairing for
        // JS.panesVisible; the data is the string "on" or "off"
        // (Player.cs: panesVisible(data == "on")).
        panes_visible(to_string(ev(1)) == "on");
    } else if (req == "UpdateLocation" && update_location) {
        // PlayerUI.LocationUpdated -- same pre-JS pairing as SetStatus.
        update_location(to_string(ev(1)));
    } else if (req == "SetPanelContents" && set_panel_contents) {
        // PlayerUI.SetPanelContents -- the picture frame, like
        // JS.setPanelContents.
        set_panel_contents(to_string(ev(1)));
    } else if (req == "RunScript" && set_panel_contents) {
        // PlayerUI.RunScript(data) -- Quest 5.0's "call a function in the
        // player's HTML frame" channel, data being "name; arg". Quest 5.0
        // games carry their own SetFramePicture built on it (Nearco II:
        // `request (RunScript, "setFramePicture; " + GetFileURL(f))`,
        // driving the Frame.htm/Frame.js pair bundled in the package),
        // which is the same picture frame later Cores reach through
        // JS.setPanelContents -- so route those two verbs to the same
        // hook. Every other function this channel can name is real
        // JavaScript we cannot run, and stays ignored.
        std::string data = to_string(ev(1));
        size_t semi = data.find(';');
        std::string fn = rt_trim(data.substr(0, semi)), arg;
        if (semi != std::string::npos) arg = rt_trim(data.substr(semi + 1));
        if (fn == "setFramePicture" && !arg.empty())
            set_panel_contents("<img src=\"" + arg + "\"/>");
        else if (fn == "clearFramePicture")
            set_panel_contents("");
    } else if (req == "Background" && grid_draw) {
        // PlayerUI.SetBackground -- the pre-JS pairing for
        // JS.setBackground, sent by games that embed a Quest 5.0-era Core
        // (Dream Pieces 2). Same meaning: it is the canvas the grid map's
        // colours were authored against. See the JS.setBackground case.
        GridDraw g;
        g.op = GridDraw::Op::Canvas;
        g.fill = to_string(ev(1));
        grid_draw(g);
    } else if ((req == "Show" || req == "Hide") &&
               (show_command_bar || panes_visible)) {
        // PlayerUI.Show/Hide -- the element-visibility channel, the pre-JS
        // pairing for JS.uiShow/uiHide. Data is an element name ("Panes",
        // "Command", "Location"); the command box and the panes mean
        // something outside a DOM ("Location" is pure layout).
        std::string el = to_string(ev(1));
        if (el == "Command" && show_command_bar)
            show_command_bar(req == "Show");
        else if (el == "Panes" && panes_visible)
            panes_visible(req == "Show");
    } else if (req == "Wait") {
        // RequestScript Wait -> DoWaitAsync: the pre-JS "press any key"
        // prompt (Core's WaitForKeyPress). Valid only pre-v540 -- v540+
        // throws (games use the `wait` script command instead). It claims
        // the wait slot (BeginPrompt on _waitTcs): a parked synchronous
        // `play sound` resumes inline HERE and a pending `wait` callback is
        // cancelled. A synchronous host then BLOCKS in do_wait until the
        // keypress and the enclosing script resumes inline; headless it is a
        // silent no-op (do_wait's doc explains why that stays oracle-exact).
        if (world_.asl_version >= 540)
            throw std::runtime_error(
                "The 'Wait' request is not supported for games written for "
                "Quest 5.4 or later. Use the 'wait' script command "
                "instead.");
        claim_wait_slot();
        if (do_wait) do_wait();
    } else if (req == "Pause") {
        // RequestScript Pause -> DoPauseAsync (Core's Pause function).
        // Valid only pre-v550 -- v550+ throws (games use SetTimeout). The
        // data is int.TryParse'd; a non-numeric string is ignored (no
        // pause), matching QuestViva. Pause uses a SEPARATE slot (_pauseTcs)
        // so it leaves any parked sync sound on the wait slot untouched. A
        // synchronous host BLOCKS in do_pause for the interval; headless it
        // is a silent no-op.
        if (world_.asl_version >= 550)
            throw std::runtime_error(
                "The 'Pause' request is not supported for games written "
                "for Quest 5.5 or later. Use the 'SetTimeout' function "
                "instead.");
        long ms = 0;
        if (parse_int32_text(to_string(ev(1)), ms) == IntParse::Ok && do_pause)
            do_pause((int)ms);
    }
}

// CreateExitScript / ObjectFactory.CreateExit.
void Interp::exec_create_exit(const ExprList &args, Context &ctx) {
    auto ev = [&](size_t i) { return eval_arg(args, i, ctx); };
    // 3 args: (alias, from, to); 4: (alias, from, to, initialType);
    // 5: (id, alias, from, to, initialType). No id -> a generated
    // "exitN" name and the anonymous flag.
    size_t base = args.size() >= 5 ? 1 : 0;
    Value alias = ev(base);
    Value from = ev(base + 1), to = ev(base + 2);
    std::string type =
        args.size() >= 4 ? to_string(ev(base + 3)) : std::string();
    std::string id = args.size() >= 5 ? to_string(ev(0)) : std::string();
    bool anonymous = id.empty();
    if (anonymous) {
        int k = 0;
        do { id = "exit" + std::to_string(++k); } while (world_.find(id));
    }
    Element *exit = world_.create_object(id, type, "exit");
    log_create(exit);
    // `newExit.Fields[Alias] = exitName` goes through Fields.Set, so a
    // null alias is REMOVED at v530+ (the initial type's alias -- "west"
    // from westdirection -- shows through) and stored as an own null
    // before that. Deeper's `create exit (name, null, room, room_west,
    // "westdirection")` relies on the inherited alias for GetExitByName.
    if (alias.type == Value::Type::Null) {
        if (world_.asl_version < 530) exit->set_field("alias", vnull());
    } else {
        exit->set_field("alias", vstr(to_string(alias)));
    }
    if (from.type == Value::Type::ObjectRef)
        exit->set_field("parent", from);
    exit->set_field("to", to);
    if (anonymous) exit->set_field("anonymous", vbool(true));
}

// list add / list remove (list, item).
void Interp::exec_list_command(bool add, const ExprList &args, Context &ctx) {
    auto ev = [&](size_t i) { return eval_arg(args, i, ctx); };
    if (args.size() < 2) return;
    Value *lst = lvalue_of(*args[0], ctx);
    // QuestViva's ListAddScript evaluates its target as an EXPRESSION and
    // mutates the QuestList reference it yields -- the target need not be
    // an assignable name at all (spondre: `list add (groups[class], entry)`).
    // When it isn't an lvalue, evaluate it; the copy aliases list_store,
    // so mutating through it edits the stored list.
    Value lst_expr;
    if (!lst) {
        lst_expr = ev(0);
        if (is_list(lst_expr)) lst = &lst_expr;
    }
    // Copy the target onto the stack before evaluating the item: ev(1) runs
    // arbitrary game script that may add or remove an attribute on the
    // element `lst` points into, reallocating its fields vector and leaving
    // `lst` dangling. A Value copy shares list_store (reference semantics),
    // so the add/remove and its undo record still hit the stored list.
    Value lst_hold;
    if (lst) { lst_hold = *lst; lst = &lst_hold; }
    Value item = ev(1);
    if (!lst || !is_list(*lst)) {
        errors().push_back("Unrecognised list type");
        return;
    }
    if (add) {
        // The value is stored boxed/typed verbatim (QuestList<object>.Add)
        // -- a list can hold dictionaries, objects, numbers... (spondre).
        auto &v = lst->list();
        log_list_change(UndoAction::Kind::ListAdd, *lst, (long)v.size(), item);
        v.push_back(std::move(item));
    } else {
        // QuestList.Remove: first occurrence only (List<T>.Remove).
        auto &v = lst->list();
        for (auto i = v.begin(); i != v.end(); ++i)
            if (values_equal(*i, item)) {
                log_list_change(UndoAction::Kind::ListRemove, *lst,
                                (long)(i - v.begin()), *i);
                v.erase(i);
                break;
            }
    }
}

// dictionary add (dictionary, key, value) / dictionary remove (dictionary,
// key).
void Interp::exec_dictionary_command(bool add, const ExprList &args,
                                     Context &ctx) {
    auto ev = [&](size_t i) { return eval_arg(args, i, ctx); };
    if (args.size() < 2) return;
    Value *d = lvalue_of(*args[0], ctx);
    // Same expression-target fallback as `list add` above (QuestViva
    // mutates whatever QuestDictionary the expression yields).
    Value d_expr;
    if (!d) {
        d_expr = ev(0);
        if (is_dict(d_expr)) d = &d_expr;
    }
    // Copy the target onto the stack before evaluating the key/value: the
    // ev(1) key and the later ev(2) value run game script that may realloc
    // the element's fields vector and dangle `d`. The copy shares dict_store
    // (reference semantics), so mutations and undo records still hit the
    // stored dictionary.
    Value d_hold;
    if (d) { d_hold = *d; d = &d_hold; }
    std::string key = to_string(ev(1));
    if (!d || !is_dict(*d)) {
        errors().push_back("Unrecognised dictionary type");
        return;
    }
    // QuestDictionary.Add throws on a duplicate key (DictionaryAddScript
    // calls IDictionary.Add directly) -- The Zen Garden defines the
    // `touch` verb twice and its golden opens with exactly this error
    // from Core's InitVerbsList. Only `dictionary add` throws; the
    // remove path below still just erases.
    if (add) {
        for (auto &kv : d->dict())
            if (kv.first == key)
                error("Error adding key '" + key + "' to dictionary: "
                      "An item with the same key has already been added. "
                      "Key: " + key);
    }
    // remove any existing entry with this key first (Add replaces).
    for (auto it = d->dict().begin(); it != d->dict().end();) {
        if (it->first == key) {
            log_dict_change(UndoAction::Kind::DictRemove, *d,
                            (long)(it - d->dict().begin()), key, it->second);
            it = d->dict().erase(it);
        } else {
            ++it;
        }
    }
    if (add) {
        log_dict_change(UndoAction::Kind::DictAdd, *d,
                        (long)d->dict().size(), key, Value{});
        d->dict().emplace_back(key, ev(2));  // store the typed value verbatim
    }
}

// One argument of a statement command, evaluated on demand; a missing one is
// null.
Value Interp::eval_arg(const ExprList &args, size_t i, Context &ctx) {
    return i < args.size() ? eval_expr(*args[i], ctx) : vnull();
}

bool Interp::exec_statement_command(const std::string &name,
                                    const ExprList &args, Context &ctx) {
    auto ev = [&](size_t i) { return eval_arg(args, i, ctx); };
    auto as_element = [&](const Value &v) -> Element * {
        return v.type == Value::Type::ObjectRef ? world_.find(v.str) : nullptr;
    };

    if (name.compare(0, 3, "JS.") == 0) {
        exec_js_command(name.substr(3), args, ctx);
        return true;
    }

    // Media/output-decoration commands (insert splices an HTML file into the
    // output): pure presentation, no world-model effect. Unsupported ones
    // no-op with a one-time warning so a game using them still runs headless
    // (insert's arg is deliberately not evaluated -- nothing observable
    // should happen). `picture` is different: PictureScript ALWAYS evaluates
    // its filename first (so an erroring expression reports like the
    // oracle's), then prints an <img> on v540+ or shows the picture through
    // the UI (ShowPictureAsync) pre-540 -- the latter lands in the
    // show_picture host hook when the front-end can draw it.
    if (name == "picture" || name == "insert") {
        if (name != "picture" || !show_picture)
            warn_once(name, "'" + name + "' is not supported yet; ignored");
        if (name == "picture" && !args.empty()) {
            std::string filename = to_string(ev(0));
            if (world_.asl_version >= 540) {
                // The <img> print's <br/> suffix (Core OutputText) is why
                // oracle transcripts show a blank line here. The URL is the
                // filename headless (IPlayer.GetUrlAsync); a Glk host's
                // render_html maps the tag onto a real image.
                print_via_core("<img src=\"" + filename + "\" />", ctx);
            } else if (show_picture) {
                show_picture(filename);
            }
            return true;
        }
        return true;
    }

    if (name == "play sound") {
        exec_play_sound(args, ctx);
        return true;
    }

    if (name == "stop sound") {   // StopSoundScript: no args
        if (stop_sound)
            stop_sound();
        else
            warn_once(name, "'" + name + "' is not supported yet; ignored");
        return true;
    }

    if (name == "request" || name == "requestsave") {
        exec_request(name, args, ctx);
        return true;
    }

    // The optional parameter dictionary of `do` / `invoke` (argument `i`)
    // becomes the locals of the script about to run.
    auto bind_params = [&](size_t i, Context &local) {
        if (i >= args.size()) return;
        Value params = ev(i);
        for (auto &kv : params.dict())
            local.locals[kv.first] = kv.second;  // values are typed per-entry
    };
    if (name == "do") {
        Element *obj = as_element(ev(0));
        std::string action = to_string(ev(1));
        if (!obj) { errors().push_back("do: not an object"); return true; }
        const Value *scr = resolve_field(obj, action);
        if (!scr || scr->type != Value::Type::Script) {
            errors().push_back("do: no script '" + action + "'");
            return true;
        }
        // Copy the script text out before evaluating the params: ev(2) runs game
        // script that may add a field to `obj`, reallocating its fields vector
        // and dangling the `scr` pointer resolve_field returned into it.
        std::string script = scr->str;
        // The firsttime scope is the attribute the script was read from
        // (resolved now, for the same dangling reason).
        std::string scope = field_scope(obj, action);
        Context local;
        bind_params(2, local);
        // DoScript passes the object as thisElement (WorldModel.RunScriptAsync
        // binds it as the "this" parameter).
        local.locals["this"] = vobj(obj->name);
        run_script(script, local, scope);
        return true;
    }
    if (name == "invoke") {
        Value scr = ev(0);
        if (scr.type != Value::Type::Script) {
            errors().push_back("invoke: not a script");
            return true;
        }
        Context local;
        bind_params(1, local);
        run_script(scr.str, local);
        return true;
    }
    if (name == "create") {
        std::string nm = to_string(ev(0));
        std::string ty = args.size() >= 2 ? to_string(ev(1)) : "";
        log_create(world_.create_object(nm, ty));
        return true;
    }
    if (name == "rundelegate") {  // RunDelegateScript
        std::vector<Value> params;
        for (size_t i = 2; i < args.size(); ++i) params.push_back(ev(i));
        interp_run_delegate(*this, as_element(ev(0)), to_string(ev(1)), params);
        return true;
    }
    if (name == "create timer") {  // CreateTimerScript
        log_create(world_.create_object(to_string(ev(0)), "", "timer"));
        return true;
    }
    if (name == "create exit") {
        exec_create_exit(args, ctx);
        return true;
    }
    if (name == "create turnscript") {  // CreateTurnScript
        log_create(world_.create_object(to_string(ev(0)), "", "turnscript"));
        return true;
    }
    if (name == "destroy") {
        std::string nm = to_string(ev(0));
        if (Element *el = world_.find(nm))
            log_destroy(el);
        world_.destroy_element(nm);
        return true;
    }
    if (name == "set") {  // set(obj, "field", value)
        Element *obj = as_element(ev(0));
        if (!obj) {
            errors().push_back("set: not an object");
            return true;
        }
        std::string attr = to_string(ev(1));
        assign_field(obj, attr, ev(2));
        return true;
    }
    if (name == "list add" || name == "list remove") {
        exec_list_command(name == "list add", args, ctx);
        return true;
    }
    if (name == "dictionary add" || name == "dictionary remove") {
        exec_dictionary_command(name == "dictionary add", args, ctx);
        return true;
    }
    if (name == "error") {
        // ErrorScript throws; the nearest script boundary reports it.
        error(to_string(ev(0)));
    }
    if (name == "finish") {
        world_.finished = true;
        return true;
    }
    if (name == "undo") {
        // UndoScript: one `undo` = one RollbackTransaction. Core's undo
        // command is <isundo/> so it never opened its own transaction; what
        // gets committed-and-rolled-back here is the previous command's.
        rollback_transaction(ctx);
        return true;
    }
    if (name == "start transaction") {
        // UndoScript (start transaction): commit the open transaction, open a
        // new one labelled with the command text (CoreParser passes
        // game.pov.currentcommand).
        roll_transaction(args.empty() ? std::string() : to_string(ev(0)));
        return true;
    }
    return false;
}

}  // namespace aslx
