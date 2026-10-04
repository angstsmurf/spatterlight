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
  the frontend whole (the engine and the map pane are linked in as objects, as
  for aslxglk_smoke) and supplies the Glk entry points CheapGlk's main()
  expects.

  Build:  make aslxglk_link_tests       (in this directory)
  Run:    ./aslxglk_link_tests          (exit 0 = all passed)
*/

#include <iostream>
#include <string>

#include "../../../quest5/aslxglk.cc"

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

/* ---- Deeper's character dialog and status panel (aslxglk-form) ---- */

/* The shape of Deeper's game.gamestart, cut down to two attributes and two
 * bonus items. */
const char *kFormHtml =
    "<div id=\"dialog_window_1\" class=\"dialog_window\" title=\"Your Character\">"
    "<table><tr class=\"details\">"
    "<td colspan=\"2\">Name: <input type=\"text\" id=\"name_input\" value=\"Skybird\"/></td>"
    "<td>Sex: <input type=\"radio\" name=\"sex_input\" value=\"Male\" checked=\"checked\"/>Male\n"
    " <input type=\"radio\" name=\"sex_input\" value=\"Female\" checked=\"checked\"/>Female</td></tr>"
    "<tr><td><br/><b>Attributes</b></td><td></td><td><br/><b>Bonus item</b></td></tr>"
    "<tr><td>Strength</td><td>"
    "<span onclick=\"incAtt('strength');\">&#x25B2;</span>"
    "<span onclick=\"decAtt('strength');\">&#x25BC;</span>"
    "<div id=\"strength\" style=\"display:inline;\">0</div></td>"
    "<td><input type=\"radio\" name=\"bonus\" value=\"bonus1\" checked=\"checked\">Two healing potions</td></tr>"
    "<tr><td>Agility</td><td>"
    "<span onclick=\"incAtt('agility');\">&#x25B2;</span>"
    "<span onclick=\"decAtt('agility');\">&#x25BC;</span>"
    "<div id=\"agility\" style=\"display:inline;\">0</div></td>"
    "<td><input type=\"radio\" name=\"bonus\" value=\"bonus2\">Sabre</td></tr>"
    "<tr><td>Points left</td><td><div id=\"points\" style=\"display:inline;\">10</div></td></tr>"
    "</table></div>"
    "<script>function done() { answer = $('#name_input').val();"
    " ASLEvent(\"HandleDialogue\", answer); }</script>";

void test_form_parse()
{
    aslxform::CharacterForm f;
    check(aslxform::parse_character_form(kFormHtml, f), "the dialog is recognised");
    check(f.title == "Your Character", "title comes from the dialog div");
    check(f.event == "HandleDialogue", "event is HandleDialogue");
    check(f.name == "Skybird", "default name");
    check(f.sex.values.size() == 2 && f.sex.checked == 1,
          "two sexes; the last of two checked radios wins, as in a browser");
    check(f.counters.size() == 2 && f.counters[0].id == "strength" &&
          f.counters[1].label == "Agility" && f.counters[0].value == 0,
          "one counter per incAtt target");
    check(f.points == 10, "points pool");
    check(f.bonus.values.size() == 2 && f.bonus.values[1] == "bonus2" &&
          f.bonus.labels[1] == "Sabre" && f.bonus.checked == 0,
          "bonus radios keep value and label apart");
    check(f.answer() == "Skybird|Female|0|0|bonus1", "answer of an untouched form");

    aslxform::CharacterForm g;
    check(!aslxform::parse_character_form("<b>Name:</b> <i>Skybird</i>", g),
          "ordinary markup is not a dialog");
}

void test_form_rules()
{
    aslxform::CharacterForm f;
    aslxform::parse_character_form(kFormHtml, f);
    check(!f.dec(0), "a counter does not go below zero");
    for (int i = 0; i < 10; i++)
        f.inc(0);
    check(f.counters[0].value == 10 && f.points == 0, "ten points spent");
    check(!f.inc(1) && f.counters[1].value == 0, "nothing left to spend");
    check(f.dec(0) && f.inc(1) && f.points == 0, "a point moves between counters");

    f.set_name("  Zog|<the>&  ");
    check(f.name == "Zogthe", "the name loses the separator and markup characters");
    f.set_name("   ");
    check(f.name == "Zogthe", "an empty name keeps the old one");
    f.set_name(std::string(40, 'x'));
    check(f.name.size() == aslxform::kFormNameMax, "the name is cut to length");
    f.set_name("Zog");

    size_t focus = 0;
    using aslxform::FormKey;
    using aslxform::FormAct;
    check(aslxform::form_key(f, focus, FormKey::Activate) == FormAct::EditName,
          "Return on the name edits it");
    aslxform::form_key(f, focus, FormKey::Down);
    aslxform::form_key(f, focus, FormKey::Left);
    check(focus == 1 && f.sex.checked == 0, "Left on the sex row picks Male");
    check(aslxform::form_click(f, focus, aslxform::form_link(aslxform::Ctl::Bonus, 1))
              == FormAct::None && f.bonus.checked == 1,
          "clicking a bonus selects it");
    check(aslxform::form_click(f, focus, aslxform::form_link(aslxform::Ctl::Dec, 0))
              == FormAct::None && f.counters[0].value == 8 && f.points == 1,
          "clicking a down arrow gives the point back");
    check(aslxform::form_click(f, focus, aslxform::form_link(aslxform::Ctl::Done))
              == FormAct::Done, "clicking Done finishes");
    check(f.answer() == "Zog|Male|8|1|bonus2", "answer after edits");
    focus = aslxform::form_focus_count(f) - 1;
    check(aslxform::form_key(f, focus, FormKey::Activate) == FormAct::Done,
          "Return on the last slot is Done");
}

void test_form_layout()
{
    aslxform::CharacterForm f;
    aslxform::parse_character_form(kFormHtml, f);
    for (int width : { 80, 30 }) {
        aslxform::Layout lay = aslxform::layout_character_form(f, width, 0);
        std::string tag = width == 80 ? " (wide)" : " (narrow)";
        bool inside = true, done = false, name_focus = false;
        std::set<unsigned> links;
        for (const aslxform::Span &sp : lay.spans) {
            int cells = (int) u32_from_utf8(sp.text).size();
            if (sp.x < 0 || sp.y < 0 || sp.y >= lay.height || sp.x + cells > width)
                inside = false;
            if (sp.link)
                links.insert(sp.link);
            if (sp.link == aslxform::form_link(aslxform::Ctl::Done))
                done = true;
            if (sp.focused && sp.text == "Name:")
                name_focus = true;
        }
        check(inside, "every span fits the grid" + tag);
        check(done, "there is a Done link" + tag);
        check(name_focus, "focus slot 0 highlights the name's label" + tag);
        /* name, 2 sexes, 2x(inc, dec), 2 bonuses, done */
        check(links.size() == 10, "one link per control" + tag);
    }
    check(aslxform::layout_character_form(f, 30, 0).height >
          aslxform::layout_character_form(f, 80, 0).height,
          "the narrow layout stacks the columns");
}

void test_status_panel()
{
    const char *html =
        "<div id=\"button_div\">"
        "<img onclick=\"ASLEvent('HandleButtonClick','look')\"/>"
        "<img onclick=\"ASLEvent('HandleButtonClick','map')\"/></div>"
        "<div id=\"status_div\"><table>"
        "<tr><td>Hit points:</td><td><span id=\"hits-span\">---</span></td></tr>"
        "<tr><td>Defence:</td><td><span id=\"defence-span\">0</span></td></tr>"
        "<tr><td>Defence bonus:</td><td><span id=\"defence-span\">0</span></td></tr>"
        "</table></div>";
    aslxform::StatusPanel p;
    check(aslxform::parse_status_panel(html, p), "the status panel is recognised");
    check(p.rows.size() == 3 && p.rows[0].label == "Hit points:" &&
          p.rows[0].id == "hits-span" && p.rows[0].value == "---",
          "rows carry label, id and starting value");
    check(p.buttons.size() == 2 && p.buttons[1] == "map", "button commands");
    aslxform::StatusPanel q;
    check(!aslxform::parse_status_panel("<div id=\"other\">x</div>", q),
          "other markup is not a panel");

    std::string id, value;
    check(aslxform::parse_jquery_html("$('#hits-span').html('12/20')", id, value) &&
          id == "hits-span" && value == "12/20", "single-quoted jQuery html()");
    check(aslxform::parse_jquery_html("$(\"#gold-span\").html(\"5\");", id, value) &&
          id == "gold-span" && value == "5", "double-quoted jQuery html()");
    check(!aslxform::parse_jquery_html("$('#hits-span').hide()", id, value),
          "other jQuery calls are not cell writes");

    /* The frontend's view: a duplicated id is written in its first row only. */
    g_cstatus = p;
    g_cstatus_vals.clear();
    g_cstatus_vals["defence-span"] = "7";
    std::vector<std::string> lines = custom_status_lines();
    check(lines.size() == 3 && lines[0] == "Hit points: ---" &&
          lines[1] == "Defence: 7" && lines[2] == "Defence bonus: 0",
          "a duplicated id updates only its first row");
    check(take_custom_ui(html) && !take_custom_ui("<b>hello</b>") &&
          take_custom_ui(kFormHtml) && g_form_pending,
          "both chunks are kept out of the transcript");
    g_cstatus = aslxform::StatusPanel();
    g_cstatus_vals.clear();
    g_form_pending = false;
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

    std::cout << "character dialog and status panel:\n";
    test_form_parse();
    test_form_rules();
    test_form_layout();
    test_status_panel();

    std::cout << (failures ? "FAILED" : "all passed") << " (" << failures
              << " failure" << (failures == 1 ? "" : "s") << ")\n";
    if (failures)
        exit(1);
}
