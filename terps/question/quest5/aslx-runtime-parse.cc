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

// aslx-runtime-parse.cc -- script source text to AST for the Quest 5 script
// language: the tokenizer helpers, the expression lexer + parser, and the
// statement parser. Mirrors QuestViva's Utility.cs splitting helpers, its NCalc
// expression grammar, and ScriptFactory.CreateScript's statement splitting +
// keyword dispatch.

#include "aslx-runtime-internal.hh"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace aslx {

// ===========================================================================
// Tokenizer helpers, mirroring QuestViva Utility.cs
// ===========================================================================

// Replace the contents of every double-quoted string with '-' so structural
// scanning (braces, newlines, "//") ignores anything inside a literal. A
// backslash escapes the next character -- in particular "\"" is a literal quote,
// not a terminator -- matching QuestViva's Utility.SplitQuotes (the backslash
// "don't process the next character" rule). Length is preserved so positions in
// the obscured string map back to the original.
std::string obscure_strings(const std::string &in) {
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

std::string get_parameter(const std::string &script, std::string &after,
                                 bool &found) {
    return extract_balanced(script, '(', ')', after, found);
}

// Split a parameter list on top-level commas, respecting strings and nested
// parens; backslash escapes the next char. Mirrors Utility.SplitParameter.
std::vector<std::string> split_parameters(const std::string &text) {
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
std::string get_script(const std::string &script, std::string &after) {
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

std::string remove_surrounding_braces(const std::string &input) {
    std::string s = rt_trim(input);
    if (s.size() >= 2 && s.front() == '{' && s.back() == '}')
        return s.substr(1, s.size() - 2);
    return s;
}

// Strip // line comments, respecting string literals (a "//" inside a quoted
// string is not a comment). Uses obscure_strings so backslash-escaped quotes are
// handled the same way the statement splitter handles them.
std::string remove_comments(const std::string &input) {
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
// Expression parser
// ===========================================================================

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
            // used by QuestNCalcLogicalExpressionParser. Core relies on \", \' and
            // \\ (the last so "\\D" yields the regex \D); an unknown escape keeps
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
        t.num = c_strtod(n.c_str());
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
        static const char *twos[] = {"<>", "!=", "==", ">=", "<=", "<<", ">>",
                                     nullptr};
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

// A fresh AST node of `kind`, with its name / operator / literal text.
ExprP node(Expr::Kind kind, std::string str = std::string()) {
    auto e = std::make_shared<Expr>();
    e->kind = kind;
    e->str = std::move(str);
    return e;
}

struct Parser {
    std::vector<Tok> toks;
    size_t p = 0;
    int depth = 0;

    explicit Parser(std::vector<Tok> t) : toks(std::move(t)) {}

    // Recursion cap for the descent: without it 100k nested "(" or "not"s
    // overflow the stack when the expression is first (lazily) compiled. The
    // guard sits on every self-recursing production; the fail unwinds to the
    // normal parse-error path.
    struct DepthGuard {
        Parser &ps;
        explicit DepthGuard(Parser &pr) : ps(pr) {
            if (++ps.depth > 200)
                ps.fail("expression is nested more than 200 levels deep");
        }
        ~DepthGuard() { --ps.depth; }
    };

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
        DepthGuard dg(*this);
        ExprP cond = parse_logical();
        if (is_op("?")) {
            advance();
            ExprP a = parse_logical();
            if (!is_op(":")) fail("expected ':'");
            advance();
            ExprP b = parse_logical();
            ExprP e = node(Expr::Kind::Ternary);
            e->a = cond; e->b = a; e->c = b;
            return e;
        }
        return cond;
    }

    ExprP bin(const std::string &op, ExprP l, ExprP r) {
        ExprP e = node(Expr::Kind::Binary, op);
        e->a = l; e->b = r;
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
            DepthGuard dg(*this);
            advance();
            ExprP e = node(Expr::Kind::Unary, "not");
            e->a = parse_not();
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
        ExprP l = parse_shift();
        while (true) {
            if (is_op(">") || is_op("<") || is_op(">=") || is_op("<=")) {
                std::string op = cur().s; advance();
                l = bin(op, l, parse_shift());
                continue;
            }
            // "in" / "not in" sit at the relational level in QuestViva's
            // grammar (QuestNCalcLogicalExpressionParser: relational => shift
            // ((">=" | "<=" | "<" | ">" | "in" | "not in") shift)*).
            if (is_kw("in")) {
                advance();
                l = bin("in", l, parse_shift());
                continue;
            }
            if (is_kw("not") && p + 1 < toks.size() &&
                toks[p + 1].t == Tok::T::Ident && toks[p + 1].s == "in") {
                advance(); advance();
                l = bin("not in", l, parse_shift());
                continue;
            }
            break;
        }
        return l;
    }
    // shift => additive (("<<" | ">>") additive)*, left-associative, between
    // relational and additive as in NCalc's grammar. Oracle-verified
    // (NCalcAsync 6.3.2): "2 << 1 + 1" is 8, "1 << 2 > 3" is True,
    // "1 << 1 << 2" is 8. Deeper's keyring bitmask: "player.keyring + (1 << n)".
    ExprP parse_shift() {
        ExprP l = parse_additive();
        while (is_op("<<") || is_op(">>")) {
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
        // Only arithmetic negation binds tightly here; logical "not" is handled
        // at parse_not (looser than equality).
        if (is_op("-")) {
            DepthGuard dg(*this);
            advance();
            ExprP e = node(Expr::Kind::Unary, "-");
            e->a = parse_unary();
            return e;
        }
        return parse_power();
    }
    ExprP parse_power() {
        // NCalc's "^" is exponentiation (not XOR), right-associative, and it
        // binds TIGHTER than unary minus: "-2^2" is -4 and "2^3^2" is 512
        // (both verified against the oracle). Moquette's GetRandomPoisson
        // opens with "L = e ^ -expected", so the right operand must be able to
        // be a unary expression too.
        ExprP l = parse_postfix();
        if (is_op("^")) {
            DepthGuard dg(*this);
            advance();
            l = bin("^", l, parse_unary());
        }
        return l;
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
                    ExprP call = node(Expr::Kind::Call, name);
                    call->args = parse_args();
                    call->a = e;  // receiver kept for future use
                    e = call;
                } else {
                    ExprP m = node(Expr::Kind::Member, name);
                    m->a = e;
                    e = m;
                }
            } else if (is_op("[")) {
                advance();
                ExprP idx = parse_ternary();
                if (!is_op("]")) fail("expected ']'");
                advance();
                ExprP ix = node(Expr::Kind::Index);
                ix->a = e; ix->b = idx;
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
            ExprP e = node(Expr::Kind::Num);
            e->num = t.num; e->is_int = t.is_int;
            return e;
        }
        if (t.t == Tok::T::Str) {
            advance();
            return node(Expr::Kind::Str, t.s);
        }
        if (t.t == Tok::T::Ident) {
            // Boolean/null literals are case-insensitive ("True", "FALSE"):
            // NCalc's Terms.Text("true", true), and the null parameter check in
            // NcalcExpressionEvaluator.ResolveVariable.
            if (rt_iequals(t.s, "true") || rt_iequals(t.s, "false")) {
                advance();
                ExprP e = node(Expr::Kind::Bool);
                e->boolean = rt_iequals(t.s, "true");
                return e;
            }
            if (rt_iequals(t.s, "null")) {
                advance();
                return node(Expr::Kind::Null);
            }
            std::string name = t.s;
            advance();
            if (is_op("(")) {
                ExprP e = node(Expr::Kind::Call, name);
                e->args = parse_args();
                return e;
            }
            return node(Expr::Kind::Var, name);
        }
        if (is_op("(")) {
            // A group, or NCalc's list "(a, b, ...)" / "()" (',' or ';'
            // separated) -- FLEE's `x in (a, b)` membership form.
            advance();
            if (is_op(")")) {
                advance();
                return node(Expr::Kind::List);
            }
            ExprP e = parse_ternary();
            if (is_op(",") || is_op(";")) {
                ExprP l = node(Expr::Kind::List);
                l->args.push_back(e);
                while (is_op(",") || is_op(";")) {
                    advance();
                    l->args.push_back(parse_ternary());
                }
                e = l;
            }
            if (!is_op(")")) fail("expected ')'");
            advance();
            return e;
        }
        fail("unexpected token '" + t.s + "'");
    }
};

}  // namespace

// Compile an expression source string to an AST (used by the statement parser).
ExprP compile_expr_str(const std::string &src) {
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
ExprP compile_expr_str_deferred(const std::string &src) {
    try {
        return compile_expr_str(src);
    } catch (const std::exception &err) {
        ExprP e = node(Expr::Kind::ParseError, err.what());
        e->src = src;
        return e;
    }
}

// ===========================================================================
// Statement parser
// ===========================================================================

bool starts_with_word(const std::string &line, const std::string &kw) {
    if (line.compare(0, kw.size(), kw) != 0) return false;
    if (line.size() == kw.size()) return true;
    char n = line[kw.size()];
    return !(std::isalnum((unsigned char)n) || n == '_' ||
             (unsigned char)n >= 0x80);
}

// Substring after the first occurrence of `kw`.
std::string text_after(const std::string &s, const std::string &kw) {
    size_t p = s.find(kw);
    if (p == std::string::npos) return s;
    return s.substr(p + kw.size());
}

// Top-level '=' position for an assignment, ignoring ==, <=, >=, <>, != and
// anything from the first '{' onward. Returns npos if not an assignment.
// If the '=' begins a "=>" (a script-literal assignment, SetScriptScript),
// is_script is set: the RHS is a script body, not an expression.
static size_t assign_eq_pos(const std::string &line, bool &is_script) {
    is_script = false;
    std::string ob = obscure_strings(line);
    size_t brace = ob.find('{');
    if (brace != std::string::npos) ob = ob.substr(0, brace);
    // "=>" first, like SetScriptConstructor (it looks for => before =).
    size_t arrow = ob.find("=>");
    if (arrow != std::string::npos) {
        is_script = true;
        return arrow;
    }
    for (size_t i = 0; i < ob.size(); ++i) {
        if (ob[i] != '=') continue;
        char prev = i > 0 ? ob[i - 1] : 0;
        char next = i + 1 < ob.size() ? ob[i + 1] : 0;
        if (prev == '<' || prev == '>' || prev == '!' || prev == '=') continue;
        if (next == '=') continue;
        return i;
    }
    return std::string::npos;
}

static Stmt parse_one_statement(const std::string &line, Interp &interp);
static std::vector<Stmt> parse_block(const std::string &text, Interp &interp);

// Parse a block of script source into a statement list.
//
// The parse itself must be depth-capped like the runtime (kMaxScriptDepth):
// compilation is lazy, so a hostile 100k-deep `{...}` nest would otherwise
// overflow the stack the first time the script RUNS. The throw is caught at
// the enclosing statement (ParseError) or script boundary like any other
// parse failure.
static int g_stmt_parse_depth = 0;
std::vector<Stmt> parse_statements(const std::string &src, Interp &interp) {
    struct DepthGuard {
        DepthGuard() {
            if (++g_stmt_parse_depth > 200) {
                --g_stmt_parse_depth;
                throw std::runtime_error(
                    "script is nested more than 200 blocks deep");
            }
        }
        ~DepthGuard() { --g_stmt_parse_depth; }
    } depth_guard;
    std::vector<Stmt> result;
    std::string line = remove_comments(remove_surrounding_braces(src));

    while (true) {
        std::string after;
        std::string stmt = get_script(line, after);
        stmt = rt_trim(stmt);

        if (!stmt.empty()) {
            if (starts_with_word(stmt, "otherwise")) {
                // Attach to the immediately-preceding firsttime.
                if (!result.empty() &&
                    result.back().kind == Stmt::Kind::FirstTime) {
                    Stmt &ft = result.back();
                    ft.else_body = parse_block(rt_trim(stmt.substr(9)), interp);
                    ft.has_else = true;
                } else {
                    interp.errors().push_back("Unexpected 'otherwise': " + stmt);
                }
            } else if (starts_with_word(stmt, "else")) {
                // Attach to the If it must immediately follow.
                Stmt *last = !result.empty() &&
                                     result.back().kind == Stmt::Kind::If
                                 ? &result.back()
                                 : nullptr;
                if (last) {
                    std::string rest = rt_trim(text_after(stmt, "else"));
                    if (starts_with_word(rest, "if")) {
                        std::string after;
                        bool found;
                        std::string cond = get_parameter(rest, after, found);
                        // Deferred: an unparsable condition must only error if
                        // this else-if is actually REACHED (all prior branches
                        // false) -- see compile_expr_str_deferred.
                        ExprP test = compile_expr_str_deferred(cond);
                        last->elseifs.emplace_back(std::move(test),
                                                   parse_block(after, interp));
                    } else {
                        last->else_body = parse_statements(rest, interp);
                        last->has_else = true;
                    }
                } else {
                    interp.errors().push_back("Unexpected 'else': " + stmt);
                }
            } else {
                // One bad statement must not abort the whole body: QuestViva
                // parses statement-by-statement (lazily), so later statements
                // still load and the failure surfaces only if the bad one runs.
                try {
                    result.push_back(parse_one_statement(stmt, interp));
                } catch (const std::exception &err) {
                    Stmt bad;
                    bad.kind = Stmt::Kind::ParseError;
                    bad.name = err.what();
                    result.push_back(std::move(bad));
                }
            }
        }

        line = after;
        if (rt_trim(line).empty()) break;
    }
    return result;
}

// The statements of the first script block in `text` -- the "{ ... }" (or
// single statement) that follows a keyword and its parameter.
static std::vector<Stmt> parse_block(const std::string &text, Interp &interp) {
    std::string after;
    return parse_statements(get_script(text, after), interp);
}

// A two-word keyword ("on ready", "get input", "show menu") opening the line.
static bool starts_with_phrase(const std::string &line, const char *phrase) {
    size_t n = std::strlen(phrase);
    return line.compare(0, n, phrase) == 0 &&
           (line.size() == n || !std::isalnum((unsigned char)line[n]));
}

static Stmt parse_one_statement(const std::string &line, Interp &interp) {
    Stmt s;
    // The keyword's parenthesized parameter; `after` is what follows it.
    std::string after;
    bool found = false;
    auto parameter = [&] { return get_parameter(line, after, found); };
    // A keyword of `len` characters followed directly by its block.
    auto bare_block = [&](Stmt::Kind kind, size_t len) {
        s.kind = kind;
        s.body = parse_block(rt_trim(line.substr(len)), interp);
    };

    if (starts_with_word(line, "//")) { s.kind = Stmt::Kind::Comment; return s; }

    if (starts_with_word(line, "msg")) {
        s.kind = Stmt::Kind::Msg;
        std::string p = parameter();
        s.expr = compile_expr_str(found ? p : std::string("\"\""));
        return s;
    }
    if (starts_with_word(line, "return")) {
        s.kind = Stmt::Kind::Return;
        std::string p = parameter();
        if (found && !rt_trim(p).empty()) s.expr = compile_expr_str(p);
        return s;
    }
    bool is_if = starts_with_word(line, "if");
    if (is_if || starts_with_word(line, "while")) {
        s.kind = is_if ? Stmt::Kind::If : Stmt::Kind::While;
        s.expr = compile_expr_str(parameter());
        s.body = parse_block(after, interp);
        return s;
    }
    if (starts_with_word(line, "foreach")) {
        s.kind = Stmt::Kind::ForEach;
        auto parts = split_parameters(parameter());
        if (parts.size() == 2) {
            s.name = parts[0];
            s.list = compile_expr_str(parts[1]);
        } else {
            interp.errors().push_back("'foreach' needs 2 parameters");
        }
        s.body = parse_block(after, interp);
        return s;
    }
    if (starts_with_word(line, "for")) {
        s.kind = Stmt::Kind::For;
        auto parts = split_parameters(parameter());
        if (parts.size() == 3 || parts.size() == 4) {
            s.name = parts[0];
            s.from = compile_expr_str(parts[1]);
            s.to = compile_expr_str(parts[2]);
            if (parts.size() == 4) s.step = compile_expr_str(parts[3]);
        } else {
            interp.errors().push_back("'for' needs 3 or 4 parameters");
        }
        s.body = parse_block(after, interp);
        return s;
    }

    if (starts_with_word(line, "switch")) {
        s.kind = Stmt::Kind::Switch;
        s.expr = compile_expr_str(parameter());
        std::string bafter;
        std::string cases = remove_surrounding_braces(get_script(after, bafter));
        while (true) {
            std::string rem;
            std::string cs = rt_trim(get_script(cases, rem));
            if (!cs.empty()) {
                if (starts_with_word(cs, "case")) {
                    std::string cafter; bool cfound;
                    std::string expr = get_parameter(cs, cafter, cfound);
                    std::vector<ExprP> matches;
                    for (const std::string &m : split_parameters(expr))
                        matches.push_back(compile_expr_str(m));
                    s.cases.emplace_back(std::move(matches),
                                         parse_block(cafter, interp));
                } else if (starts_with_word(cs, "default")) {
                    s.else_body =
                        parse_statements(rt_trim(cs.substr(7)), interp);
                } else {
                    interp.errors().push_back("Invalid inside switch: " + cs);
                }
            }
            cases = rem;
            if (rt_trim(cases).empty()) break;
        }
        return s;
    }
    if (starts_with_word(line, "firsttime")) {
        bare_block(Stmt::Kind::FirstTime, 9);
        s.ran = std::make_shared<bool>(false);
        return s;
    }
    if (starts_with_phrase(line, "on ready")) {
        bare_block(Stmt::Kind::OnReady, 8);
        return s;
    }
    if (starts_with_word(line, "wait")) {
        bare_block(Stmt::Kind::Wait, 4);
        return s;
    }
    if (starts_with_phrase(line, "get input")) {
        bare_block(Stmt::Kind::GetInput, 9);
        return s;
    }
    if (starts_with_phrase(line, "show menu")) {
        // show menu (caption, options, allowCancel) { callback }
        std::string param = parameter();
        auto parts = split_parameters(param);
        if (parts.size() != 3) {
            interp.errors().push_back(
                "'show menu' script should have 3 parameters: 'show menu (" +
                param + ")'");
            s.kind = Stmt::Kind::Comment;
            return s;
        }
        s.kind = Stmt::Kind::ShowMenu;
        for (const std::string &p : parts)
            s.call_args.push_back(compile_expr_str(p));
        s.body = parse_block(after, interp);
        return s;
    }
    if (starts_with_word(line, "ask")) {
        // ask (caption) { callback }
        std::string param = parameter();
        if (!found || rt_trim(param).empty()) {
            interp.errors().push_back(
                "'ask' script should have 1 parameter: 'ask (" + param + ")'");
            s.kind = Stmt::Kind::Comment;
            return s;
        }
        s.kind = Stmt::Kind::Ask;
        s.expr = compile_expr_str(param);
        s.body = parse_block(after, interp);
        return s;
    }

    // ScriptFactory.CreateScript tries every KEYWORD constructor before the
    // SetScriptConstructor, so a keyword statement whose arguments contain a
    // bare '=' is a call, never an assignment -- GiantKiller Too's
    // `do (object, "ondrop", QuickParams ("successful", not oldparent = object.parent))`.
    // The keywords handled above are already dispatched; these are the ones
    // that fall through to the generic Call below (plus "JS." functions).
    static const char *const kCallKeywords[] = {
        "create exit", "create timer", "create turnscript", "create", "destroy",
        "dictionary add", "dictionary remove", "do", "error", "finish",
        "insert", "invoke", "list add", "list remove", "picture", "play sound",
        "request", "requestsave", "requestspeak", "rundelegate", "set",
        "start transaction", "stop sound", "undo",
    };
    bool keyword_call = line.compare(0, 3, "JS.") == 0;
    for (const char *k : kCallKeywords)
        if (!keyword_call && starts_with_word(line, k)) keyword_call = true;

    // Assignment: "x = expr", "obj.prop = expr", or the script-literal form
    // "x => { script }" (which stores the RHS as a Script value, SetScriptScript).
    bool is_script_assign = false;
    size_t eq = keyword_call ? std::string::npos
                             : assign_eq_pos(line, is_script_assign);
    if (eq != std::string::npos) {
        s.kind = Stmt::Kind::Assign;
        std::string lhs = rt_trim(line.substr(0, eq));
        std::string rhs = rt_trim(line.substr(eq + (is_script_assign ? 2 : 1)));
        size_t dot = lhs.rfind('.');
        if (dot == std::string::npos) {
            s.name = lhs;
        } else {
            s.obj = compile_expr_str(lhs.substr(0, dot));
            s.name = rt_trim(lhs.substr(dot + 1));
        }
        if (is_script_assign) s.script_text = rhs;
        else s.expr = compile_expr_str(rhs);
        return s;
    }

    // Otherwise a function call: "Name (args)", bare "Name", or either form
    // with a trailing "{ script }", which is passed as one extra script-literal
    // argument bound to the function's last parameter (FunctionCallScript's
    // paramFunction -- e.g. Core's "ShowMenu (...) { ... }" callback).
    s.kind = Stmt::Kind::Call;
    std::string ob = obscure_strings(line);
    size_t brace = ob.find('{');
    size_t paren = ob.find('(');
    if (paren != std::string::npos &&
        (brace == std::string::npos || paren < brace)) {
        std::string param = parameter();
        s.name = rt_trim(line.substr(0, paren));
        if (!rt_trim(param).empty())
            for (const std::string &a : split_parameters(param))
                s.call_args.push_back(compile_expr_str(a));
        s.script_text = rt_trim(after);   // "" when no block follows
    } else if (brace != std::string::npos) {
        s.name = rt_trim(line.substr(0, brace));
        s.script_text = rt_trim(line.substr(brace));
    } else {
        s.name = rt_trim(line);
    }
    return s;
}

}  // namespace aslx
