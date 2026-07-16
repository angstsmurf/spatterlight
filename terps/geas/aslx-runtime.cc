// aslx-runtime.cc -- Quest 5 script + expression evaluator. See aslx-runtime.hh.

#include "aslx-runtime.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <stdexcept>

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

struct CompiledRegex {
    std::regex re;
    // names[k] is the name of capture group k+1 ("" for an unnamed group).
    std::vector<std::string> names;
    bool valid = false;
};

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
static std::vector<std::pair<std::string, std::string>> named_groups(
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
static int named_group_len(const CompiledRegex &cr, const std::smatch &m) {
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
    Value v; v.type = Value::Type::StringList; v.list() = std::move(l); return v;
}
static Value vobjlist(std::vector<std::string> l) {
    Value v; v.type = Value::Type::ObjectList; v.list() = std::move(l); return v;
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
        for (size_t i = 0; i < v.list().size(); ++i) {
            if (i) s += ", ";
            s += v.list()[i];
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

// Case-insensitive ASCII compare (NCalc's true/false literals and the "null"
// parameter lookup are case-insensitive).
static bool rt_iequals(const std::string &a, const char *b) {
    size_t n = 0;
    for (; n < a.size() && b[n]; ++n)
        if (std::tolower((unsigned char)a[n]) != std::tolower((unsigned char)b[n]))
            return false;
    return n == a.size() && b[n] == 0;
}

static std::string rt_trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

// Replace the contents of each "..." string with dashes, keeping the quotes and
// everything outside strings unchanged, so structural scans ignore string bodies.
// Replace the contents of every double-quoted string with '-' so structural
// scanning (braces, newlines, "//") ignores anything inside a literal. A
// backslash escapes the next character -- in particular "\"" is a literal quote,
// not a terminator -- matching QuestViva's Utility.SplitQuotes (the backslash
// "don't process the next character" rule). Length is preserved so positions in
// the obscured string map back to the original.
static std::string obscure_strings(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    bool inq = false;
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '\\') {  // backslash + next char are literal, never a quote
            out += inq ? '-' : c;
            if (i + 1 < in.size()) { out += inq ? '-' : in[i + 1]; ++i; }
            continue;
        }
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

// Strip // line comments, respecting string literals (a "//" inside a quoted
// string is not a comment). Uses obscure_strings so backslash-escaped quotes are
// handled the same way the statement splitter handles them.
static std::string remove_comments(const std::string &input) {
    std::string ob = obscure_strings(input);
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (ob[i] == '/' && i + 1 < input.size() && ob[i + 1] == '/') {
            // Skip to end of line (the comment); keep the newline itself.
            while (i < input.size() && input[i] != '\n') ++i;
            if (i < input.size()) out += '\n';
            continue;
        }
        out += input[i];
    }
    return out;
}

// Quest identifiers can contain spaces ("OUTSIDE INN", "game.Next text").
// QuestViva pre-encodes every expression with Utility.EncodeIdentifierSpaces:
// outside string literals, two space-separated words whose boundary characters
// are both word characters are joined into one identifier -- unless either
// word is an expression keyword (and/or/xor/not/if/in). We do the same but
// join with '\x01' (instead of "___SPACE___"), which the lexer folds back to a
// space inside a single Ident token, so no decode pass is needed downstream.
static bool is_word_char(char c) {
    return std::isalnum((unsigned char)c) || c == '_';
}

static bool is_expr_keyword(const std::string &w) {
    // Utility.s_keywords (case-sensitive, like the HashSet it feeds).
    return w == "and" || w == "or" || w == "xor" || w == "not" || w == "if" ||
           w == "in";
}

static std::string encode_identifier_spaces(const std::string &in) {
    std::string out = in;
    bool inq = false;
    for (size_t i = 0; i < out.size(); ++i) {
        char c = out[i];
        if (c == '\\') { ++i; continue; }  // backslash: next char is literal
        if (c == '"') { inq = !inq; continue; }
        if (inq || c != ' ') continue;
        // A single space with word characters on both sides (a run of spaces
        // never joins -- QuestViva splits on ' ' and an empty word bails).
        if (i == 0 || i + 1 >= out.size()) continue;
        if (!is_word_char(out[i - 1]) || !is_word_char(out[i + 1])) continue;
        // The trailing word-char run before / leading run after the space
        // (the (\w+)$ / ^(\w+) matches in IsSplitVariableName).
        size_t a = i;
        while (a > 0 && is_word_char(out[a - 1])) --a;
        size_t b = i + 1;
        while (b < out.size() && is_word_char(out[b])) ++b;
        if (is_expr_keyword(out.substr(a, i - a)) ||
            is_expr_keyword(out.substr(i + 1, b - i - 1)))
            continue;
        out[i] = '\x01';
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
            char c = src[i];
            // Backslash escapes, matching Parlot's StringLiteralQuotes decoding
            // used by QuestNCalcLogicalExpressionParser. Core relies on \" \' \\
            // (the last so "\\D" yields the regex \D); an unknown escape keeps
            // the following character verbatim.
            if (c == '\\' && i + 1 < src.size()) {
                char n = src[i + 1];
                switch (n) {
                case 'n': s += '\n'; break;
                case 't': s += '\t'; break;
                case 'r': s += '\r'; break;
                default:  s += n;    break;  // " ' \ and anything else: literal
                }
                i += 2;
                continue;
            }
            s += c;
            ++i;
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
        std::string s;
        while (i < src.size() &&
               (std::isalnum((unsigned char)src[i]) || src[i] == '_' ||
                src[i] == '\x01')) {
            // '\x01' is the encode_identifier_spaces join marker: this is one
            // multi-word identifier ("OUTSIDE INN"); restore the space.
            s += (src[i] == '\x01') ? ' ' : src[i];
            ++i;
        }
        toks.push_back({Tok::T::Ident, s});
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
        ExprP l = parse_not();
        while (is_kw("and") || is_kw("or") || is_kw("xor")) {
            std::string op = cur().s; advance();
            l = bin(op, l, parse_not());
        }
        return l;
    }
    // Logical NOT binds looser than equality/comparison, so "not x = null" is
    // "not (x = null)" -- matching QuestViva's notOperator level, which sits
    // between and/or and equality (not the tight unary "-"). See
    // QuestNCalcLogicalExpressionParser (the grammar comment on notOperator).
    ExprP parse_not() {
        if (is_kw("not") || is_op("!")) {
            advance();
            auto e = std::make_shared<Expr>();
            e->kind = Expr::Kind::Unary; e->str = "not"; e->a = parse_not();
            return e;
        }
        return parse_equality();
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
        while (true) {
            if (is_op(">") || is_op("<") || is_op(">=") || is_op("<=")) {
                std::string op = cur().s; advance();
                l = bin(op, l, parse_additive());
                continue;
            }
            // "in" / "not in" sit at the relational level in QuestViva's
            // grammar (QuestNCalcLogicalExpressionParser: relational => shift
            // ((">=" | "<=" | "<" | ">" | "in" | "not in") shift)*).
            if (is_kw("in")) {
                advance();
                l = bin("in", l, parse_additive());
                continue;
            }
            if (is_kw("not") && p + 1 < toks.size() &&
                toks[p + 1].t == Tok::T::Ident && toks[p + 1].s == "in") {
                advance(); advance();
                l = bin("not in", l, parse_additive());
                continue;
            }
            break;
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
        // Only arithmetic negation binds tightly here; logical "not" is handled
        // at parse_not (looser than equality).
        if (is_op("-")) {
            std::string op = cur().s;
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
            // Boolean/null literals are case-insensitive ("True", "FALSE"):
            // NCalc's Terms.Text("true", true), and the null parameter check in
            // NcalcExpressionEvaluator.ResolveVariable.
            if (rt_iequals(t.s, "true") || rt_iequals(t.s, "false")) {
                advance();
                auto e = std::make_shared<Expr>();
                e->kind = Expr::Kind::Bool; e->boolean = rt_iequals(t.s, "true");
                return e;
            }
            if (rt_iequals(t.s, "null")) {
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
    std::string enc = encode_identifier_spaces(src);
    Lexer lex(enc);
    lex.lex();
    Parser parser(lex.toks);
    try {
        return parser.parse();
    } catch (const std::runtime_error &err) {
        throw std::runtime_error(std::string(err.what()) + " in [" + src + "]");
    }
}

// ===========================================================================
// Statement AST + parser
// ===========================================================================

struct Stmt {
    enum class Kind {
        Msg, If, While, For, ForEach, Assign, Call, Return, Comment,
        Switch, FirstTime, OnReady, Wait
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
// Interp
// ===========================================================================

Interp::Interp(World &world) : world_(world) {
    rng_.seed(1234);
    const char *env = std::getenv("ASLX_SEED");
    if (env) rng_.seed((uint32_t)std::strtoul(env, nullptr, 10));
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
    std::string enc = encode_identifier_spaces(src);
    Lexer lex(enc);
    lex.lex();
    Parser parser(lex.toks);
    ExprP e;
    try {
        e = parser.parse();
    } catch (const std::runtime_error &err) {
        throw std::runtime_error(std::string(err.what()) + " in [" + src + "]");
    }
    expr_cache_[src] = e;
    return e;
}

Value Interp::eval(const std::string &source, Context &ctx) {
    std::string s = rt_trim(source);
    if (s.empty()) return vnull();
    // A parse failure (or any other internal throw) must not abort the engine:
    // log it as a world error and yield null, so one unsupported construct
    // degrades gracefully (QuestViva wraps evaluation the same way).
    try {
        ExprP e = compile_expr(s);
        return eval_expr(*e, ctx);
    } catch (const std::exception &err) {
        errors().push_back(std::string("Error evaluating expression: ") +
                           err.what());
        return vnull();
    }
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
    if (script_errors_fatal_) return;
    if (script_depth_ >= kMaxScriptDepth)
        throw std::runtime_error(
            "Script execution depth exceeded 200 - this usually means a script "
            "is recursing infinitely");
    // This is the script boundary (QuestViva RunScriptAsync): a parse or
    // runtime throw aborts THIS script body only; it is logged and reported,
    // and the calling script carries on with its next statement.
    ++script_depth_;
    try {
        auto stmts = compile_script(source);
        exec_block(*stmts, ctx);
    } catch (const std::exception &err) {
        report_script_error(err.what());
    }
    --script_depth_;
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
        std::string text = to_string(v);
        // v540+ routes msg through Core's OutputText so the {...} text processor
        // runs (QuestViva WorldModel.PrintAsync). Without Core (unit tests) or on
        // older games, print directly. OutputText ends at JS.addText -> print.
        Element *ot = world_.find("OutputText");
        if (world_.asl_version >= 540 && ot && ot->elem_type == "function")
            call_function("OutputText", {vstr(text)}, &ctx);
        else
            print(text + "\n");
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
        if (is_list(lst)) items = lst.list();
        else if (lst.type == Value::Type::StringDict ||
                 lst.type == Value::Type::ObjectDict ||
                 lst.type == Value::Type::ScriptDict) {
            for (auto &kv : lst.dict()) items.push_back(kv.first);
        } else {
            error("Cannot foreach over non-list");
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
        Value val;
        if (s.expr) {
            val = eval_expr(*s.expr, ctx);
        } else {
            // "x => { ... }": assign the script literal itself.
            val.type = Value::Type::Script;
            val.str = s.script_text;
        }
        if (!s.obj) {
            ctx.locals[s.name] = val;
        } else {
            Value ov = eval_expr(*s.obj, ctx);
            Element *e = interp_eval_element(*this, ov);
            if (e) e->set_field(s.name, val);
            else error("Assignment to attribute '" + s.name +
                                    "' of a non-object");
        }
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
            exec_block(s.body, ctx);
        } else if (s.has_else) {
            exec_block(s.else_body, ctx);
        }
        return;
    }
    case Stmt::Kind::OnReady: {
        on_ready_.emplace_back(&s.body, ctx);
        return;
    }
    case Stmt::Kind::Wait: {
        // The real input model (M4, TODO §3) blocks on a keypress before the
        // callback. Headless for now: warn once and run the callback as if the
        // key had been pressed immediately.
        warn_once("wait", "'wait' runs its script without pausing (no input "
                          "model yet)");
        exec_block(s.body, ctx);
        return;
    }
    case Stmt::Kind::Call: {
        // Reserved statement commands (do/invoke/create/set/list add/...) take
        // precedence, then game functions, then built-ins.
        if (exec_statement_command(s.name, s.call_args, ctx)) return;
        std::vector<Value> args;
        for (const auto &a : s.call_args) args.push_back(eval_expr(*a, ctx));
        if (!s.script_text.empty()) {
            // Trailing "{ script }" block: one extra script-literal argument.
            Value scr;
            scr.type = Value::Type::Script;
            scr.str = s.script_text;
            args.push_back(std::move(scr));
        }
        if (world_.find(s.name) &&
            (world_.find(s.name)->elem_type == "function")) {
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
    if (frames_.empty()) throw std::runtime_error(message);
    throw std::runtime_error(message + " (in " + frames_.back() + ")");
}

void Interp::report_script_error(const std::string &what) {
    std::string msg = std::string("Error running script: ") + what;
    errors().push_back(msg);
    if (++script_error_count_ >= kMaxScriptErrors) {
        // Every script is failing the same way; the session is wedged. Stop
        // running scripts and end the game (WorldModel's scriptErrorsFatal).
        script_errors_fatal_ = true;
        world_.finished = true;
    } else if (!reporting_error_) {
        // The message also goes to the player, once (no recursive reports if
        // printing itself errors) -- WorldModel.PrintAsync in the catch.
        reporting_error_ = true;
        print(msg + "\n");
        reporting_error_ = false;
    }
}

void Interp::warn_once(const std::string &key, const std::string &message) {
    for (const auto &k : warned_)
        if (k == key) return;
    warned_.push_back(key);
    world_.warnings.push_back(message);
}

void Interp::drain_on_ready() {
    // FIFO; callbacks queued during draining are appended and run this pass.
    while (!on_ready_.empty()) {
        auto item = on_ready_.front();
        on_ready_.erase(on_ready_.begin());
        Context ctx = item.second;
        // Each callback is its own script boundary: one failing callback is
        // reported and the rest of the queue still drains.
        try {
            exec_block(*item.first, ctx);
        } catch (const std::exception &err) {
            report_script_error(err.what());
        }
        if (script_errors_fatal_) break;
    }
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
    Element *def = in.world().find(impl->declared_type);
    const Value *pn = def ? def->field("paramnames") : nullptr;
    for (size_t k = 0; k < params.size(); ++k) {
        if (pn && k < pn->list().size())
            local.locals[pn->list()[k]] = params[k];
    }
    Value self; self.type = Value::Type::ObjectRef; self.str = obj->name;
    local.locals["this"] = self;
    in.run_script(impl->str, local);
    return local.return_value;
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
    if (rt_iequals(name, "null")) return vnull();
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
            error("Unknown object or variable '" + e.str + "'");
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
            if (i < 0 || (size_t)i >= coll.list().size()) {
                error("List index out of range");
                return vnull();
            }
            return coll.type == Value::Type::ObjectList ? vobj(coll.list()[i])
                                                        : vstr(coll.list()[i]);
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
                                   ? std::atof(v.str.c_str()) : as_double(v));
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
        if (world_.find(e.str) && world_.find(e.str)->elem_type == "function")
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
        // "x in y": list membership, dictionary key lookup, or substring
        // (NCalc's In over QuestList / string; dictionary keys per Quest docs).
        if (op == "in" || op == "not in") {
            bool contains = false;
            if (is_list(r)) {
                std::string item = (l.type == Value::Type::ObjectRef ||
                                    l.type == Value::Type::String ||
                                    l.type == Value::Type::Script)
                                       ? l.str : to_string(l);
                for (auto &s : r.list())
                    if (s == item) { contains = true; break; }
            } else if (r.type == Value::Type::StringDict ||
                       r.type == Value::Type::ObjectDict ||
                       r.type == Value::Type::ScriptDict) {
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
            auto elem_str = [](const Value &v) {
                return (v.type == Value::Type::ObjectRef ||
                        v.type == Value::Type::String ||
                        v.type == Value::Type::Script)
                           ? v.str : to_string(v);
            };
            if (op == "+" && is_list(l) && is_list(r)) {
                Value out = l; out.detach();
                for (auto &s : r.list()) out.list().push_back(s);
                return out;
            }
            if (op == "+" && is_list(l)) {
                Value out = l; out.detach();
                out.list().push_back(elem_str(r));
                return out;
            }
            if (op == "+") {  // elem + list -> prepend
                Value out = r; out.detach();
                out.list().insert(out.list().begin(), elem_str(l));
                return out;
            }
            if (op == "-" && is_list(l)) {  // list - elem -> remove first
                Value out = l; out.detach();
                std::string it = elem_str(r);
                auto &v = out.list();
                for (auto i = v.begin(); i != v.end(); ++i)
                    if (*i == it) { v.erase(i); break; }
                return out;
            }
            if (op == "*" && is_list(l) && is_list(r)) {  // union
                Value out = l; out.detach();
                for (auto &s : r.list()) {
                    bool present = false;
                    for (auto &x : l.list()) if (x == s) { present = true; break; }
                    if (!present) out.list().push_back(s);
                }
                return out;
            }
        }
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
        error("Function not found: '" + name + "'");
        return vnull();
    }
    Context local;
    const Value *pn = fn->field("paramnames");
    if (pn) {
        for (size_t i = 0; i < pn->list().size() && i < args.size(); ++i)
            local.locals[pn->list()[i]] = args[i];
    }
    const Value *body = fn->field("script");
    if (body && body->type == Value::Type::Script) {
        frames_.push_back(name);
        try {
            run_script(body->str, local);
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
        if (const Value *inh = resolve_field(el, e.str))
            return &el->set_field(e.str, *inh);
        return &el->set_field(e.str, Value{});
    }
    return nullptr;
}

bool Interp::exec_statement_command(const std::string &name,
                                    const std::vector<std::shared_ptr<Expr>> &args,
                                    Context &ctx) {
    auto ev = [&](size_t i) -> Value {
        return i < args.size() ? eval_expr(*args[i], ctx) : vnull();
    };
    auto as_element = [&](const Value &v) -> Element * {
        return v.type == Value::Type::ObjectRef ? world_.find(v.str) : nullptr;
    };

    // JS.* -- the front-end bridge. Only JS.addText carries game text; route it
    // to the output sink. Everything else is a UI side effect we can ignore in
    // the headless/native core (a later presentation milestone wires the rest).
    if (name.compare(0, 3, "JS.") == 0) {
        std::string fn = name.substr(3);
        if (fn == "addText" && !args.empty())
            print(to_string(ev(0)));
        return true;
    }

    // Media/output-decoration commands (insert splices an HTML file into the
    // output): pure presentation, no world-model effect. Accept and no-op with
    // a one-time warning so a game using them still runs headless (the
    // presentation milestone wires them to Glk). Args are deliberately not
    // evaluated -- nothing observable should happen.
    if (name == "picture" || name == "play sound" || name == "stop sound" ||
        name == "insert") {
        warn_once(name, "'" + name + "' is not supported yet; ignored");
        return true;
    }

    // request (RequestType, data): a player-UI request (RequestScript). The
    // first argument is a bare enum identifier (Speak, Quit, Show, ...) that is
    // not a resolvable expression, so we must NOT evaluate the args -- headless
    // ignores UI requests (a later presentation milestone wires the handful Core
    // relies on). Same for `request` with a callback and RequestSave.
    if (name == "request") return true;

    if (name == "do") {
        Element *obj = as_element(ev(0));
        std::string action = to_string(ev(1));
        if (!obj) { errors().push_back("do: not an object"); return true; }
        const Value *scr = resolve_field(obj, action);
        if (!scr || scr->type != Value::Type::Script) {
            errors().push_back("do: no script '" + action + "'");
            return true;
        }
        Context local;
        if (args.size() >= 3) {
            Value params = ev(2);
            for (auto &kv : params.dict())
                local.locals[kv.first] = kv.second;  // values are typed per-entry
        }
        // DoScript passes the object as thisElement (WorldModel.RunScriptAsync
        // binds it as the "this" parameter).
        local.locals["this"] = vobj(obj->name);
        run_script(scr->str, local);
        return true;
    }
    if (name == "invoke") {
        Value scr = ev(0);
        if (scr.type != Value::Type::Script) {
            errors().push_back("invoke: not a script");
            return true;
        }
        Context local;
        if (args.size() >= 2) {
            Value params = ev(1);
            for (auto &kv : params.dict())
                local.locals[kv.first] = kv.second;  // values are typed per-entry
        }
        run_script(scr.str, local);
        return true;
    }
    if (name == "create") {
        std::string nm = to_string(ev(0));
        std::string ty = args.size() >= 2 ? to_string(ev(1)) : "";
        world_.create_object(nm, ty);
        return true;
    }
    if (name == "rundelegate") {  // RunDelegateScript
        std::vector<Value> params;
        for (size_t i = 2; i < args.size(); ++i) params.push_back(ev(i));
        interp_run_delegate(*this, as_element(ev(0)), to_string(ev(1)), params);
        return true;
    }
    if (name == "create timer") {  // CreateTimerScript
        world_.create_object(to_string(ev(0)), "", "timer");
        return true;
    }
    if (name == "create turnscript") {  // CreateTurnScript
        world_.create_object(to_string(ev(0)), "", "turnscript");
        return true;
    }
    if (name == "destroy") {
        world_.destroy_element(to_string(ev(0)));
        return true;
    }
    if (name == "set") {  // set(obj, "field", value)
        Element *obj = as_element(ev(0));
        if (obj) obj->set_field(to_string(ev(1)), ev(2));
        else errors().push_back("set: not an object");
        return true;
    }
    if (name == "list add" || name == "list remove") {
        if (args.size() < 2) return true;
        Value *lst = lvalue_of(*args[0], ctx);
        Value item = ev(1);
        if (!lst || !(lst->type == Value::Type::StringList ||
                      lst->type == Value::Type::ObjectList)) {
            errors().push_back("Unrecognised list type");
            return true;
        }
        std::string s = (item.type == Value::Type::ObjectRef) ? item.str
                                                             : to_string(item);
        if (name == "list add") {
            lst->list().push_back(s);
        } else {
            lst->list().erase(std::remove(lst->list().begin(), lst->list().end(), s),
                            lst->list().end());
        }
        return true;
    }
    if (name == "dictionary add" || name == "dictionary remove") {
        if (args.size() < 2) return true;
        Value *d = lvalue_of(*args[0], ctx);
        std::string key = to_string(ev(1));
        if (!d || !(d->type == Value::Type::StringDict ||
                    d->type == Value::Type::ObjectDict ||
                    d->type == Value::Type::ScriptDict)) {
            errors().push_back("Unrecognised dictionary type");
            return true;
        }
        // remove any existing entry with this key first (Add replaces).
        for (auto it = d->dict().begin(); it != d->dict().end();) {
            if (it->first == key) it = d->dict().erase(it);
            else ++it;
        }
        if (name == "dictionary add")
            d->dict().emplace_back(key, ev(2));  // store the typed value verbatim
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
    if (name == "undo" || name == "start transaction") {
        // UndoLogger is a later milestone; accept and no-op for now.
        return true;
    }
    return false;
}

}  // namespace aslx

// The statement parser and built-ins are large; they live in a second unit that
// is included here so this file stays the single translation unit for the
// runtime (the test unity-includes it).
#include "aslx-runtime-parse.inc"
#include "aslx-runtime-builtins.inc"
