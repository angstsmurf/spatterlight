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
    // `not` binds looser than `=` (QuestViva notOperator level): "not x = y" is
    // "not (x = y)". Core relies on this pervasively ("not obj = null").
    CHECK_STR(evals(in, "not 1 = 2"), "True");
    CHECK_STR(evals(in, "not 1 = 1"), "False");
    CHECK_STR(evals(in, "not \"x\" = null"), "True");
    CHECK_STR(evals(in, "not 1 = 1 and 2 = 2"), "False");
    CHECK_STR(evals(in, "not not 1 = 1"), "True");
    CHECK_STR(evals(in, "5 > 3 ? \"big\" : \"small\""), "big");
    CHECK_STR(evals(in, "-4 + 1"), "-3");
    CHECK_STR(evals(in, "true and false"), "False");
    CHECK_STR(evals(in, "\"a\" < \"b\""), "True");
    // Backslash escapes in string literals (Parlot StringLiteralQuotes): \" \' \\.
    CHECK_STR(evals(in, "Replace(\"a\\\"b\", \"\\\"\", \"X\")"), "aXb");
    CHECK_STR(evals(in, "\"\\\\D\""), "\\D");

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

    // Statement splitting must not be derailed by an escaped quote in a string
    // literal: "\"" is a literal quote, so the next line is a separate statement
    // (regression for the JSSafe-style whole-body-swallow bug).
    CHECK_STR(run(in,
        "s = Replace(\"a\\\"b\", \"\\\"\", \"\")\n"
        "msg (s)"), "ab");
    // A // inside a string is not a comment.
    CHECK_STR(run(in, "msg (\"http://x\")"), "http://x");

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

static void test_script_commands() {
    World w;
    bool ok = load_file("fixtures/aslx/runtime.aslx", w);
    if (!ok) std::cerr << dump(w);
    CHECK(ok);
    Interp in(w);

    // switch / case (multiple values) / default
    const char *sw =
        "switch (x) {\n"
        "  case (1) { msg (\"one\") }\n"
        "  case (2, 3) { msg (\"two-three\") }\n"
        "  default { msg (\"other\") }\n"
        "}";
    CHECK_STR(run(in, std::string("x = 2\n") + sw), "two-three");
    CHECK_STR(run(in, std::string("x = 3\n") + sw), "two-three");
    CHECK_STR(run(in, std::string("x = 9\n") + sw), "other");

    // firsttime / otherwise -- state persists across runs of the same (cached)
    // script source, mirroring Quest's per-instance m_hasRun.
    const char *ft =
        "firsttime { msg (\"first\") }\n"
        "otherwise { msg (\"again\") }";
    CHECK_STR(run(in, ft), "first");
    CHECK_STR(run(in, ft), "again");
    CHECK_STR(run(in, ft), "again");

    // do -- run a named script attribute on an object
    CHECK_STR(run(in, "do (player, \"greet\")"), "player waves");

    // create / create-with-type / destroy
    run(in, "create (\"widget\")");
    CHECK(w.find("widget"));
    run(in, "create (\"crate\", \"container\")");
    {
        Context c;
        CHECK_STR(Interp::to_string(in.eval("DoesInherit(crate, \"container\")", c)),
                  "True");
    }
    run(in, "destroy (\"widget\")");
    CHECK(!w.find("widget"));

    // set(obj, field, value)
    CHECK_STR(run(in, "set (box, \"colour\", \"red\")\nmsg (box.colour)"), "red");

    // list add / list remove, local and on an attribute
    CHECK_STR(run(in,
        "l = NewStringList()\n"
        "list add (l, \"x\")\n"
        "list add (l, \"y\")\n"
        "list remove (l, \"x\")\n"
        "msg (Join(l, \",\"))"), "y");
    CHECK_STR(run(in,
        "box.tags = NewStringList()\n"
        "list add (box.tags, \"heavy\")\n"
        "list add (box.tags, \"wooden\")\n"
        "msg (ListCount(box.tags))"), "2");

    // dictionary add / remove / count / contains / item
    CHECK_STR(run(in,
        "d = NewDictionary()\n"
        "dictionary add (d, \"a\", \"1\")\n"
        "dictionary add (d, \"b\", \"2\")\n"
        "dictionary remove (d, \"a\")\n"
        "msg (DictionaryContains(d, \"a\"))\n"
        "msg (DictionaryContains(d, \"b\"))\n"
        "msg (DictionaryCount(d))\n"
        "msg (DictionaryItem(d, \"b\"))"), "False\nTrue\n1\n2");

    // on ready -- deferred until drain, then FIFO
    {
        in.clear_output();
        Context ctx;
        in.run_script(
            "msg (\"a\")\n"
            "on ready { msg (\"deferred\") }\n"
            "msg (\"b\")", ctx);
        std::string before = in.output();
        CHECK(before.find("deferred") == std::string::npos);
        CHECK(before.find("a") != std::string::npos &&
              before.find("b") != std::string::npos);
        in.drain_on_ready();
        CHECK(in.output().find("deferred") != std::string::npos);
    }

    // JS.addText routes to the output sink; other JS.* calls are ignored.
    CHECK_STR(run(in, "JS.addText (\"<b>hi</b>\")\nJS.updateLocation (\"room\")"),
              "<b>hi</b>");

    // finish sets the world flag.
    CHECK(!w.finished);
    run(in, "finish");
    CHECK(w.finished);
}

static void test_new_builtins() {
    World w;
    bool ok = load_file("fixtures/aslx/runtime.aslx", w);
    CHECK(ok);
    Interp in(w);

    // Load-time [template] substitution: Templated() returns "[Greeting] world"
    // with [Greeting] already replaced by the template text.
    CHECK_STR(run(in, "msg (Templated())"), "hello world");
    // Template() / DynamicTemplate() at runtime.
    CHECK_STR(evals(in, "Template(\"Greeting\")"), "hello");
    CHECK_STR(evals(in, "DynamicTemplate(\"Weigh\", box)"), "weighs 4");

    // String helpers.
    CHECK_STR(evals(in, "CapFirst(\"hello\")"), "Hello");
    CHECK_STR(evals(in, "SafeXML(\"a<b>&c\")"), "a&lt;b&gt;&amp;c");

    // TypeOf(obj, attr): own, inherited, and missing.
    CHECK_STR(evals(in, "TypeOf(box, \"weight\")"), "int");
    CHECK_STR(evals(in, "TypeOf(box, \"capacity\")"), "int");   // inherited
    CHECK_STR(evals(in, "TypeOf(box, \"nope\")"), "null");

    // World-model queries.
    CHECK_STR(evals(in, "ListCount(AllObjects())"), "3");        // room, player, box
    CHECK_STR(evals(in, "ListCount(GetDirectChildren(room))"), "2");
    CHECK_STR(evals(in, "Contains(room, player)"), "True");
    CHECK_STR(evals(in, "Contains(player, room)"), "False");
    CHECK_STR(evals(in, "ListContains(GetAttributeNames(player, false), \"score\")"),
              "True");

    // Typed list/dictionary item access.
    CHECK_STR(evals(in, "StringListItem(Split(\"a,b,c\", \",\"), 1)"), "b");
    CHECK_STR(evals(in, "ObjectListItem(AllObjects(), 0)"), "room");
}

// The parser primitives: named-group matching, match strength, and Populate,
// mirroring QuestViva Utility.IsRegexMatch/GetMatchStrength/Populate on the
// simplepattern-derived regexes CoreParser feeds them.
static void test_regex_primitives() {
    World w; w.asl_version = 550;
    Interp in(w);

    // IsRegexMatch: named group + case-insensitive, like a "look at #object1#"
    // command pattern. Non-matching input returns False, not an error.
    CHECK_STR(evals(in, "IsRegexMatch(\"^look at (?<object1>.*)$\", \"look at thing\")"),
              "True");
    CHECK_STR(evals(in, "IsRegexMatch(\"^TAKE (?<object1>.*)$\", \"take sword\")"),
              "True");
    CHECK_STR(evals(in, "IsRegexMatch(\"^look at (?<object1>.*)$\", \"take thing\")"),
              "False");

    // GetMatchStrength = input length minus the length matched by named groups.
    // "look at thing" is 13 chars, object1 captures "thing" (5) -> 8.
    CHECK_STR(evals(in, "GetMatchStrength(\"^look at (?<object1>.*)$\", \"look at thing\")"),
              "8");
    // A more specific command (less captured by the group) scores higher.
    CHECK_STR(evals(in, "GetMatchStrength(\"^(?<object1>.*)$\", \"look at thing\")"),
              "0");

    // Populate returns a string dictionary of group name -> captured text.
    CHECK_STR(evals(in,
              "StringDictionaryItem(Populate(\"^put (?<object1>.*) in (?<object2>.*)$\","
              " \"put hat in box\"), \"object1\")"), "hat");
    CHECK_STR(evals(in,
              "StringDictionaryItem(Populate(\"^put (?<object1>.*) in (?<object2>.*)$\","
              " \"put hat in box\"), \"object2\")"), "box");
    CHECK_STR(evals(in,
              "DictionaryCount(Populate(\"^put (?<object1>.*) in (?<object2>.*)$\","
              " \"put hat in box\"))"), "2");

    // Multi-alternative verb regex (from ConvertVerbSimplePattern shape).
    CHECK_STR(evals(in,
              "IsRegexMatch(\"^take (?<object>.*)$|^get (?<object>.*)$\", \"get lamp\")"),
              "True");
    CHECK_STR(evals(in,
              "StringDictionaryItem(Populate(\"^take (?<object>.*)$|^get (?<object>.*)$\","
              " \"get lamp\"), \"object\")"), "lamp");

    // Character-class group, no backslash escapes needed in the source string.
    CHECK_STR(evals(in,
              "StringDictionaryItem(Populate(\"^(?<num>[0-9]+)(?<rest>.*)$\","
              " \"42abc\"), \"num\")"), "42");

    // Cache path: same cacheID reuses the compiled regex (3-arg form).
    CHECK_STR(evals(in,
              "IsRegexMatch(\"^go (?<object1>.*)$\", \"go north\", \"go\")"), "True");
    CHECK_STR(evals(in,
              "GetMatchStrength(\"^go (?<object1>.*)$\", \"go north\", \"go\")"), "3");
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

// End-to-end boot: load a game plus the full Core library, run InitInterface +
// StartGame, and confirm the whole pipeline (default types, parent field, the
// text processor via msg->OutputText, ~all the primitives) runs with zero
// errors and produces processed output. This is the milestone-3 boot proof.
static void test_coreboot_runs() {
    World w;
    // core_dir relative to test/ (where the harness runs).
    bool ok = load_file("fixtures/aslx/coreboot.aslx", w, "../aslx-core");
    CHECK(ok);
    CHECK(w.errors.empty());
    if (!w.errors.empty())
        for (auto &e : w.errors) std::cerr << "  boot-load err: " << e << "\n";

    Interp in(w);
    std::string out;
    in.print = [&](const std::string &s) { out += s; };

    Context c1, c2;
    if (w.find("InitInterface")) in.call_function("InitInterface", {}, &c1);
    if (w.find("StartGame")) in.call_function("StartGame", {}, &c2);
    if (in.has_pending_on_ready()) in.drain_on_ready();

    // The whole boot ran without a single script error.
    if (!w.errors.empty())
        for (auto &e : w.errors) std::cerr << "  boot-run err: " << e << "\n";
    CHECK(w.errors.empty());

    // The title rendered (StartGame -> PrintCentered -> msg -> OutputText), and
    // the {...} text processor expanded (no raw "{=" survives, WriteVerb ran so
    // "It is" appears for the gender-"it" fixture player).
    CHECK(out.find("Core Boot Test") != std::string::npos);
    CHECK(out.find("It is") != std::string::npos);
    CHECK(out.find("{=") == std::string::npos);

    // The parent field is exposed and drives containment.
    CHECK_STR(Interp::to_string(in.eval("player.parent", c1)), "lounge");
    CHECK_STR(Interp::to_string(in.eval("Contains(lounge, chest)", c1)), "True");
    // The game object inherits defaultgame (implicit default type).
    CHECK_STR(Interp::to_string(in.eval("DoesInherit(game, \"defaultgame\")", c1)),
              "True");
}

int main() {
    test_expressions();
    test_control_flow();
    test_functions_and_objects();
    test_script_commands();
    test_new_builtins();
    test_regex_primitives();
    test_coreboot_runs();
    test_rng_determinism();

    if (g_failures == 0) {
        std::cout << "aslx_runtime_test: all checks passed\n";
        return 0;
    }
    std::cerr << "aslx_runtime_test: " << g_failures << " failure(s)\n";
    return 1;
}
