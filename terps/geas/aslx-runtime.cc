// aslx-runtime.cc -- Quest 5 script + expression evaluator. See aslx-runtime.hh.

#include "aslx-runtime.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace aslx {

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

long Rng::between(long lo, long hi) {
    if (hi <= lo) return lo;
    unsigned long span = (unsigned long)(hi - lo + 1);
    return lo + (long)(next() % span);
}

double Rng::next_double() { return next() * (1.0 / 4294967296.0); }

// ===========================================================================
// Value constructors + coercions
// ===========================================================================

static Value vnull() { Value v; v.type = Value::Type::Null; return v; }
static Value vstr(std::string s) {
    Value v; v.type = Value::Type::String; v.str = std::move(s); return v;
}
static Value vint(long i) { Value v; v.type = Value::Type::Int; v.integer = i; return v; }
static Value vdouble(double d) { Value v; v.type = Value::Type::Double; v.dbl = d; return v; }
static Value vbool(bool b) { Value v; v.type = Value::Type::Boolean; v.boolean = b; return v; }
static Value vobj(std::string name) {
    Value v; v.type = Value::Type::ObjectRef; v.str = std::move(name); return v;
}
static Value vstrlist(std::vector<std::string> l) {
    Value v; v.type = Value::Type::StringList; v.list = std::move(l); return v;
}
static Value vobjlist(std::vector<std::string> l) {
    Value v; v.type = Value::Type::ObjectList; v.list = std::move(l); return v;
}

static bool is_number(const Value &v) {
    return v.type == Value::Type::Int || v.type == Value::Type::Double;
}
static double as_double(const Value &v) {
    if (v.type == Value::Type::Int) return (double)v.integer;
    if (v.type == Value::Type::Double) return v.dbl;
    if (v.type == Value::Type::Boolean) return v.boolean ? 1 : 0;
    return 0;
}
static bool is_list(const Value &v) {
    return v.type == Value::Type::StringList || v.type == Value::Type::ObjectList;
}

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
        for (size_t i = 0; i < v.list.size(); ++i) {
            if (i) s += ", ";
            s += v.list[i];
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

// ===========================================================================
// Tokenizer helpers, mirroring QuestViva Utility.cs
// ===========================================================================

static std::string rt_trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

// Replace the contents of each "..." string with dashes, keeping the quotes and
// everything outside strings unchanged, so structural scans ignore string bodies.
static std::string obscure_strings(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    bool inq = false;
    for (char c : in) {
        if (c == '"') { out += '"'; inq = !inq; }
        else out += inq ? '-' : c;
    }
    return out;
}

// Extract the text between the first `open` and its matching `close`. Returns
// the inside; sets `after` to whatever follows the close. Empty string if none.
static std::string extract_balanced(const std::string &text, char open, char close,
                                    std::string &after, bool &found) {
    found = false;
    after.clear();
    std::string ob = obscure_strings(text);
    size_t start = ob.find(open);
    if (start == std::string::npos) return "";
    int depth = 1;
    size_t pos = start;
    while (true) {
        ++pos;
        if (pos >= ob.size()) break;
        if (ob[pos] == open) ++depth;
        else if (ob[pos] == close) --depth;
        if (depth == 0) break;
    }
    if (depth != 0) return "";  // unbalanced
    found = true;
    after = text.substr(pos + 1);
    return text.substr(start + 1, pos - start - 1);
}

static std::string get_parameter(const std::string &script, std::string &after,
                                 bool &found) {
    return extract_balanced(script, '(', ')', after, found);
}

// Split a parameter list on top-level commas, respecting strings and nested
// parens; backslash escapes the next char. Mirrors Utility.SplitParameter.
static std::vector<std::string> split_parameters(const std::string &text) {
    std::vector<std::string> result;
    bool inq = false, process_next = true;
    int brackets = 0;
    std::string cur;
    for (char c : text) {
        bool process = process_next;
        process_next = true;
        if (process) {
            if (c == '\\') { process_next = false; }
            else if (c == '"') { inq = !inq; }
            else if (!inq) {
                if (c == '(') ++brackets;
                if (c == ')') brackets = std::max(0, brackets - 1);
                if (brackets == 0 && c == ',') {
                    result.push_back(rt_trim(cur));
                    cur.clear();
                    continue;
                }
            }
        }
        cur += c;
    }
    result.push_back(rt_trim(cur));
    return result;
}

// Pull the next statement off the front of `script`. A statement runs to the
// next newline, unless a `{` opens first, in which case it is `head { block }`.
static std::string get_script(const std::string &script, std::string &after) {
    after.clear();
    std::string ob = obscure_strings(script);
    size_t brace = ob.find('{');
    size_t nl = ob.find('\n');
    size_t comment = ob.find("//");
    if (nl == std::string::npos) return script;
    if (brace == std::string::npos || nl < brace ||
        (comment != std::string::npos && comment < brace && comment < nl)) {
        after = script.substr(nl + 1);
        return script.substr(0, nl);
    }
    std::string before = script.substr(0, brace);
    bool found;
    std::string inside = extract_balanced(script, '{', '}', after, found);
    if (inside.find('\n') != std::string::npos)
        return before + "{" + inside + "}";
    return before + inside;
}

static std::string remove_surrounding_braces(const std::string &input) {
    std::string s = rt_trim(input);
    if (s.size() >= 2 && s.front() == '{' && s.back() == '}')
        return s.substr(1, s.size() - 2);
    return s;
}

// Strip // line comments, respecting string literals.
static std::string remove_comments(const std::string &input) {
    std::string out;
    bool inq = false;
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '"') { inq = !inq; out += c; continue; }
        if (!inq && c == '/' && i + 1 < input.size() && input[i + 1] == '/') {
            while (i < input.size() && input[i] != '\n') ++i;
            if (i < input.size()) out += '\n';
            continue;
        }
        out += c;
    }
    return out;
}

// ===========================================================================
// Expression AST + parser
// ===========================================================================

struct Expr {
    enum class Kind {
        Num, Str, Bool, Null, Var, Member, Index, Call, Unary, Binary, Ternary
    };
    Kind kind;
    // literals
    double num = 0; bool is_int = false;
    std::string str;      // Str / Var name / Member name / Call name / op
    bool boolean = false;
    // children
    std::shared_ptr<Expr> a, b, c;
    std::vector<std::shared_ptr<Expr>> args;
};

using ExprP = std::shared_ptr<Expr>;

namespace {

struct Tok {
    enum class T { Num, Str, Ident, Op, End } t;
    std::string s;
    double num = 0; bool is_int = false;
};

struct Lexer {
    const std::string &src;
    size_t i = 0;
    std::vector<Tok> toks;

    explicit Lexer(const std::string &s) : src(s) {}

    void lex() {
        while (i < src.size()) {
            char c = src[i];
            if (std::isspace((unsigned char)c)) { ++i; continue; }
            if (c == '"') { lex_string(); continue; }
            if (std::isdigit((unsigned char)c) ||
                (c == '.' && i + 1 < src.size() &&
                 std::isdigit((unsigned char)src[i + 1]))) {
                lex_number();
                continue;
            }
            if (std::isalpha((unsigned char)c) || c == '_') { lex_ident(); continue; }
            lex_op();
        }
        toks.push_back({Tok::T::End, ""});
    }

    void lex_string() {
        ++i;  // opening quote
        std::string s;
        while (i < src.size() && src[i] != '"') {
            // ASLX strings do not use backslash escapes; a doubled "" is rare.
            s += src[i++];
        }
        if (i < src.size()) ++i;  // closing quote
        toks.push_back({Tok::T::Str, s});
    }

    void lex_number() {
        size_t start = i;
        bool isdbl = false;
        while (i < src.size() &&
               (std::isdigit((unsigned char)src[i]) || src[i] == '.')) {
            if (src[i] == '.') isdbl = true;
            ++i;
        }
        std::string n = src.substr(start, i - start);
        Tok t{Tok::T::Num, n};
        t.num = std::atof(n.c_str());
        t.is_int = !isdbl;
        toks.push_back(t);
    }

    void lex_ident() {
        size_t start = i;
        while (i < src.size() &&
               (std::isalnum((unsigned char)src[i]) || src[i] == '_'))
            ++i;
        toks.push_back({Tok::T::Ident, src.substr(start, i - start)});
    }

    void lex_op() {
        static const char *twos[] = {"<>", "!=", "==", ">=", "<=", nullptr};
        for (int k = 0; twos[k]; ++k) {
            if (src.compare(i, 2, twos[k]) == 0) {
                toks.push_back({Tok::T::Op, twos[k]});
                i += 2;
                return;
            }
        }
        toks.push_back({Tok::T::Op, std::string(1, src[i])});
        ++i;
    }
};

struct Parser {
    std::vector<Tok> toks;
    size_t p = 0;

    explicit Parser(std::vector<Tok> t) : toks(std::move(t)) {}

    const Tok &cur() { return toks[p]; }
    bool is_op(const char *o) {
        return cur().t == Tok::T::Op && cur().s == o;
    }
    bool is_kw(const char *k) {
        return cur().t == Tok::T::Ident && cur().s == k;
    }
    void advance() { if (p + 1 < toks.size()) ++p; }
    [[noreturn]] void fail(const std::string &m) {
        throw std::runtime_error("expression parse error: " + m);
    }

    ExprP parse() {
        ExprP e = parse_ternary();
        if (cur().t != Tok::T::End) fail("unexpected token '" + cur().s + "'");
        return e;
    }

    ExprP parse_ternary() {
        ExprP cond = parse_logical();
        if (is_op("?")) {
            advance();
            ExprP a = parse_logical();
            if (!is_op(":")) fail("expected ':'");
            advance();
            ExprP b = parse_logical();
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Kind::Ternary;
            e->a = cond; e->b = a; e->c = b;
            return e;
        }
        return cond;
    }

    ExprP bin(const std::string &op, ExprP l, ExprP r) {
        auto e = std::make_shared<Expr>();
        e->kind = Expr::Kind::Binary; e->str = op; e->a = l; e->b = r;
        return e;
    }

    ExprP parse_logical() {
        ExprP l = parse_equality();
        while (is_kw("and") || is_kw("or") || is_kw("xor")) {
            std::string op = cur().s; advance();
            l = bin(op, l, parse_equality());
        }
        return l;
    }
    ExprP parse_equality() {
        ExprP l = parse_relational();
        while (is_op("=") || is_op("==") || is_op("<>") || is_op("!=")) {
            std::string op = cur().s; advance();
            l = bin((op == "==" ? "=" : (op == "!=" ? "<>" : op)), l,
                    parse_relational());
        }
        return l;
    }
    ExprP parse_relational() {
        ExprP l = parse_additive();
        while (is_op(">") || is_op("<") || is_op(">=") || is_op("<=")) {
            std::string op = cur().s; advance();
            l = bin(op, l, parse_additive());
        }
        return l;
    }
    ExprP parse_additive() {
        ExprP l = parse_multiplicative();
        while (is_op("+") || is_op("-")) {
            std::string op = cur().s; advance();
            l = bin(op, l, parse_multiplicative());
        }
        return l;
    }
    ExprP parse_multiplicative() {
        ExprP l = parse_unary();
        while (is_op("*") || is_op("/") || is_op("%")) {
            std::string op = cur().s; advance();
            l = bin(op, l, parse_unary());
        }
        return l;
    }
    ExprP parse_unary() {
        if (is_op("-") || is_op("!") || is_kw("not")) {
            std::string op = is_kw("not") ? "not" : cur().s;
            advance();
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Kind::Unary; e->str = op; e->a = parse_unary();
            return e;
        }
        return parse_postfix();
    }
    ExprP parse_postfix() {
        ExprP e = parse_primary();
        while (true) {
            if (is_op(".")) {
                advance();
                if (cur().t != Tok::T::Ident) fail("expected name after '.'");
                std::string name = cur().s; advance();
                if (is_op("(")) {
                    // method-style call obj.name(args) -- rare; treat as call
                    // taking the receiver as the first argument is not needed
                    // for M2, so parse and drop into a Call on `name`.
                    auto call = std::make_shared<Expr>();
                    call->kind = Expr::Kind::Call; call->str = name;
                    call->args = parse_args();
                    call->a = e;  // receiver kept for future use
                    e = call;
                } else {
                    auto m = std::make_shared<Expr>();
                    m->kind = Expr::Kind::Member; m->a = e; m->str = name;
                    e = m;
                }
            } else if (is_op("[")) {
                advance();
                ExprP idx = parse_ternary();
                if (!is_op("]")) fail("expected ']'");
                advance();
                auto ix = std::make_shared<Expr>();
                ix->kind = Expr::Kind::Index; ix->a = e; ix->b = idx;
                e = ix;
            } else {
                break;
            }
        }
        return e;
    }
    std::vector<ExprP> parse_args() {
        // assumes cur() is '('
        advance();
        std::vector<ExprP> args;
        if (is_op(")")) { advance(); return args; }
        while (true) {
            args.push_back(parse_ternary());
            if (is_op(",") || is_op(";")) { advance(); continue; }
            break;
        }
        if (!is_op(")")) fail("expected ')'");
        advance();
        return args;
    }
    ExprP parse_primary() {
        const Tok &t = cur();
        if (t.t == Tok::T::Num) {
            advance();
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Kind::Num; e->num = t.num; e->is_int = t.is_int;
            return e;
        }
        if (t.t == Tok::T::Str) {
            advance();
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Kind::Str; e->str = t.s;
            return e;
        }
        if (t.t == Tok::T::Ident) {
            if (t.s == "true" || t.s == "false") {
                advance();
                auto e = std::make_shared<Expr>();
                e->kind = Expr::Kind::Bool; e->boolean = (t.s == "true");
                return e;
            }
            if (t.s == "null") {
                advance();
                auto e = std::make_shared<Expr>();
                e->kind = Expr::Kind::Null;
                return e;
            }
            std::string name = t.s;
            advance();
            if (is_op("(")) {
                auto e = std::make_shared<Expr>();
                e->kind = Expr::Kind::Call; e->str = name;
                e->args = parse_args();
                return e;
            }
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Kind::Var; e->str = name;
            return e;
        }
        if (is_op("(")) {
            advance();
            ExprP e = parse_ternary();
            if (!is_op(")")) fail("expected ')'");
            advance();
            return e;
        }
        fail("unexpected token '" + t.s + "'");
    }
};

}  // namespace

// Compile an expression source string to an AST (used by the statement parser).
static ExprP compile_expr_str(const std::string &src) {
    Lexer lex(src);
    lex.lex();
    Parser parser(lex.toks);
    return parser.parse();
}

// ===========================================================================
// Statement AST + parser
// ===========================================================================

struct Stmt {
    enum class Kind { Msg, If, While, For, ForEach, Assign, Call, Return, Comment };
    Kind kind;
    std::string name;              // Assign var/prop, For/ForEach var, Call name
    ExprP expr;                    // Msg/Return value, While/If cond, Assign value
    ExprP obj;                     // Assign target object (before last dot), or null
    ExprP from, to, step;          // For
    ExprP list;                    // ForEach
    std::vector<Stmt> body;        // block
    // If: chained else-if/else
    std::vector<std::pair<ExprP, std::vector<Stmt>>> elseifs;
    std::vector<Stmt> else_body;
    bool has_else = false;
    std::vector<ExprP> call_args;  // Call
};

// ===========================================================================
// Interp
// ===========================================================================

Interp::Interp(World &world) : world_(world) {
    rng_.seed(1234);
    const char *env = std::getenv("ASLX_SEED");
    if (env) rng_.seed((uint32_t)std::strtoul(env, nullptr, 10));
    print = [this](const std::string &s) { output_ += s; };
}
Interp::~Interp() = default;

std::shared_ptr<Expr> Interp::compile_expr(const std::string &src) {
    auto it = expr_cache_.find(src);
    if (it != expr_cache_.end()) return it->second;
    Lexer lex(src);
    lex.lex();
    Parser parser(lex.toks);
    ExprP e = parser.parse();
    expr_cache_[src] = e;
    return e;
}

Value Interp::eval(const std::string &source, Context &ctx) {
    std::string s = rt_trim(source);
    if (s.empty()) return vnull();
    ExprP e = compile_expr(s);
    return eval_expr(*e, ctx);
}

// -- statement parsing ------------------------------------------------------

static bool starts_with_word(const std::string &line, const std::string &kw) {
    if (line.compare(0, kw.size(), kw) != 0) return false;
    if (line.size() == kw.size()) return true;
    char n = line[kw.size()];
    return !(std::isalnum((unsigned char)n) || n == '_');
}

// Forward-declared recursive statement compiler (defined in the .inc below).
static std::vector<Stmt> parse_statements(const std::string &src, Interp &interp);

std::shared_ptr<std::vector<Stmt>> Interp::compile_script(const std::string &src) {
    auto it = script_cache_.find(src);
    if (it != script_cache_.end()) return it->second;
    auto v = std::make_shared<std::vector<Stmt>>(parse_statements(src, *this));
    script_cache_[src] = v;
    return v;
}

void Interp::run_script(const std::string &source, Context &ctx) {
    auto stmts = compile_script(source);
    exec_block(*stmts, ctx);
}

// -- execution --------------------------------------------------------------

void Interp::exec_block(const std::vector<Stmt> &stmts, Context &ctx) {
    for (const Stmt &s : stmts) {
        exec_stmt(s, ctx);
        if (ctx.returned) return;
    }
}

Element *interp_eval_element(Interp &, const Value &);

void Interp::exec_stmt(const Stmt &s, Context &ctx) {
    switch (s.kind) {
    case Stmt::Kind::Comment: return;
    case Stmt::Kind::Msg: {
        Value v = eval_expr(*s.expr, ctx);
        print(to_string(v) + "\n");
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
        std::vector<std::string> items;
        if (is_list(lst)) items = lst.list;
        else if (lst.type == Value::Type::StringDict ||
                 lst.type == Value::Type::ObjectDict) {
            for (auto &kv : lst.dict) items.push_back(kv.first);
        } else {
            errors().push_back("Cannot foreach over non-list");
            return;
        }
        bool obj = (lst.type == Value::Type::ObjectList ||
                    lst.type == Value::Type::ObjectDict);
        for (const std::string &item : items) {
            ctx.locals[s.name] = obj ? vobj(item) : vstr(item);
            exec_block(s.body, ctx);
            if (ctx.returned) break;
        }
        return;
    }
    case Stmt::Kind::Assign: {
        Value val = eval_expr(*s.expr, ctx);
        if (!s.obj) {
            ctx.locals[s.name] = val;
        } else {
            Value ov = eval_expr(*s.obj, ctx);
            Element *e = interp_eval_element(*this, ov);
            if (e) e->set_field(s.name, val);
            else errors().push_back("Assignment to attribute of non-object");
        }
        return;
    }
    case Stmt::Kind::Call: {
        std::vector<Value> args;
        for (const auto &a : s.call_args) args.push_back(eval_expr(*a, ctx));
        // A game function takes precedence at statement position.
        if (world_.find(s.name) &&
            (world_.find(s.name)->elem_type == "function")) {
            call_function(s.name, std::move(args), &ctx);
        } else {
            bool handled = false;
            call_builtin(s.name, args, handled, ctx);
            if (!handled)
                errors().push_back("Function not found: '" + s.name + "'");
        }
        return;
    }
    }
}

Element *interp_eval_element(Interp &in, const Value &v) {
    if (v.type == Value::Type::ObjectRef) return in.world().find(v.str);
    return nullptr;
}

// -- expression evaluation --------------------------------------------------

const Value *Interp::resolve_field(Element *e, const std::string &name) {
    if (!e) return nullptr;
    if (const Value *own = e->field(name)) return own;
    // inherited types, most recent first
    for (auto it = e->inherits.rbegin(); it != e->inherits.rend(); ++it) {
        Element *type = world_.find(*it);
        if (type) {
            if (const Value *v = resolve_field(type, name)) return v;
        }
    }
    return nullptr;
}

Value Interp::resolve_variable(const std::string &name, Context &ctx, bool &found) {
    found = true;
    if (name == "null") return vnull();
    auto it = ctx.locals.find(name);
    if (it != ctx.locals.end()) return it->second;
    if (Element *e = world_.find(name)) {
        (void)e;
        return vobj(name);
    }
    found = false;
    return vnull();
}

static bool values_equal(const Value &a, const Value &b) {
    if (a.type == Value::Type::Null || b.type == Value::Type::Null)
        return a.type == Value::Type::Null && b.type == Value::Type::Null;
    if (is_number(a) && is_number(b)) return as_double(a) == as_double(b);
    if (a.type == Value::Type::Boolean && b.type == Value::Type::Boolean)
        return a.boolean == b.boolean;
    if (a.type == Value::Type::ObjectRef && b.type == Value::Type::ObjectRef)
        return a.str == b.str;
    if ((a.type == Value::Type::String || a.type == Value::Type::ObjectRef) &&
        (b.type == Value::Type::String || b.type == Value::Type::ObjectRef))
        return a.str == b.str;
    return false;
}

Value Interp::eval_expr(const Expr &e, Context &ctx) {
    switch (e.kind) {
    case Expr::Kind::Num:
        return e.is_int ? vint((long)e.num) : vdouble(e.num);
    case Expr::Kind::Str: return vstr(e.str);
    case Expr::Kind::Bool: return vbool(e.boolean);
    case Expr::Kind::Null: return vnull();
    case Expr::Kind::Var: {
        bool found;
        Value v = resolve_variable(e.str, ctx, found);
        if (!found) {
            errors().push_back("Unknown object or variable '" + e.str + "'");
            return vnull();
        }
        return v;
    }
    case Expr::Kind::Member: {
        Value ov = eval_expr(*e.a, ctx);
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
            if (i < 0 || (size_t)i >= coll.list.size()) {
                errors().push_back("List index out of range");
                return vnull();
            }
            return coll.type == Value::Type::ObjectList ? vobj(coll.list[i])
                                                        : vstr(coll.list[i]);
        }
        if (coll.type == Value::Type::StringDict ||
            coll.type == Value::Type::ObjectDict) {
            std::string key = to_string(idx);
            for (auto &kv : coll.dict)
                if (kv.first == key)
                    return coll.type == Value::Type::ObjectDict ? vobj(kv.second)
                                                                : vstr(kv.second);
            errors().push_back("Dictionary key not found: " + key);
            return vnull();
        }
        errors().push_back("Cannot index a non-collection");
        return vnull();
    }
    case Expr::Kind::Call: {
        std::vector<Value> args;
        for (const auto &a : e.args) args.push_back(eval_expr(*a, ctx));
        bool handled = false;
        Value r = call_builtin(e.str, args, handled, ctx);
        if (handled) return r;
        if (world_.find(e.str) && world_.find(e.str)->elem_type == "function")
            return call_function(e.str, std::move(args), &ctx);
        errors().push_back("Unknown function '" + e.str + "'");
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
    case Expr::Kind::Binary: {
        const std::string &op = e.str;
        if (op == "and") return vbool(truthy(eval_expr(*e.a, ctx)) &&
                                      truthy(eval_expr(*e.b, ctx)));
        if (op == "or") return vbool(truthy(eval_expr(*e.a, ctx)) ||
                                     truthy(eval_expr(*e.b, ctx)));
        Value l = eval_expr(*e.a, ctx);
        Value r = eval_expr(*e.b, ctx);
        if (op == "xor") return vbool(truthy(l) != truthy(r));
        if (op == "=") return vbool(values_equal(l, r));
        if (op == "<>") return vbool(!values_equal(l, r));
        if (op == "+") {
            if (l.type == Value::Type::String || r.type == Value::Type::String ||
                l.type == Value::Type::ObjectRef || r.type == Value::Type::ObjectRef)
                return vstr(to_string(l) + to_string(r));
            if (l.type == Value::Type::Int && r.type == Value::Type::Int)
                return vint(l.integer + r.integer);
            return vdouble(as_double(l) + as_double(r));
        }
        if (op == "-" || op == "*" || op == "%") {
            bool ints = l.type == Value::Type::Int && r.type == Value::Type::Int;
            double a = as_double(l), b = as_double(r);
            if (op == "-") return ints ? vint(l.integer - r.integer) : vdouble(a - b);
            if (op == "*") return ints ? vint(l.integer * r.integer) : vdouble(a * b);
            return ints ? vint(r.integer ? l.integer % r.integer : 0)
                        : vdouble(std::fmod(a, b));
        }
        if (op == "/") {
            double b = as_double(r);
            return vdouble(b != 0 ? as_double(l) / b : 0);
        }
        // relational
        if (is_number(l) && is_number(r)) {
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
    Element *fn = world_.find(name);
    if (!fn || fn->elem_type != "function") {
        errors().push_back("Function not found: '" + name + "'");
        return vnull();
    }
    Context local;
    const Value *pn = fn->field("paramnames");
    if (pn) {
        for (size_t i = 0; i < pn->list.size() && i < args.size(); ++i)
            local.locals[pn->list[i]] = args[i];
    }
    const Value *body = fn->field("script");
    if (body && body->type == Value::Type::Script)
        run_script(body->str, local);
    return local.return_value;
}

}  // namespace aslx

// The statement parser and built-ins are large; they live in a second unit that
// is included here so this file stays the single translation unit for the
// runtime (the test unity-includes it).
#include "aslx-runtime-parse.inc"
#include "aslx-runtime-builtins.inc"
