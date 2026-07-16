// aslx-runtime-parse.inc -- statement parser for the Quest 5 script language.
// Textually included at the end of aslx-runtime.cc (same translation unit, so it
// sees Lexer/Parser/Stmt/Expr and the tokenizer helpers). Mirrors QuestViva's
// ScriptFactory.CreateScript statement splitting + keyword dispatch.

namespace aslx {

// Substring after the first occurrence of `kw`.
static std::string text_after(const std::string &s, const std::string &kw) {
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
static std::string get_parameter_wrap(const std::string &s, std::string &after);

// Parse a block of script source into a statement list.
static std::vector<Stmt> parse_statements(const std::string &src, Interp &interp) {
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
                    std::string a;
                    std::string body =
                        get_script(rt_trim(stmt.substr(9)), a);
                    ft.else_body = parse_statements(body, interp);
                    ft.has_else = true;
                } else {
                    interp.errors().push_back("Unexpected 'otherwise': " + stmt);
                }
            } else if (starts_with_word(stmt, "else")) {
                // Attach to the most recent If.
                Stmt *last = nullptr;
                for (auto it = result.rbegin(); it != result.rend(); ++it) {
                    if (it->kind == Stmt::Kind::If) { last = &*it; break; }
                    break;  // else must immediately follow its if
                }
                if (last) {
                    std::string rest = rt_trim(text_after(stmt, "else"));
                    if (starts_with_word(rest, "if")) {
                        std::string a2, a3;
                        std::string cond = get_parameter_wrap(rest, a2);
                        std::string body = get_script(a2, a3);
                        last->elseifs.emplace_back(compile_expr_str(cond),
                                                   parse_statements(body, interp));
                    } else {
                        last->else_body = parse_statements(rest, interp);
                        last->has_else = true;
                    }
                } else {
                    interp.errors().push_back("Unexpected 'else': " + stmt);
                }
            } else {
                result.push_back(parse_one_statement(stmt, interp));
            }
        }

        line = after;
        if (rt_trim(line).empty()) break;
    }
    return result;
}

// Small wrapper so the else-if path reads cleanly.
static std::string get_parameter_wrap(const std::string &s, std::string &after) {
    bool found;
    return get_parameter(s, after, found);
}

static Stmt parse_one_statement(const std::string &line, Interp &interp) {
    Stmt s;

    if (starts_with_word(line, "//")) { s.kind = Stmt::Kind::Comment; return s; }

    if (starts_with_word(line, "msg")) {
        s.kind = Stmt::Kind::Msg;
        std::string after; bool found;
        std::string p = get_parameter(line, after, found);
        s.expr = compile_expr_str(found ? p : std::string("\"\""));
        return s;
    }
    if (starts_with_word(line, "return")) {
        s.kind = Stmt::Kind::Return;
        std::string after; bool found;
        std::string p = get_parameter(line, after, found);
        if (found && !rt_trim(p).empty()) s.expr = compile_expr_str(p);
        return s;
    }
    if (starts_with_word(line, "if")) {
        s.kind = Stmt::Kind::If;
        std::string after; bool found;
        std::string cond = get_parameter(line, after, found);
        std::string bafter; std::string body = get_script(after, bafter);
        s.expr = compile_expr_str(cond);
        s.body = parse_statements(body, interp);
        return s;
    }
    if (starts_with_word(line, "while")) {
        s.kind = Stmt::Kind::While;
        std::string after; bool found;
        std::string cond = get_parameter(line, after, found);
        std::string bafter; std::string body = get_script(after, bafter);
        s.expr = compile_expr_str(cond);
        s.body = parse_statements(body, interp);
        return s;
    }
    if (starts_with_word(line, "foreach")) {
        s.kind = Stmt::Kind::ForEach;
        std::string after; bool found;
        std::string param = get_parameter(line, after, found);
        std::string bafter; std::string body = get_script(after, bafter);
        auto parts = split_parameters(param);
        if (parts.size() == 2) {
            s.name = parts[0];
            s.list = compile_expr_str(parts[1]);
        } else {
            interp.errors().push_back("'foreach' needs 2 parameters");
        }
        s.body = parse_statements(body, interp);
        return s;
    }
    if (starts_with_word(line, "for")) {
        s.kind = Stmt::Kind::For;
        std::string after; bool found;
        std::string param = get_parameter(line, after, found);
        std::string bafter; std::string body = get_script(after, bafter);
        auto parts = split_parameters(param);
        if (parts.size() == 3 || parts.size() == 4) {
            s.name = parts[0];
            s.from = compile_expr_str(parts[1]);
            s.to = compile_expr_str(parts[2]);
            if (parts.size() == 4) s.step = compile_expr_str(parts[3]);
        } else {
            interp.errors().push_back("'for' needs 3 or 4 parameters");
        }
        s.body = parse_statements(body, interp);
        return s;
    }

    if (starts_with_word(line, "switch")) {
        s.kind = Stmt::Kind::Switch;
        std::string after; bool found;
        std::string cond = get_parameter(line, after, found);
        s.expr = compile_expr_str(cond);
        std::string bafter;
        std::string block = remove_surrounding_braces(get_script(after, bafter));
        std::string cases = block;
        while (true) {
            std::string rem;
            std::string cs = rt_trim(get_script(cases, rem));
            if (!cs.empty()) {
                if (starts_with_word(cs, "case")) {
                    std::string a2, a3; bool f2;
                    std::string expr = get_parameter(cs, a2, f2);
                    std::string cbody = get_script(a2, a3);
                    std::vector<ExprP> matches;
                    for (const std::string &m : split_parameters(expr))
                        matches.push_back(compile_expr_str(m));
                    s.cases.emplace_back(std::move(matches),
                                         parse_statements(cbody, interp));
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
        s.kind = Stmt::Kind::FirstTime;
        std::string a;
        std::string body = get_script(rt_trim(line.substr(9)), a);
        s.body = parse_statements(body, interp);
        s.ran = std::make_shared<bool>(false);
        return s;
    }
    if (line.compare(0, 8, "on ready") == 0 &&
        (line.size() == 8 || !std::isalnum((unsigned char)line[8]))) {
        s.kind = Stmt::Kind::OnReady;
        std::string a;
        std::string body = get_script(rt_trim(line.substr(8)), a);
        s.body = parse_statements(body, interp);
        return s;
    }
    if (starts_with_word(line, "wait")) {
        s.kind = Stmt::Kind::Wait;
        std::string a;
        std::string body = get_script(rt_trim(line.substr(4)), a);
        s.body = parse_statements(body, interp);
        return s;
    }

    // Assignment: "x = expr", "obj.prop = expr", or the script-literal form
    // "x => { script }" (which stores the RHS as a Script value, SetScriptScript).
    bool is_script_assign = false;
    size_t eq = assign_eq_pos(line, is_script_assign);
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
        std::string after; bool found;
        std::string param = get_parameter(line, after, found);
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
