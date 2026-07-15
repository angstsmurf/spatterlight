// aslx_runtime_test.cc -- milestone-2 script/expression runtime tests.
//
//   ./aslx_runtime_test            run the assertions
//
// Built by test/Makefile; links libexpat + zlib (via the loader). See
// TODO-quest5.md §2.

#include "../aslx.cc"
#include "../aslx-runtime.cc"

#include <cstdio>
#include <iostream>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_STR(a, b)                                                        \
    do {                                                                       \
        std::string _a = (a), _b = (b);                                        \
        if (_a != _b) {                                                        \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": got ["    \
                      << _a << "] want [" << _b << "]\n";                      \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

using namespace aslx;

// Run a script against a world and return its output (trailing newline trimmed).
static std::string run(Interp &in, const std::string &src) {
    in.clear_output();
    Context ctx;
    in.run_script(src, ctx);
    std::string o = in.output();
    while (!o.empty() && o.back() == '\n') o.pop_back();
    return o;
}

// Evaluate a single expression to a string.
static std::string evals(Interp &in, const std::string &expr) {
    Context ctx;
    return Interp::to_string(in.eval(expr, ctx));
}

static void test_expressions() {
    World w;
    w.asl_version = 550;
    Interp in(w);

    CHECK_STR(evals(in, "1 + 2 * 3"), "7");
    CHECK_STR(evals(in, "(1 + 2) * 3"), "9");
    CHECK_STR(evals(in, "10 / 4"), "2.5");
    CHECK_STR(evals(in, "10 - 3 - 2"), "5");
    CHECK_STR(evals(in, "7 % 3"), "1");
    CHECK_STR(evals(in, "\"foo\" + \"bar\""), "foobar");
    CHECK_STR(evals(in, "\"n=\" + 5"), "n=5");
    CHECK_STR(evals(in, "2 = 2"), "True");
    CHECK_STR(evals(in, "2 <> 3"), "True");
    CHECK_STR(evals(in, "3 > 2 and 1 < 2"), "True");
    CHECK_STR(evals(in, "3 > 2 or 1 > 2"), "True");
    CHECK_STR(evals(in, "not (1 = 2)"), "True");
    CHECK_STR(evals(in, "5 > 3 ? \"big\" : \"small\""), "big");
    CHECK_STR(evals(in, "-4 + 1"), "-3");
    CHECK_STR(evals(in, "true and false"), "False");
    CHECK_STR(evals(in, "\"a\" < \"b\""), "True");

    // Built-in string functions (1-based positions).
    CHECK_STR(evals(in, "LCase(\"ABC\")"), "abc");
    CHECK_STR(evals(in, "UCase(\"abc\")"), "ABC");
    CHECK_STR(evals(in, "Left(\"hello\", 2)"), "he");
    CHECK_STR(evals(in, "Right(\"hello\", 3)"), "llo");
    CHECK_STR(evals(in, "Mid(\"hello\", 2, 3)"), "ell");
    CHECK_STR(evals(in, "Mid(\"hello\", 3)"), "llo");
    CHECK_STR(evals(in, "Instr(\"hello\", \"l\")"), "3");
    CHECK_STR(evals(in, "Instr(\"hello\", \"z\")"), "0");
    CHECK_STR(evals(in, "Replace(\"a-b-c\", \"-\", \"+\")"), "a+b+c");
    CHECK_STR(evals(in, "Trim(\"  hi  \")"), "hi");
    CHECK_STR(evals(in, "LengthOf(\"hello\")"), "5");
    CHECK_STR(evals(in, "StartsWith(\"hello\", \"he\")"), "True");
    CHECK_STR(evals(in, "Join(Split(\"a,b,c\", \",\"), \"|\")"), "a|b|c");
    CHECK_STR(evals(in, "ListCount(Split(\"a,b,c\", \",\"))"), "3");
    CHECK_STR(evals(in, "CInt(\"42\") + 1"), "43");
    CHECK_STR(evals(in, "Abs(-7)"), "7");
    CHECK_STR(evals(in, "Max(3, 9)"), "9");
}

static void test_control_flow() {
    World w;
    w.asl_version = 550;
    Interp in(w);

    CHECK_STR(run(in, "msg (\"hi\")"), "hi");

    // if / else if / else
    CHECK_STR(run(in,
        "x = 5\n"
        "if (x > 10) {\n"
        "  msg (\"big\")\n"
        "}\n"
        "else if (x > 3) {\n"
        "  msg (\"medium\")\n"
        "}\n"
        "else {\n"
        "  msg (\"small\")\n"
        "}"), "medium");

    // while
    CHECK_STR(run(in,
        "i = 0\n"
        "result = \"\"\n"
        "while (i < 3) {\n"
        "  result = result + i\n"
        "  i = i + 1\n"
        "}\n"
        "msg (result)"), "012");

    // for with step
    CHECK_STR(run(in,
        "s = \"\"\n"
        "for (n, 1, 5, 2) {\n"
        "  s = s + n\n"
        "}\n"
        "msg (s)"), "135");

    // foreach over a split list
    CHECK_STR(run(in,
        "out = \"\"\n"
        "foreach (item, Split(\"x,y,z\", \",\")) {\n"
        "  out = out + item\n"
        "}\n"
        "msg (out)"), "xyz");
}

static void test_functions_and_objects() {
    World w;
    bool ok = load_file("fixtures/aslx/runtime.aslx", w);
    if (!ok) std::cerr << dump(w);
    CHECK(ok);
    Interp in(w);

    // Function with a return value + object attribute mutation.
    CHECK_STR(run(in, "msg (AddScore(10))"), "10");
    CHECK_STR(run(in, "msg (AddScore(5))"), "15");   // score persisted
    CHECK_STR(run(in, "msg (player.score)"), "15");

    // Function reading an inherited attribute and an object arg.
    CHECK_STR(run(in, "Describe(box)"), "The box weighs 4");

    // Inherited attribute directly.
    CHECK_STR(run(in, "msg (box.capacity)"), "10");
    CHECK(w.find("box"));
    {
        Context ctx;
        CHECK(Interp::to_string(in.eval("DoesInherit(box, \"container\")", ctx)) == "True");
        CHECK(Interp::to_string(in.eval("DoesInherit(player, \"container\")", ctx)) == "False");
    }

    // Numeric loop function.
    CHECK_STR(run(in, "msg (SumTo(5))"), "15");

    // if/elseif/else inside a function returning strings.
    CHECK_STR(run(in, "msg (Classify(-3))"), "negative");
    CHECK_STR(run(in, "msg (Classify(0))"), "zero");
    CHECK_STR(run(in, "msg (Classify(7))"), "positive");
}

static void test_rng_determinism() {
    World w; w.asl_version = 550;
    Interp a(w), b(w);
    Context c1, c2;
    std::string s1, s2;
    for (int i = 0; i < 5; ++i) {
        s1 += Interp::to_string(a.eval("GetRandomInt(1, 100)", c1)) + ",";
        s2 += Interp::to_string(b.eval("GetRandomInt(1, 100)", c2)) + ",";
    }
    CHECK_STR(s1, s2);            // deterministic across instances (seed 1234)
    CHECK(s1.find_first_not_of("0123456789,") == std::string::npos);
}

int main() {
    test_expressions();
    test_control_flow();
    test_functions_and_objects();
    test_rng_determinism();

    if (g_failures == 0) {
        std::cout << "aslx_runtime_test: all checks passed\n";
        return 0;
    }
    std::cerr << "aslx_runtime_test: " << g_failures << " failure(s)\n";
    return 1;
}
