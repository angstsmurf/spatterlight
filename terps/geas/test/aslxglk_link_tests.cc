/*
  aslxglk_link_tests -- checks for the Quest 5 frontend's HTML link parser
  (link_action) and its clickability gate (LinkAction::live).

  Core turns every clickable thing it prints into an <a>/<span> carrying the
  action in its attributes; the frontend has to recover that action and decide
  whether the anchor becomes a Glk hyperlink.  Neither half is reachable from
  the other harnesses: aslx_replay strips HTML before it ever sees a tag, and
  aslxglk_smoke runs on CheapGlk, whose gestalt reports no hyperlink support --
  so g_hyperlinks is false there and no link is ever registered or clicked.
  That blind spot has cost two regressions in the same afternoon (an object
  link sending the element NAME instead of its alias, then an object link not
  rendering as a link at all), hence these direct calls.

  link_action lives in aslxglk.cc's anonymous namespace, so this TU includes
  the frontend whole -- the same unity build aslxglk_smoke does -- and supplies
  the Glk entry points CheapGlk's main() expects.

  Build:  make aslxglk_link_tests       (in this directory)
  Run:    ./aslxglk_link_tests          (exit 0 = all passed)
*/

#include <iostream>
#include <string>

#include "../quest5/aslxglk.cc"

extern "C" {
#include "glkstart.h"
}

/* glkimp's real-time-delays preference, referenced by the frontend. */
extern "C" int gli_sa_delays;
int gli_sa_delays = 0;

glkunix_argumentlist_t glkunix_arguments[] = {
    { nullptr, glkunix_arg_End, nullptr }
};

int glkunix_startup_code(glkunix_startup_t *)
{
    return 1;
}

namespace {

int failures = 0;

void check(bool ok, const std::string &what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << "\n";
    if (!ok)
        failures++;
}

/* An inline object name: Core's ProcessTextCommand_Object output for
   {object:doorobj} in a game where that object is aliased "door". */
void test_object_link()
{
    LinkAction a = link_action(
        "<a id=\"l1\" style=\"\" class=\"cmdlink elementmenu\" "
        "data-elementid=\"doorobj\">door</a>");

    /* The ELEMENT name is what travels; the command is built at click time
       from the live world, because only there is the alias -- the thing the
       parser actually resolves -- and the verb menu knowable. */
    check(a.element == "doorobj", "object link carries the element name");
    check(a.command.empty(), "object link builds no command up front");

    /* And it must be clickable, or the anchor renders as plain text and the
       object cannot be reached at all when the side pane is hidden. */
    check(a.live(), "object link is live");
}

/* Core's ProcessTextCommand_Command output for {command:address}. */
void test_command_link()
{
    LinkAction a = link_action(
        "<a id=\"l2\" style=\"\" class=\"cmdlink commandlink\" "
        "data-elementid=\"address\" data-command=\"address\">address</a>");
    check(a.command == "address", "command link sends its data-command");
    check(a.element.empty(), "command link needs no element resolution");
    check(a.live(), "command link is live");
}

/* Core's ProcessTextCommand_Exit output for {exit:exit_away}: the command is
   already fully phrased (verb + alias), so it needs no click-time lookup. */
void test_exit_link()
{
    LinkAction a = link_action(
        "<a style=\"\" class=\"cmdlink exitlink\" data-elementid=\"exit_away\" "
        "data-command=\"go to leave\">leave</a>");
    check(a.command == "go to leave", "exit link sends its data-command");
    check(a.live(), "exit link is live");
}

/* A ShowMenu option: onclick="ASLEvent('Func','param')". */
void test_aslevent_link()
{
    LinkAction a = link_action(
        "<a class=\"cmdlink\" style=\"\" "
        "onclick=\"ASLEvent('ShowMenuResponse','yes')\">yes</a>");
    check(a.event_func == "ShowMenuResponse", "ASLEvent function recovered");
    check(a.event_param == "yes", "ASLEvent parameter recovered");
    check(a.live(), "ASLEvent link is live");
}

/* The web player's own bridge, called from hand-written game markup. */
void test_sendcommand_link()
{
    LinkAction lit = link_action(
        "<span onclick=\"sendCommand('north')\">north</span>");
    check(lit.command == "north", "sendCommand literal recovered");
    check(lit.live(), "sendCommand literal is live");

    LinkAction self = link_action(
        "<span onclick=\"sendCommand(this.innerText)\">look</span>");
    check(self.inner_text, "sendCommand(this.innerText) defers to the content");
    /* live() is false until the renderer fills the command in from the
       element's text -- which it does before asking (see the <a>/<span> arms
       of render_html). */
}

void test_endwait_link()
{
    LinkAction a = link_action("<a onclick=\"endWait()\">Continue</a>");
    check(a.end_wait, "endWait link recovered");
    check(a.live(), "endWait link is live");
}

/* Anything with no action at all stays plain text. */
void test_inert_anchor()
{
    LinkAction a = link_action("<a href=\"https://example.com/\">a link</a>");
    check(!a.live(), "a plain href is not a clickable action");
    LinkAction b = link_action("<span style=\"color:red\">red</span>");
    check(!b.live(), "a bare styled span is not a clickable action");
    /* An elementmenu with no id to resolve has nothing to do either. */
    LinkAction c = link_action("<a class=\"cmdlink elementmenu\">nothing</a>");
    check(!c.live(), "an elementmenu with no element id is not live");
}

/* -- bundled-JS callback scanning (Moquette's JS.act0Clear -> ASLEvent) ---- */

/* ASLEvent(func, param) calls, in source order, including inside a nested
   setTimeout closure -- exactly the shape moquette.js's act0Clear uses. */
void test_js_scan_aslevents()
{
    std::vector<std::pair<std::string, std::string> > evs;
    js_scan_aslevents(
        "$(x).effect('drop'); setTimeout(function() {"
        "  EndOutputSection('intro');"
        "  ASLEvent(\"FinishAct0Clear\", \"\");"
        "}, 1500);", evs);
    check(evs.size() == 1, "one ASLEvent found in a setTimeout body");
    check(!evs.empty() && evs[0].first == "FinishAct0Clear",
          "the completion name is recovered from the nested closure");
    check(!evs.empty() && evs[0].second.empty(), "its empty param stays empty");

    /* A single-argument ASLEvent, and single quotes. */
    std::vector<std::pair<std::string, std::string> > one;
    js_scan_aslevents("ASLEvent('StartAct1')", one);
    check(one.size() == 1 && one[0].first == "StartAct1",
          "single-arg ASLEvent recovered");
    check(one.size() == 1 && one[0].second.empty(),
          "single-arg ASLEvent has an empty param");

    /* A param, and an ASLEvent mentioned only inside a string literal must
       NOT be picked up. */
    std::vector<std::pair<std::string, std::string> > two;
    js_scan_aslevents(
        "ASLEvent(\"Go\", \"north\"); var s = \"call ASLEvent(later)\";", two);
    check(two.size() == 1, "an ASLEvent named inside a string is not a call");
    check(two.size() == 1 && two[0].first == "Go" && two[0].second == "north",
          "the real call's func and param are both recovered");
}

/* Call-graph edges: identifiers immediately followed by '(' that name another
   bundled function, so index_bundled_js can follow heatherText -> doHeatherText
   to the ASLEvent that actually lives in the helper. */
void test_js_scan_calls()
{
    std::set<std::string> known;
    known.insert("doHeatherText");
    known.insert("getRandomInt");
    std::vector<std::string> calls;
    js_scan_calls(
        "$('<div/>').appendTo('body'); doHeatherText(); notBundled();"
        " var n = getRandomInt(0, 3);", known, calls);
    check(calls.size() == 2, "only the two known callees are edges");
    check(!calls.empty() && calls[0] == "doHeatherText",
          "the delegated helper is an edge");
    check(calls.size() == 2 && calls[1] == "getRandomInt",
          "a second known callee is an edge, in source order");

    /* A name that only appears inside a string is not a call edge. */
    std::vector<std::string> none;
    js_scan_calls("var s = \"doHeatherText()\";", known, none);
    check(none.empty(), "a callee named inside a string is not an edge");
}

}  /* namespace */

void glk_main(void)
{
    std::cout << "link_action:\n";
    test_object_link();
    test_command_link();
    test_exit_link();
    test_aslevent_link();
    test_sendcommand_link();
    test_endwait_link();
    test_inert_anchor();

    std::cout << "bundled-js scanning:\n";
    test_js_scan_aslevents();
    test_js_scan_calls();

    std::cout << (failures ? "FAILED" : "all passed") << " (" << failures
              << " failure" << (failures == 1 ? "" : "s") << ")\n";
    if (failures)
        exit(1);
}
