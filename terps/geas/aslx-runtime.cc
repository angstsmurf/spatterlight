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
    unsigned long span = (unsigned long)(hi - lo + 1);
    long r = lo + (long)(next() % span);
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
    Value v; v.type = Value::Type::StringList;
    v.list();  // allocate the backing so a fresh list has its own identity
    for (std::string &s : l) v.list().push_back(vstr(std::move(s)));
    return v;
}
static Value vobjlist(std::vector<std::string> l) {
    Value v; v.type = Value::Type::ObjectList;
    v.list();
    for (std::string &s : l) v.list().push_back(vobj(std::move(s)));
    return v;
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
    // Any byte >= 0x80 is part of a UTF-8 sequence: .NET \w covers Unicode
    // letters, and game identifiers use them ("Glühwein").
    return std::isalnum((unsigned char)c) || c == '_' ||
           (unsigned char)c >= 0x80;
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
        Num, Str, Bool, Null, Var, Member, Index, Call, Unary, Binary, Ternary,
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
    // QuestViva's NcalcExpressionEvaluator constructs its OWN ExpressionOwner
    // -- and thus its own seed-1234 ErkyrathRandom -- per compiled expression,
    // so every expression's GetRandomInt/GetRandomDouble sequence starts fresh
    // and advances only when THAT expression evaluates. (Verified against the
    // oracle: EFMB's PickOneString stream restarts at the seed.)
    std::shared_ptr<Rng> rng;
};

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
            if (std::isalpha((unsigned char)c) || c == '_' ||
                (unsigned char)c >= 0x80) {
                // Bytes >= 0x80 are UTF-8 letters (.NET identifiers are \w,
                // which is Unicode-aware -- "Glühwein").
                lex_ident();
                continue;
            }
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
                (unsigned char)src[i] >= 0x80 || src[i] == '\x01')) {
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
        ExprP e = parser.parse();
        if (e) e->src = src;  // mark the Expression<T> wrap boundary
        return e;
    } catch (const std::runtime_error &err) {
        throw std::runtime_error(std::string(err.what()) + " in [" + src + "]");
    }
}

// Compile, deferring a failure to evaluation time (see Expr::Kind::ParseError).
static ExprP compile_expr_str_deferred(const std::string &src) {
    try {
        return compile_expr_str(src);
    } catch (const std::exception &err) {
        auto e = std::make_shared<Expr>();
        e->kind = Expr::Kind::ParseError;
        e->str = err.what();
        e->src = src;
        return e;
    }
}

// ===========================================================================
// Statement AST + parser
// ===========================================================================

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
    if (e) e->src = src;  // mark the Expression<T> wrap boundary
    expr_cache_[src] = e;
    return e;
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

// -- statement parsing ------------------------------------------------------

static bool starts_with_word(const std::string &line, const std::string &kw) {
    if (line.compare(0, kw.size(), kw) != 0) return false;
    if (line.size() == kw.size()) return true;
    char n = line[kw.size()];
    return !(std::isalnum((unsigned char)n) || n == '_' ||
             (unsigned char)n >= 0x80);
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
            "is recursing infinitely (e.g. a \"changed<field>\" script that "
            "sets the field it's watching)");
    // This is the script boundary (QuestViva RunScriptAsync): a parse or
    // runtime throw aborts THIS script body only; it is logged and reported,
    // and the calling script carries on with its next statement.
    ++script_depth_;
    try {
        auto stmts = compile_script(source);
        exec_block(*stmts, ctx);
    } catch (const TurnSuspended &) {
        // Not an error: a synchronous `play sound` parked the turn. The
        // suspension passes through every script boundary to the turn
        // boundary (QuestViva's await chain suspends RunScriptAsync itself).
        --script_depth_;
        throw;
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
static bool values_equal(const Value &a, const Value &b);

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
        else if (lst.type == Value::Type::StringDict ||
                 lst.type == Value::Type::ObjectDict ||
                 lst.type == Value::Type::ScriptDict) {
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
            if (e) {
                const Value *prev = resolve_field(e, s.name);
                Value old = prev ? *prev : vnull();
                // Fields.Set CLONES a list/dictionary on any assignment that
                // changes the OWN attribute (QuestList.RequiresCloning is
                // unconditionally true; the changed-check consults own
                // attributes only, so localising an inherited list --
                // "newPOV.pov_alt = newPOV.pov_alt" -- copies the type's
                // backing instead of aliasing it. Basilica's possession
                // mechanic relies on every body getting its own alt list).
                const Value *own = e->field(s.name);
                bool same_backing = own &&
                    own->list_store == val.list_store &&
                    own->dict_store == val.dict_store;
                if (!same_backing) val.detach();
                // v530+: assigning null REMOVES the own attribute
                // (Fields.Set), so HasAttribute goes false and Core re-init
                // paths ("player.grid_coordinates = null" then Grid_Redraw)
                // work. Older games store the null.
                if (world_.asl_version >= 530 && val.type == Value::Type::Null) {
                    log_field_set(e, s.name, val, /*removing=*/true);
                    e->remove_field(s.name);
                } else {
                    log_field_set(e, s.name, val, /*removing=*/false);
                    e->set_field(s.name, val);
                }
                // EVERY parent write moves the element to the end of its
                // parent's children -- even a same-value one. QuestViva's
                // Fields.Set calls SetParentFromFields unconditionally, which
                // removes + re-appends in the children index (the `changed`
                // check only guards the saver-facing SortIndex metafield).
                // WearGarment's redundant `object.parent = game.pov` really
                // does reorder the inventory.
                if (s.name == "parent") {
                    log_sort_index(e);
                    e->sort_index = world_.next_sort_index++;
                }
                fire_changed_script(e, s.name, old);
            } else {
                error("Assignment to attribute '" + s.name +
                                    "' of a non-object");
            }
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
        if (wait_pending_) { wait_cb_ = PendingCallback{}; end_pending_callback(); }
        wait_pending_ = true;
        wait_cb_.body = &s.body;
        wait_cb_.ctx = ctx;
        wait_cb_.ctx.returned = false;
        begin_pending_callback();
        return;
    }
    case Stmt::Kind::GetInput: {
        if (command_override_) {
            command_cb_ = PendingCallback{};
            end_pending_callback();
        }
        command_override_ = true;
        command_cb_.body = &s.body;
        command_cb_.ctx = ctx;
        command_cb_.ctx.returned = false;
        begin_pending_callback();
        return;
    }
    case Stmt::Kind::Ask: {
        // PlayerUI.ShowQuestion: the caption is handed to the host, not
        // printed -- rendering the yes/no prompt is presentation.
        std::string caption = to_string(eval_expr(*s.expr, ctx));
        if (question_pending_) {
            question_cb_ = PendingCallback{};
            end_pending_callback();
        }
        question_pending_ = true;
        question_ = caption;
        question_cb_.body = &s.body;
        question_cb_.ctx = ctx;
        question_cb_.ctx.returned = false;
        begin_pending_callback();
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
        // The caption is printed (PrintAsync) before the menu goes to the UI.
        print_via_core(caption, ctx);
        if (menu_pending_) { menu_cb_ = PendingCallback{}; end_pending_callback(); }
        menu_pending_ = true;
        menu_ = std::move(md);
        menu_cb_.body = &s.body;
        menu_cb_.ctx = ctx;
        menu_cb_.ctx.returned = false;
        begin_pending_callback();
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
    // No "(in function)" suffix: QuestViva prints exception messages verbatim
    // ("Error running script: " + ex.Message), and the golden transcripts
    // contain the bare form. frames_ stays for diagnostics elsewhere.
    throw std::runtime_error(message);
}

static std::string safe_xml_escape(const std::string &s);

void Interp::report_script_error(const std::string &what) {
    std::string msg = std::string("Error running script: ") + what;
    // The LOG copy carries the innermost executing function for diagnostics;
    // the player-facing print below stays bare (QuestViva prints ex.Message).
    errors().push_back(frames_.empty() ? msg
                                       : msg + " [in " + frames_.back() + "]");
    if (++script_error_count_ >= kMaxScriptErrors) {
        // Every script is failing the same way; the session is wedged. Stop
        // running scripts and end the game (WorldModel's scriptErrorsFatal).
        script_errors_fatal_ = true;
        world_.finished = true;
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
    // See the header note: the LogException-only boundary. Same log format as
    // report_script_error, no print, no breaker feed.
    std::string msg = std::string("Error running script: ") + what;
    errors().push_back(frames_.empty() ? msg
                                       : msg + " [in " + frames_.back() + "]");
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
    // Bony King's beforeenter spiral). The cap throw fires BEFORE the guarded
    // region, propagating to the ENCLOSING boundary, like RunScriptAsync's
    // entry check. A failure inside is reported and the engine carries on; a
    // TurnSuspended (synchronous `play sound`) abandons the callback silently.
    if (script_depth_ >= kMaxScriptDepth)
        throw std::runtime_error(
            "Script execution depth exceeded 200 - this usually means a script "
            "is recursing infinitely (e.g. a \"changed<field>\" script that "
            "sets the field it's watching)");
    ++script_depth_;
    try {
        exec_block(*body, ctx);
    } catch (const TurnSuspended &) {
        --script_depth_;
        throw;
    } catch (const std::exception &err) {
        report_script_error(err.what());
    }
    --script_depth_;
}

void Interp::add_on_ready(const std::vector<Stmt> *body, const Context &ctx) {
    // WorldModel.AddOnReady: queue only while a prompt callback is
    // outstanding; otherwise run immediately. The count is held >0 during the
    // run so nested `on ready` statements queue instead of recursing.
    if (pending_callback_count_ > 0) {
        on_ready_.emplace_back(body, ctx);
        return;
    }
    begin_pending_callback();
    Context c = ctx;
    try {
        run_callback_boundary(body, c);
    } catch (...) {
        // AddOnReady's finally: the pending count is released (and the queue
        // flushed) even when the callback throws past its boundary -- the
        // depth-cap throw and TurnSuspended both do -- then the throw
        // continues to the enclosing boundary.
        end_pending_callback();
        throw;
    }
    end_pending_callback();
}

void Interp::end_pending_callback() {
    --pending_callback_count_;
    // EndPendingCallbackAsync: flush the on-ready queue, FIFO, while nothing
    // pends. A flushed callback may itself register a prompt (show menu inside
    // on ready) -- the count check stops the flush; resolving that prompt
    // continues it.
    while (pending_callback_count_ == 0 && !on_ready_.empty() &&
           !world_.finished) {
        auto item = on_ready_.front();
        on_ready_.erase(on_ready_.begin());
        Context ctx = item.second;
        run_callback_boundary(item.first, ctx);
    }
}

void Interp::drain_on_ready() {
    // Manual flush for hosts/tests. Anything still queued while no prompt
    // pends (e.g. the flush stopped early because the game finished) runs now;
    // with a prompt outstanding this is a no-op -- resolving the prompt
    // flushes the queue itself.
    if (pending_callback_count_ > 0) return;
    while (!on_ready_.empty()) {
        auto item = on_ready_.front();
        on_ready_.erase(on_ready_.begin());
        Context ctx = item.second;
        run_callback_boundary(item.first, ctx);
        if (script_errors_fatal_) break;
    }
}

void Interp::fire_changed_script(Element *e, const std::string &attr,
                                 const Value &oldval) {
    // Element.SetFieldAsync: fires on every script assignment (no same-value
    // check), with `oldvalue` and this = the element; RunScriptAsync is the
    // error boundary, which run_script provides.
    const Value *scr = resolve_field(e, "changed" + attr);
    if (!scr || scr->type != Value::Type::Script) return;
    Context local;
    local.locals["oldvalue"] = oldval;
    local.locals["this"] = vobj(e->name);
    run_script(scr->str, local);
}

void Interp::print_via_core(const std::string &text, Context &ctx) {
    // WorldModel.PrintAsync: v540+ routes through Core's OutputText so the
    // {...} text processor runs; failures inside its body report at the
    // script boundary as usual, while bypass throws (depth cap) are logged
    // only. Without Core (unit tests) or on older games, print directly.
    // OutputText ends at JS.addText -> print.
    Element *ot = world_.find("OutputText");
    if (world_.asl_version >= 540 && ot && ot->elem_type == "function") {
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
        if (!text.empty()) print(text + "\n");
    }
}

// Utility.SafeXML -- also the safexml builtin.
static std::string safe_xml_escape(const std::string &s) {
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
        command_override_ = false;
        PendingCallback cb = std::move(command_cb_);
        command_cb_ = PendingCallback{};
        cb.ctx.locals["result"] = vstr(command);
        run_callback_boundary(cb.body, cb.ctx);
        end_pending_callback();
        return;
    }
    Context ctx;
    try {
        if (world_.asl_version < 520) {
            // Pre-v520 games expect the engine to echo the command.
            print_via_core("", ctx);
            print_via_core("> " + safe_xml_escape(command), ctx);
        }
        Element *hc = world_.find("HandleCommand");
        if (hc && hc->elem_type == "function") {
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
            Element *ft = world_.find("FinishTurn");
            if (ft && ft->elem_type == "function") {
                try {
                    call_function("FinishTurn", {}, &ctx);
                } catch (const std::exception &err) {
                    // TryFinishTurnAsync's catch: LogException-only.
                    log_exception(err.what());
                }
            }
        }
        // HandleCommandAsyncInternal ends the turn with a pane refresh (its
        // std::exception throws are LogException'd inside update_lists).
        if (!world_.finished) update_lists();
    } catch (const TurnSuspended &) {
        // Synchronous `play sound` mid-command: the rest of the turn --
        // including FinishTurn -- is abandoned (the QuestViva await chain
        // never resumes headless).
    }
}

void Interp::set_menu_response(const std::string *key) {
    if (!menu_pending_) return;
    menu_pending_ = false;
    MenuData md = std::move(menu_);
    menu_ = MenuData{};
    PendingCallback cb = std::move(menu_cb_);
    menu_cb_ = PendingCallback{};
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
    run_callback_boundary(cb.body, cb.ctx);
    end_pending_callback();
    // WorldModel.SetMenuResponse: pane refresh once the chain resolved.
    try {
        if (!world_.finished) update_lists();
    } catch (const TurnSuspended &) {}
}

void Interp::set_question_response(bool response) {
    if (!question_pending_) return;
    question_pending_ = false;
    question_.clear();
    PendingCallback cb = std::move(question_cb_);
    question_cb_ = PendingCallback{};
    cb.ctx.locals["result"] = vbool(response);
    run_callback_boundary(cb.body, cb.ctx);
    end_pending_callback();
    // WorldModel.SetQuestionResponse: pane refresh once the chain resolved.
    try {
        if (!world_.finished) update_lists();
    } catch (const TurnSuspended &) {}
}

void Interp::finish_wait() {
    if (!wait_pending_) return;
    wait_pending_ = false;
    PendingCallback cb = std::move(wait_cb_);
    wait_cb_ = PendingCallback{};
    run_callback_boundary(cb.body, cb.ctx);
    end_pending_callback();
    // WorldModel.FinishWait: refresh the panes once the callback chain
    // resolved (a park inside update_lists just abandons the refresh).
    try {
        if (!world_.finished) update_lists();
    } catch (const TurnSuspended &) {}
}

void Interp::send_event(const std::string &name, const std::string &param) {
    // WorldModel.SendEventCore -- the ASLEvent bridge (hyperlink onclicks).
    Context ctx;
    Element *h = world_.find(name);
    if (!h || h->elem_type != "function") {
        print_via_core("Error - no handler for event '" + name + "'", ctx);
        return;
    }
    try {
        call_function(name, {vstr(param)}, &ctx);
        // SendEventCore's version switch: pre-540 stops after the handler;
        // 540..579 runs TryFinishTurnAsync (LogException-only, like
        // send_command); then the panes refresh.
        if (world_.asl_version < 540) return;
        if (world_.asl_version < 580) {
            Element *ft = world_.find("FinishTurn");
            if (ft && ft->elem_type == "function") {
                try {
                    call_function("FinishTurn", {}, &ctx);
                } catch (const std::exception &err) {
                    log_exception(err.what());
                }
            }
        }
        if (!world_.finished) update_lists();
    } catch (const TurnSuspended &) {
        // synchronous `play sound`: the rest of the event chain is abandoned
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
    Element *f = world_.find(scope);
    if (!f || f->elem_type != "function")
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
    Element *gda = world_.find("GetDisplayAlias");
    d.display_alias = gda && gda->elem_type == "function"
        ? to_string(call_function("GetDisplayAlias", {vobj(obj->name)}, &ctx))
        : obj->name;

    // GetListDisplayAliasAsync: the pane label (may carry {}-processed markup).
    Element *glda = world_.find("GetListDisplayAlias");
    d.text = glda && glda->elem_type == "function"
        ? to_string(call_function("GetListDisplayAlias", {vobj(obj->name)}, &ctx))
        : d.display_alias;

    // Verbs: pre-v520 (or Core-less) reads the inventoryverbs/displayverbs
    // fields directly; later Core supplies GetDisplayVerbs.
    Element *gdv = world_.find("GetDisplayVerbs");
    if (world_.asl_version <= 520 || !gdv || gdv->elem_type != "function") {
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

std::vector<ListData> Interp::exits_list_data() {
    // GetExitsListDataAsync: ScopeExits, or GetExitsList on v530+.
    std::string scope = "ScopeExits";
    Element *gel = world_.find("GetExitsList");
    if (world_.asl_version >= 530 && gel && gel->elem_type == "function")
        scope = "GetExitsList";
    std::vector<ListData> out;
    for (Element *e : objects_in_scope(scope))
        out.push_back(list_data_for(e, false));
    return out;
}

void Interp::update_status_variables() {
    // UpdateStatusVariablesAsync: Core's UpdateStatusAttributes ends in
    // JS.updateStatus. Its own catch is LogException-only.
    Element *f = world_.find("UpdateStatusAttributes");
    if (!f || f->elem_type != "function") return;
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
        if (up->elem_type == "timer" && world_.find(up->name) == up.get())
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
    // hence the log_field_set calls here and below.
    if (Element *game = world_.find("game")) {
        Value v = vint(t);
        log_field_set(game, "timeelapsed", v, false);
        game->set_field("timeelapsed", v);
    }
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
        log_field_set(t, "trigger", v, false);
        t->set_field("trigger", v);
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
            Value v = vint(field_int(t, "trigger") + field_int(t, "interval"));
            log_field_set(t, "trigger", v, false);
            t->set_field("trigger", v);
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
            run_script(d.second, local);
        } catch (const TurnSuspended &) {
            // A suspended timer script suspends the whole batch
            // (TickAsyncInternal's await chain) -- including the pane refresh.
            suspended = true;
            break;
        }
    }
    // TickAsyncInternal refreshes the panes after the batch (inside the same
    // LogException-only try, which update_lists provides itself).
    if (!suspended) {
        try {
            update_lists();
        } catch (const TurnSuspended &) {}
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
    Element *def = in.world().find(impl->declared_type);
    const Value *pn = def ? def->field("paramnames") : nullptr;
    for (size_t k = 0; k < params.size(); ++k) {
        if (pn && k < pn->list().size())
            local.locals[Interp::to_string(pn->list()[k])] = params[k];
    }
    Value self; self.type = Value::Type::ObjectRef; self.str = obj->name;
    local.locals["this"] = self;
    in.run_script(impl->str, local);
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

// Template lookups live in aslx-runtime-builtins.inc (same TU, included below).
static const std::string *find_template(World &w, const std::string &name);
static const std::string *find_dyn_template(World &w, const std::string &name);

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
        return;
    }
    case UndoAction::Kind::FieldRemove: {
        Element *e = world_.find(a.element);
        if (e) e->set_field(a.attr, a.old_value);
        return;
    }
    case UndoAction::Kind::SortIndex: {
        Element *e = world_.find(a.element);
        if (e) e->sort_index = a.index;
        return;
    }
    case UndoAction::Kind::Create:
        // CreateDestroyLogEntry undo-of-create: Elements.Remove -- the storage
        // stays alive (like our destroy_element), only the name unregisters.
        world_.by_name.erase(a.element);
        return;
    case UndoAction::Kind::Destroy:
        // Undo-of-destroy: re-register the kept-alive element, fields intact.
        world_.by_name[a.element] = a.element_ptr;
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

// -- expression evaluation --------------------------------------------------

// Walk the field chain (own field, then inherited types most-recent-first,
// depth-first) collecting the base value -- the first NON-extend field -- and
// every `listextend` field encountered anywhere in the chain, in traversal
// order (most-derived first). Mirrors QuestViva Fields.Get: extendable fields
// live outside normal resolution (m_extendableFields) and merge on read.
static void collect_field_chain(World &w, Element *e, const std::string &name,
                                const Value *&base,
                                std::vector<const Value *> &exts) {
    if (!e) return;
    if (const Value *own = e->field(name)) {
        if (own->list_extend) exts.push_back(own);
        else if (!base) base = own;
    }
    for (auto it = e->inherits.rbegin(); it != e->inherits.rend(); ++it)
        collect_field_chain(w, w.find(*it), name, base, exts);
}

const Value *Interp::resolve_field(Element *e, const std::string &name) {
    if (!e) return nullptr;
    const Value *base = nullptr;
    std::vector<const Value *> exts;
    collect_field_chain(world_, e, name, base, exts);
    if (exts.empty()) return base;  // fast path: no listextend in the chain
    // Merge on read (Fields.GetMergedResult): the base list's entries first,
    // then each extension least-derived to most-derived (QuestList.MergeLists
    // accumulates parent-first). A non-list base is ignored, like QuestViva's
    // null extendableBaseField. The merged Value lives in a per-(element,attr)
    // slot so the returned pointer stays valid; it is rebuilt on every read
    // (QuestViva re-merges each Get too).
    Value merged;
    merged.type = (base && (base->type == Value::Type::StringList ||
                            base->type == Value::Type::ObjectList))
                      ? base->type : exts.front()->type;
    if (base && (base->type == Value::Type::StringList ||
                 base->type == Value::Type::ObjectList))
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
    // ASLX_TRACE_CALLS=1: stream every function invocation with the current
    // script depth, matching the oracle's QVH_TRACE_CALLS=2 format, to diff
    // depth accounting frame-for-frame (the depth cap decides which sub-calls
    // die during error-cascade unwinds).
    static const bool trace_calls = std::getenv("ASLX_TRACE_CALLS") != nullptr;
    if (trace_calls)
        fprintf(stderr, "[call d%d] %s\n", script_depth_, name.c_str());
    Element *fn = world_.find(name);
    if (!fn || fn->elem_type != "function") {
        error("Function not found: '" + name + "'");
        return vnull();
    }
    Context local;
    const Value *pn = fn->field("paramnames");
    if (pn) {
        for (size_t i = 0; i < pn->list().size() && i < args.size(); ++i)
            local.locals[to_string(pn->list()[i])] = args[i];
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
        if (const Value *inh = resolve_field(el, e.str)) {
            // The copy-down creates an own attribute: log it as an added
            // field set so a rollback removes it (back to the inherited one).
            log_field_set(el, e.str, *inh, /*removing=*/false);
            return &el->set_field(e.str, *inh);
        }
        log_field_set(el, e.str, Value{}, /*removing=*/false);
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
    // to the output sink. JS.setPanelContents is the picture frame
    // (SetFramePicture/ClearFramePicture wrap it, and OnEnterRoom sets it from
    // the room's `picture` attribute) -- routed to the set_panel_contents host
    // hook when one is installed, untouched (arg unevaluated) otherwise.
    // Everything else is a UI side effect we can ignore in the headless/native
    // core (a later presentation milestone wires the rest).
    if (name.compare(0, 3, "JS.") == 0) {
        std::string fn = name.substr(3);
        if (fn == "addText" && !args.empty())
            print(to_string(ev(0)));
        else if (fn == "setPanelContents" && !args.empty() && set_panel_contents)
            set_panel_contents(to_string(ev(0)));
        else if (fn == "updateStatus" && !args.empty() && update_status)
            update_status(to_string(ev(0)));
        else if (fn == "updateLocation" && !args.empty() && update_location)
            update_location(to_string(ev(0)));
        else if (fn == "eval" && !args.empty() && request_restart) {
            /* The restart channel: Core's `restart` command evals
             * "window.location.reload();" (older Cores first probe the
             * desktop player's RestartGame()).  That reload IS the restart
             * -- route it to the host.  Everything else this eval channel
             * carries (transcript flags, jQuery pane tweaks) stays ignored;
             * the argument is only evaluated with a hook installed, so
             * headless transcripts are untouched. */
            std::string js = to_string(ev(0));
            if (js.find("location.reload") != std::string::npos ||
                js.find("RestartGame") != std::string::npos)
                request_restart();
        } else if (grid_draw) {
            /* The grid-map paint vocabulary (CoreGrid.aslx -> grid.js),
             * forwarded as GridDraw commands. Argument evaluation only
             * happens down here, hook set -- the headless path above stays
             * untouched. Guarding numbers through as_double keeps a game
             * that passes junk from crashing the bridge (grid.js would have
             * silently drawn NaNs). */
            auto num = [&](size_t i) { return as_double(ev(i)); };
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
                g.border = to_string(ev(5));
                g.borderwidth = (int)num(6);
                g.fill = to_string(ev(7));
                g.sides = (int)num(8);
                grid_draw(g);
            } else if (fn == "Grid_DrawLabel") {
                g.op = GridDraw::Op::Label;
                g.x = num(0); g.y = num(1); g.z = (int)num(2);
                g.text = to_string(ev(3));
                g.fill = args.size() > 4 ? to_string(ev(4)) : "black";
                grid_draw(g);
            } else if (fn == "Grid_DrawLine") {
                g.op = GridDraw::Op::Line;
                g.x = num(0); g.y = num(1); g.x2 = num(2); g.y2 = num(3);
                g.border = to_string(ev(4));
                g.borderwidth = (int)num(5);
                grid_draw(g);
            } else if (fn == "Grid_DrawPlayer") {
                g.op = GridDraw::Op::Player;
                g.x = num(0); g.y = num(1); g.z = (int)num(2);
                g.w = num(3);
                g.border = to_string(ev(4));
                g.borderwidth = (int)num(5);
                g.fill = to_string(ev(6));
                grid_draw(g);
            } else if (fn == "Grid_ClearAllLayers") {
                g.op = GridDraw::Op::Clear;
                grid_draw(g);
            }
        }
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

    // play sound (file, synchronous, loop) -- PlaySoundScript.ExecuteAsync
    // evaluates all three in that order, then hands them to the UI.
    if (name == "play sound") {
        std::string filename = !args.empty() ? to_string(ev(0)) : "";
        bool sync = args.size() >= 2 && truthy(ev(1));
        bool loop = args.size() >= 3 && truthy(ev(2));
        if (sync && wait_pending_) {
            // A synchronous play claims the wait slot (BeginPrompt on
            // _waitTcs): a pending `wait` is cancelled -- its callback never
            // runs. Note no begin_pending_callback -- PlaySoundScript never
            // counts itself, so `on ready` is not deferred by the sound.
            wait_pending_ = false;
            wait_cb_ = PendingCallback{};
            end_pending_callback();
        }
        if (play_sound) {
            // The host plays it; a synchronous host BLOCKS in the hook until
            // playback finishes, so the rest of the turn resumes exactly
            // where QuestViva's awaited wait slot would resume it.
            play_sound(filename, sync, loop);
            return true;
        }
        warn_once(name, "'" + name + "' is not supported yet; ignored");
        if (sync) {
            // With no UI to report the sound finished, everything after this
            // statement in the turn is abandoned. Mirror that exactly: the
            // suspension unwinds to the turn boundary, unreported.
            throw TurnSuspended{};
        }
        return true;
    }

    if (name == "stop sound") {   // StopSoundScript: no args
        if (stop_sound)
            stop_sound();
        else
            warn_once(name, "'" + name + "' is not supported yet; ignored");
        return true;
    }

    // request (RequestType, data): a player-UI request (RequestScript). The
    // first argument is a bare enum identifier (Speak, Quit, Show, ...) that is
    // not a resolvable expression, so we must NOT evaluate the args -- headless
    // ignores UI requests (a later presentation milestone wires the handful Core
    // relies on). Same for `request` with a callback and RequestSave.
    if (name == "request" || name == "requestsave") {
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
        if (req == "RequestSave") {
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
        } else if (req == "UpdateLocation" && update_location) {
            // PlayerUI.LocationUpdated -- same pre-JS pairing as SetStatus.
            update_location(to_string(ev(1)));
        } else if (req == "SetPanelContents" && set_panel_contents) {
            // PlayerUI.SetPanelContents -- the picture frame, like
            // JS.setPanelContents.
            set_panel_contents(to_string(ev(1)));
        }
        return true;
    }

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
    if (name == "create exit") {  // CreateExitScript / ObjectFactory.CreateExit
        // 3 args: (alias, from, to); 4: (alias, from, to, initialType);
        // 5: (id, alias, from, to, initialType). No id -> a generated
        // "exitN" name and the anonymous flag.
        size_t base = args.size() >= 5 ? 1 : 0;
        std::string alias = to_string(ev(base));
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
        exit->set_field("alias", vstr(alias));
        if (from.type == Value::Type::ObjectRef)
            exit->set_field("parent", from);
        exit->set_field("to", to);
        if (anonymous) exit->set_field("anonymous", vbool(true));
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
        if (obj) {
            std::string attr = to_string(ev(1));
            const Value *prev = resolve_field(obj, attr);
            Value old = prev ? *prev : vnull();
            Value nv = ev(2);
            // Clone-on-set for collections -- see the Assign case.
            const Value *own = obj->field(attr);
            bool same_backing = own && own->list_store == nv.list_store &&
                                own->dict_store == nv.dict_store;
            if (!same_backing) nv.detach();
            // v530+ null assignment removes the attribute -- see Assign.
            if (world_.asl_version >= 530 && nv.type == Value::Type::Null) {
                log_field_set(obj, attr, nv, /*removing=*/true);
                obj->remove_field(attr);
            } else {
                log_field_set(obj, attr, nv, /*removing=*/false);
                obj->set_field(attr, nv);
            }
            // Same-value writes reorder too -- see the Assign case.
            if (attr == "parent") {
                log_sort_index(obj);
                obj->sort_index = world_.next_sort_index++;
            }
            fire_changed_script(obj, attr, old);
        } else {
            errors().push_back("set: not an object");
        }
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
        if (name == "list add") {
            // The value is stored boxed/typed verbatim (QuestList<object>.Add)
            // -- a list can hold dictionaries, objects, numbers... (spondre).
            auto &v = lst->list();
            if (undo_logging_) {
                UndoAction a;
                a.kind = UndoAction::Kind::ListAdd;
                a.list_backing = lst->list_store;
                a.old_value = item;
                a.index = (long)v.size();
                add_undo(std::move(a));
            }
            v.push_back(std::move(item));
        } else {
            // QuestList.Remove: first occurrence only (List<T>.Remove).
            auto &v = lst->list();
            for (auto i = v.begin(); i != v.end(); ++i)
                if (values_equal(*i, item)) {
                    if (undo_logging_) {
                        UndoAction a;
                        a.kind = UndoAction::Kind::ListRemove;
                        a.list_backing = lst->list_store;
                        a.old_value = *i;
                        a.index = (long)(i - v.begin());
                        add_undo(std::move(a));
                    }
                    v.erase(i);
                    break;
                }
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
            if (it->first == key) {
                if (undo_logging_) {
                    UndoAction a;
                    a.kind = UndoAction::Kind::DictRemove;
                    a.dict_backing = d->dict_store;
                    a.attr = key;
                    a.old_value = it->second;
                    a.index = (long)(it - d->dict().begin());
                    add_undo(std::move(a));
                }
                it = d->dict().erase(it);
            } else {
                ++it;
            }
        }
        if (name == "dictionary add") {
            if (undo_logging_) {
                UndoAction a;
                a.kind = UndoAction::Kind::DictAdd;
                a.dict_backing = d->dict_store;
                a.attr = key;
                a.index = (long)d->dict().size();
                add_undo(std::move(a));
            }
            d->dict().emplace_back(key, ev(2));  // store the typed value verbatim
        }
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

// The statement parser and built-ins are large; they live in a second unit that
// is included here so this file stays the single translation unit for the
// runtime (the test unity-includes it).
#include "aslx-runtime-parse.inc"
#include "aslx-runtime-builtins.inc"
#include "aslx-state.inc"
