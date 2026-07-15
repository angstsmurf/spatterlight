// aslx_loader_test.cc -- milestone-1 loader tests for the Quest 5 engine.
//
//   ./aslx_loader_test              run the assertions
//   ./aslx_loader_test <file>       load <file> (.aslx or .quest) and dump it
//
// Built by test/Makefile; links libexpat and zlib. See TODO-quest5.md.

#include "../aslx.cc"

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

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        auto _a = (a);                                                         \
        auto _b = (b);                                                         \
        if (!(_a == _b)) {                                                     \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " << #a  \
                      << " == " << #b << " (got " << _a << " vs " << _b        \
                      << ")\n";                                                \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

using namespace aslx;

static const Value *field(const World &w, const char *elem, const char *f) {
    Element *e = w.find(elem);
    return e ? e->field(f) : nullptr;
}

static void test_hello() {
    World w;
    bool ok = load_file("fixtures/aslx/hello.aslx", w);
    if (!ok) {
        std::cerr << "load failed:\n" << dump(w);
    }
    CHECK(ok);

    // Version + game metadata.
    CHECK_EQ(w.asl_version, 550);
    CHECK_EQ(w.game_name, std::string("Hello Quest"));

    // implied + template registration.
    CHECK_EQ(w.implied_types.size(), (size_t)3);
    const std::string *imp = w.implied_type("object", "displayverbs");
    CHECK(imp && *imp == "simplestringlist");
    CHECK_EQ(w.templates.size(), (size_t)1);
    CHECK_EQ(w.templates[0].first, std::string("Take"));

    // Game element fields: implicit string, flag boolean, typed int, script.
    Element *game = w.find("game");
    CHECK(game && game->elem_type == "game");
    const Value *author = field(w, "game", "author");
    CHECK(author && author->type == Value::Type::String &&
          author->str == "Test Author");
    const Value *showscore = field(w, "game", "showscore");
    CHECK(showscore && showscore->type == Value::Type::Boolean &&
          showscore->boolean == true);
    const Value *maxscore = field(w, "game", "maxscore");
    CHECK(maxscore && maxscore->type == Value::Type::Int &&
          maxscore->integer == 50);
    const Value *start = field(w, "game", "start");
    CHECK(start && start->type == Value::Type::Script);

    // Containment: player and lamp nest inside room; room + hall are roots.
    Element *room = w.find("room");
    Element *player = w.find("player");
    Element *lamp = w.find("lamp");
    CHECK(room && player && lamp);
    CHECK(player->parent == room);
    CHECK(lamp->parent == room);
    CHECK(room->parent == nullptr);

    // Inheritance recorded.
    CHECK_EQ(room->inherits.size(), (size_t)1);
    CHECK_EQ(room->inherits[0], std::string("editor_room"));

    // Lamp fields: implicit string, typed simplestringlist, int, double, script.
    const Value *look = lamp->field("look");
    CHECK(look && look->type == Value::Type::String && look->str == "A brass lamp.");
    const Value *dv = lamp->field("displayverbs");
    CHECK(dv && dv->type == Value::Type::StringList);
    CHECK_EQ(dv->list.size(), (size_t)3);
    if (dv && dv->list.size() == 3) {
        CHECK_EQ(dv->list[0], std::string("Look"));
        CHECK_EQ(dv->list[1], std::string("Take"));
        CHECK_EQ(dv->list[2], std::string("Rub"));
    }
    const Value *weight = lamp->field("weight");
    CHECK(weight && weight->type == Value::Type::Int && weight->integer == 3);
    const Value *price = lamp->field("price");
    CHECK(price && price->type == Value::Type::Double);
    if (price) CHECK(price->dbl > 4.4 && price->dbl < 4.6);
    const Value *take = lamp->field("take");
    CHECK(take && take->type == Value::Type::Script);

    // implicit implied type: displayverbs without a type= would still be a list.
    // (Covered by lamp above, which specifies it; the game's <start> proves
    //  implicit typing is not required for the explicit path.)

    // exit: alias + lazy object reference, nested inside room.
    Element *exit = nullptr;
    for (Element *c : room->children)
        if (c->elem_type == "exit") exit = c;
    CHECK(exit != nullptr);
    if (exit) {
        const Value *to = exit->field("to");
        CHECK(to && to->type == Value::Type::ObjectRef && to->str == "hall");
        const Value *alias = exit->field("alias");
        CHECK(alias && alias->str == "north");
    }

    // Nested <value> stringlist and <item> dictionary on hall.
    const Value *cmdlist = field(w, "hall", "cmdlist");
    CHECK(cmdlist && cmdlist->type == Value::Type::StringList);
    CHECK_EQ(cmdlist->list.size(), (size_t)2);
    const Value *mapping = field(w, "hall", "mapping");
    CHECK(mapping && mapping->type == Value::Type::StringDict);
    CHECK_EQ(mapping->dict.size(), (size_t)2);
    if (mapping && mapping->dict.size() == 2) {
        CHECK_EQ(mapping->dict[0].first, std::string("a"));
        CHECK_EQ(mapping->dict[0].second, std::string("alpha"));
    }

    // command: pattern (simplepattern -> regex) + script body.
    Element *cmd = w.find("rublamp");
    CHECK(cmd && cmd->elem_type == "command");
    const Value *pat = cmd ? cmd->field("pattern") : nullptr;
    CHECK(pat && pat->type == Value::Type::String);
    // "rub #object#; polish #object#" -> two anchored alternatives.
    CHECK(pat && pat->str.find("(?<object>.*)") != std::string::npos);
    CHECK(pat && pat->str.find("|") != std::string::npos);
    const Value *cscript = cmd ? cmd->field("script") : nullptr;
    CHECK(cscript && cscript->type == Value::Type::Script);

    // anonymous verb: derived name, isverb flag, property, body -> defaulttext.
    Element *verb = nullptr;
    for (const auto &up : w.elements)
        if (up->elem_type == "verb") verb = up.get();
    CHECK(verb != nullptr);
    if (verb) {
        CHECK(verb->anonymous);
        const Value *isv = verb->field("isverb");
        CHECK(isv && isv->boolean);
        const Value *prop = verb->field("property");
        CHECK(prop && prop->str == "xyzzy");
        const Value *dt = verb->field("defaulttext");
        CHECK(dt && dt->str == "You wave your hands mysteriously.");
    }

    // function: params split, return type, script body.
    Element *fn = w.find("MoveObject");
    CHECK(fn && fn->elem_type == "function");
    const Value *params = fn ? fn->field("paramnames") : nullptr;
    CHECK(params && params->type == Value::Type::StringList);
    CHECK_EQ(params->list.size(), (size_t)2);
    if (params && params->list.size() == 2) {
        CHECK_EQ(params->list[0], std::string("obj"));
        CHECK_EQ(params->list[1], std::string("dest"));
    }
    const Value *ret = fn ? fn->field("returntype") : nullptr;
    CHECK(ret && ret->str == "boolean");

    // walkthrough element with a simplestringlist steps field.
    Element *wt = w.find("main");
    CHECK(wt && wt->elem_type == "walkthrough");
    const Value *steps = wt ? wt->field("steps") : nullptr;
    CHECK(steps && steps->type == Value::Type::StringList);
    CHECK_EQ(steps->list.size(), (size_t)3);
}

// Loads a game that <include>s English.aslx + Core.aslx from the bundled Core
// dir, proving include resolution pulls in the whole Core library cleanly.
static void test_coreboot() {
    World w;
    // core_dir is relative to test/ (where the harness runs). The fixture's
    // includes resolve English.aslx (Languages/) and Core.aslx (root), which in
    // turn pull in every non-editor Core*.aslx.
    bool ok = load_file("fixtures/aslx/coreboot.aslx", w, "../aslx-core");
    if (!ok || !w.errors.empty()) {
        std::cerr << "coreboot load failed / had errors:\n" << dump(w);
    }
    CHECK(ok);
    // A clean boot has no errors at all: no unresolved includes, no unknown
    // attribute types (delegate types, listextend and list are all handled).
    CHECK_EQ(w.errors.size(), (size_t)0);

    CHECK_EQ(w.asl_version, 580);
    CHECK_EQ(w.game_name, std::string("Core Boot Test"));

    // Core brought in a large library: hundreds of functions and dozens of
    // types/commands/verbs. Exact counts drift with Core versions, so assert
    // generous floors rather than equality.
    size_t functions = 0, types = 0, commands = 0, verbs = 0, delegates = 0;
    for (const auto &up : w.elements) {
        const std::string &t = up->elem_type;
        if (t == "function") ++functions;
        else if (t == "type") ++types;
        else if (t == "command") ++commands;
        else if (t == "verb") ++verbs;
        else if (t == "delegate") ++delegates;
    }
    CHECK(functions > 200);
    CHECK(types > 30);
    CHECK(commands > 20);
    CHECK(verbs > 10);
    CHECK(delegates >= 2);   // AddScript + AssociatedScope, declared in Core

    // A couple of well-known Core elements resolved by name.
    CHECK(w.find("MoveObject") && w.find("MoveObject")->elem_type == "function");
    Element *openable = w.find("openable");
    CHECK(openable && openable->elem_type == "type");

    // Core's openable.displayverbs is a listextend: a StringList flagged to
    // extend the inherited list rather than replace it.
    const Value *dv = openable ? openable->field("displayverbs") : nullptr;
    CHECK(dv && dv->type == Value::Type::StringList && dv->list_extend);

    // The fixture's command has an implicit-typed <multiple>. The command.multiple
    // implied type is the AssociatedScope delegate, so the body loads as a
    // delegate-implementation Script, not a plain string.
    Element *cmd = w.find("pushcmd");
    CHECK(cmd && cmd->elem_type == "command");
    const Value *mult = cmd ? cmd->field("multiple") : nullptr;
    CHECK(mult && mult->type == Value::Type::Script);

    // The game's own objects merged into the same tree, with containment.
    Element *lounge = w.find("lounge");
    Element *player = w.find("player");
    Element *chest = w.find("chest");
    CHECK(lounge && player && chest);
    if (player) CHECK(player->parent == lounge);
    if (chest) {
        // chest inherits both editor_object and Core's openable type.
        bool has_openable = false;
        for (const std::string &t : chest->inherits)
            if (t == "openable") has_openable = true;
        CHECK(has_openable);
    }

    // Includes were recorded (English + Core + everything Core pulls in), and
    // none of them left an "include not found" error behind.
    CHECK(w.includes.size() >= 2);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        std::string core = "";
        if (const char *env = std::getenv("ASLX_CORE")) core = env;
        World w;
        bool ok = load_file(argv[1], w, core);
        std::cout << dump(w);
        return ok ? 0 : 1;
    }

    test_hello();
    test_coreboot();

    if (g_failures == 0) {
        std::cout << "aslx_loader_test: all checks passed\n";
        return 0;
    }
    std::cerr << "aslx_loader_test: " << g_failures << " failure(s)\n";
    return 1;
}
