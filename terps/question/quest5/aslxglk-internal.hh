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


/* aslxglk-internal.hh -- what the Quest 5 Glk frontend's translation units
 * share: the frontend state, its types and the prototype of every function
 * that crosses a file.  Private to the aslxglk*.cc files (and to the link
 * tests in the harness); the public entry points are in QuestionRunner.hh.
 *
 *   aslxglk.cc           core, session loop, host hooks, entry points
 *   aslxglk-output.cc    HTML rendering, retraction, output sections
 *   aslxglk-panes.cc     banner and side pane
 *   aslxglk-input.cc     line/key/menu input, character dialog, metaverbs
 *   aslxglk-media.cc     resources: pictures, sounds, panels
 *   aslxglk-autosave.cc  Spatterlight autosave of the frontend state
 *
 * The state lives here as C++17 inline variables, so every unit sees the one
 * definition. */

#ifndef QUESTION_ASLXGLK_INTERNAL_HH
#define QUESTION_ASLXGLK_INTERNAL_HH

#include "aslx.hh"
#include "aslx-runtime.hh"
#include "aslxglk-form.hh"
#include "aslxglk-map.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include "glk.h"
#ifdef SPATTERLIGHT
/* Full window/stream structs: autosave needs the serialization tags.
 * fileref.h: the per-session temp directory staged sounds must live in so
 * the app can replay them after an autorestore. */
#include "glkimp.h"
#include "fileref.h"
#endif
}

#ifdef SPATTERLIGHT
/* Spatterlight autosave/autorestore (questionglk-autosave.mm). */
#include "../questionglk-autosave.h"
#endif

/* Presentation helpers shared with the classic Quest 1-4 frontend
 * (questionglk.cc): the status banner, side pane + divider, transcript metaverb,
 * save-file prompts, string/UTF-8 utilities and resource registration. */
#include "../questglk-common.hh"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libgen.h>
#endif
#include <unistd.h>

/* Spatterlight's "slow draw / real-time delays" preference (glkimp).  When it
 * is off -- as in the deterministic import tests -- the wall-clock timer
 * heartbeat is suppressed, exactly like the classic Question loop. */
extern "C" int gli_sa_delays;

namespace aslxglk {

using namespace aslx;

/* The Glk timer interval currently requested (ms, 0 = off) and the ms
 * accumulated toward the next whole engine second while the interval runs
 * faster than 1s (the map marker glide -- update_timer_request,
 * timer_event_second).  File-scope globals so the blocking do_pause hook,
 * which overrides the timer with its own one-shot interval, can reset them
 * and let the next update_timer_request re-arm cleanly. */
inline glui32 g_timer_ms = 0;

inline int g_timer_frac_ms = 0;

/* Shared frontend helpers (questglk-common.hh).  Using-declarations rather
 * than a using-directive, so each borrowed name is spelled out here. */
using questglk::change_side_pane_width;

using questglk::close_side_pane_windows;

using questglk::draw_status_banner;

using questglk::echo_input_line;

using questglk::fill_side_divider;

using questglk::glk_style_for;

using questglk::lower;

using questglk::match_help_command;

using questglk::match_restore_command;

using questglk::match_sidebar_command;

using questglk::match_status_command;

using questglk::match_transcript_command;

using questglk::NOTHING_TO_UNDO;

using questglk::open_side_pane_windows;

using questglk::PANE_COMPASS;

using questglk::PANE_INVENTORY;

using questglk::PANE_PLACES_OBJECTS;

using questglk::PANE_STATUS;

using questglk::play_single_sound;

using questglk::post_game_menu_match;

using questglk::post_game_menu_print;

using questglk::post_game_menu_reprompt;

using questglk::POSTGAME_QUIT;

using questglk::POSTGAME_RESTART;

using questglk::POSTGAME_RESTORE;

using questglk::POSTGAME_UNDO;

using questglk::print_status_report;

using questglk::print_system_commands;

using questglk::prompt_read_save;

using questglk::prompt_write_save;

using questglk::put_pane_header;

using questglk::put_pane_link;

using questglk::register_path_resource;

using questglk::set_side_pane_wide;

using questglk::side_pane_is_wide;

using questglk::status_wants_pane;

using questglk::put_stream_utf8;

using questglk::QUIT_FAREWELL;

using questglk::stop_single_sound;

using questglk::toggle_transcript;

using questglk::wait_for_event;

using questglk::trim;

using questglk::unput_tail_exact;

using questglk::utf8_append;

using questglk::utf8_cp_len;

using questglk::utf8_from_uni;

using questglk::utf8_next_cp;

std::string cap_first(const std::string &s);

inline winid_t gwin = nullptr;      /* main text buffer */

inline winid_t gbanner = nullptr;   /* one-line status grid: room name */

inline strid_t gtranscript = nullptr;

inline bool g_manual_echo = false;  /* the HOST echoes accepted input lines */

inline bool g_echo_control = false; /* library supports glk_set_echo_line_event */

inline bool g_hyperlinks = false;

inline std::string g_room_name;

/* Prompt-first presentation.  The reference player has a separate input box
 * ("Type here..."), so no prompt is ever printed and the GAME side echoes
 * each accepted command into the transcript itself ("> cmd" -- Core's
 * game.echocommand, or the engine for pre-v520).  In a Glk buffer we want a
 * standard "> " prompt at the input point instead, so when the library lets
 * us control line-input echo we print the prompt up front, echo the accepted
 * line ourselves, and swallow the game-side echo (g_swallow) so the command
 * does not appear twice.  Echo-incapable libraries (CheapGlk: the smoke
 * harness) keep the reference presentation, byte-stable for the harness. */
inline bool g_prompt_first = false;

/* Is the reference player's command box showing?  Core hides it when
 * game.showcommandbar is off, and GamebookCore hides it for good: a gamebook
 * has no parser at all, it is played by clicking the links on each page.
 * While it is hidden we print no prompt and request no line input, so the
 * page reads as a page -- exactly what the reference player shows.  Only a
 * `get input` re-shows it (GamebookCore's GetInput does so explicitly). */
inline bool g_command_bar = true;

/* Set by JS.HookClicks: the game has bound a handler to the whole output area
 * that turns ANY click there into a command (spondre's glue.js sends
 * "(Unknown)", a token no verb matches, so the game's catch-all response
 * runs).  A Glk text buffer delivers no such click -- only hyperlinks are
 * clickable -- so the visible "click to continue" link stands in for it: with
 * the hook installed and no wait to release, clicking runs a turn.  Without
 * that, spondre's title has no way forward but typing a command at a screen
 * that offers nothing to type at. */
inline bool g_click_hook = false;

/* The command a hook-driven click sends.  The text is not inspected by the
 * games using this idiom -- verified on spondre, where "(Unknown)",
 * "xyzzyplugh" and "look" all produce byte-identical output at the title, so
 * what matters is only that a turn runs. */
inline const char *const kClickHookCommand = "(Unknown)";

inline int g_swallow = 0;           /* 2 = leading blank + "> cmd" outstanding,
                                       1 = blank swallowed, "> cmd" outstanding */
inline std::string g_swallow_cmd;   /* the trimmed command the echo must match */

/* Click actions, one per Glk hyperlink value (index + 1).  A cmdlink carries
 * either a literal command (data-command / data-elementid) or an
 * onclick="ASLEvent('Func','param')" pair (Core's ShowMenu options). */
struct LinkAction {
    std::string command;        /* send_command() this */
    std::string event_func;     /* or call_function(event_func, {event_param}) */
    std::string event_param;
    std::string toggle_key;     /* pane object: expand/collapse its verb list */
    std::string element;        /* inline object link: pop its verb menu */
    bool end_wait = false;      /* onclick="endWait()": release a pending wait */
    bool inner_text = false;    /* command is the element's own text (filled in
                                 * by the renderer, which has the content) */

    /* The renderer's clickability gate: anything a click can DO belongs here,
     * or the anchor is drawn as plain text and the action is unreachable.
     * `element` counts -- its command is only built at click time. */
    bool live() const {
        return !command.empty() || !event_func.empty() || !element.empty() ||
               end_wait;
    }
};

inline std::vector<LinkAction> g_links;

/* Link values at or below this are spent: JS.disableAllCommandLinks (Gamebook
 * Core, when a page choice has been taken) retires every cmdlink printed so
 * far, so an earlier page's options cannot be clicked again.  A high-water
 * mark rather than a clear, because the values are indices into g_links and
 * the already-printed links keep theirs -- clearing would hand an old link on
 * screen the action of whichever new one reused its number. */
inline size_t g_links_spent = 0;

/* Label of the host "Continue..." chrome currently at the transcript tail
 * (QuestViva's #endWaitLink), or empty when none is showing. */
inline std::string g_continue_shown;

/* Right-hand pane: Quest's Inventory / "Places and Objects" / compass lists
 * (WorldModel.UpdateListsAsync -> the update_list host hook), shown the way
 * the classic runner shows its objwin -- opened only when there is something
 * to list, with a thin graphics divider, closed otherwise. */
inline winid_t gobjwin = nullptr;

inline winid_t gdivider = nullptr;

inline bool g_use_objpane = false;          /* host can split; probed at startup */

/* JS.panesVisible: the game's own say over the panes.  Core sends false from
 * InitInterface when game.showpanes is off, and games toggle it around
 * cutscenes.  While false the pane stays closed no matter what the lists
 * hold, like the reference player hiding #gamePanes. */
inline bool g_panes_visible = true;

inline std::vector<ListData> g_pane_inv, g_pane_places, g_pane_exits;

inline bool g_pane_dirty = false;           /* lists changed since the last redraw */

inline std::vector<LinkAction> g_pane_links;   /* rebuilt on every pane redraw */

/* Element name of the pane object whose verb list is currently unfolded, or
 * empty.  QuestViva pops a verb menu when an object name is clicked; a Glk
 * pane has nowhere to pop one, so the verbs appear as an indented list under
 * the name, and clicking the name again folds it away.  One at a time, like
 * the reference menu.  Survives redraws, so the list stays open across the
 * turns its own verbs run. */
inline std::string g_pane_expanded;

/* Pane hyperlink values live above the main-window link ids, which only ever
 * grow one per rendered cmdlink and never get near this. */
inline const glui32 kPaneLinkBase = 0x40000000;

/* JS.updateStatus payload flattened for the banner ("Score: 3 | Health: 90%"),
 * shown right-aligned next to the room name like the classic runner. */
inline std::string g_status_line;

/* The status is listed in the side pane instead of the banner, which is too
 * narrow for it (see status_wants_pane). */
inline bool g_status_in_pane = false;

/* request_hyperlinks is in force: a pane repainted outside the turn loop's
 * own redraws has to re-arm its request. */
inline bool g_links_requested = false;

/* JS.updateLocation payload -- Core's own "where am I" string, preferred over
 * resolving game.pov.parent by hand (empty for old pre-JS games). */
inline std::string g_location_line;

/* Deeper builds two pieces of UI out of raw HTML that a Glk window cannot
 * show as text: a jQuery-UI character creation dialog (game.gamestart) and a
 * status panel whose cells the game rewrites through JS.eval.  Both chunks
 * are taken out of the transcript (take_custom_ui).  The dialog runs as a
 * text grid once the boot is over -- in the browser its "Done" button fires
 * after the start script has finished, too -- and the panel's rows go at the
 * top of the side pane. */
inline aslxform::CharacterForm g_form;

inline bool g_form_pending = false;

inline winid_t gformwin = nullptr;         /* open only while the dialog is up */

inline aslxform::StatusPanel g_cstatus;

/* <span id> -> its current contents, as last set by the game. */
inline std::map<std::string, std::string> g_cstatus_vals;

void request_hyperlinks();
void cancel_hyperlinks();

/* Inline image drawing (defined in the resources section below): draw the
 * named game resource in the main window, original size but never wider
 * than the window. False when unresolvable or images are unsupported. */
bool draw_image_inline(const std::string &name);

/* Inline <audio> playback (defined alongside it): start the first of `srcs`
 * that resolves, on the same single channel as `play sound`. */
bool play_audio_inline(const std::vector<std::string> &srcs, bool loop);

void panel_arrange();

/* Pane redraw (defined after the HTML renderer): repaints the side pane from
 * the stored lists when they changed; safe to call anywhere no line input is
 * pending. */
void redraw_side_pane(Interp &in);

void fill_pane_divider();

#ifdef SPATTERLIGHT

/* Spatterlight autosave.  aslx_do_autosave (defined with the rest of the
 * autosave glue before run_session) runs at the parser prompt whenever
 * g_autosave_interp is set -- read_line calls it after printing the prompt
 * and again after state-changing timer ticks, so the snapshot always ends
 * at "waiting for a command". */
inline bool g_autorestore_pending = false;   /* an autosave exists; restore at boot */

inline bool g_autorestore_reentry = false;   /* first prompt after it: no reprint */

inline std::string g_autorestore_panel;      /* frame picture to re-establish */

/* RNG streams captured at autosave time, applied just before play resumes
 * (AFTER InitInterface's own draws) so the restored prompt's randomness is
 * exactly the saved one. */
inline std::vector<std::pair<std::string, std::array<uint32_t, 4>>> g_autorestore_rngs;

inline Interp *g_autosave_interp = nullptr;

void aslx_do_autosave(Interp &in);

/* Autosave under a prompt a turn puts up: a menu (the parser's "which one do
 * you mean?" among them), a yes/no question, a `get input`, a `wait` for a
 * keypress.  Its callback
 * sits in the engine as a live script continuation that no snapshot can
 * hold, so that autosave instead carries the state the turn STARTED from
 * plus a replay record -- the command and every answer given to a prompt
 * since -- and an autorestore re-runs the turn with its output muted,
 * feeding it those answers, until the prompt is pending again (see
 * start_replay).  The screen already shows the prompt: the GUI snapshot was
 * taken with it up. */
using RngStreams = std::vector<std::pair<std::string, std::array<uint32_t, 4>>>;

struct TurnStart {
    bool valid = false;         /* false once the engine moved on without it */
    std::string state;          /* save_game at the start of the turn */
    RngStreams rngs;            /* ...and the RNG streams, drawn again on replay */
    std::string undo;           /* ...and the undo history, as of then */
};

inline TurnStart g_turn_start;

inline std::string g_turn_command;                 /* the turn being played */

inline std::vector<std::string> g_turn_answers;    /* its prompts' answers so far */

/* Armed for the one read_line that shows such a prompt: that read_line
 * writes the menu autosave. */
inline Interp *g_menu_autosave_interp = nullptr;

/* Read from the autosave record; the turn loop starts the replay from them. */
inline bool g_autorestore_replay = false;

inline std::string g_replay_command;

inline std::vector<std::string> g_replay_answers;

/* The start script counts as a turn of its own, with no command and no
 * state to start from: an autosave under one of its prompts is the answers
 * given so far (a record with an empty state), and the autorestore boots
 * the game again with them.  True from a fresh boot until the first
 * command, or until the engine moves on some other way. */
inline bool g_boot_turn = false;

void aslx_do_menu_autosave(Interp &in, bool at_wait = false);

bool replay_answer(const char *kinds, std::string *text = nullptr);

bool replay_reached(Interp &in, const char *where);

void turn_start_changed();

#endif

/* True while an autorestore replays a turn (see g_turn_start): the prompt
 * loop and the prompt hooks answer from g_replay_answers instead of the
 * player, and every hook that draws is muted. */
inline bool g_replaying = false;

/* Scope guard: suspend autosave while a prompt the snapshot cannot describe
 * is up.  Engine-owned prompts (the SCRIPT-COMMAND forms of ShowMenu, ask,
 * wait) are safe -- pending state INSIDE the snapshot, and they run from the
 * prompt loop where the hook is already unset.  The EXPRESSION forms are not:
 * the engine blocks inside eval, so a half-run script sits on the C++ stack
 * with nothing recording it, and a timer tick or link click can open one while
 * the parser prompt's hook is still armed.  Same reason a pending `get input`
 * has never autosaved.
 *
 * The inline object link's verb menu does NOT use this: it runs BETWEEN turns
 * (the click starts no script), so the world under it is clean and the menu
 * itself is recorded in the frontend blob -- see g_pending_menu_alias. */
struct AutosaveSuspend {
#ifdef SPATTERLIGHT
    Interp *saved;
    AutosaveSuspend() : saved(g_autosave_interp) { g_autosave_interp = nullptr; }
    ~AutosaveSuspend() { g_autosave_interp = saved; }
#endif
};

/* The inline object-link verb menu currently on screen, if any: the object's
 * display alias (non-empty = a menu is up) and the exact options shown.  Both
 * go into the autosave blob, so a snapshot taken under the menu -- the window
 * ending at "Choose [1-2] (or nothing to cancel)> " -- restores straight back
 * INTO the menu, with the options that are already on screen rather than a
 * re-resolved list that might not match them. */
inline std::string g_pending_menu_alias;

inline std::vector<std::string> g_pending_menu_verbs;

#ifdef SPATTERLIGHT

/* Set at autorestore when the blob carried a pending menu: the next parser
 * read_line re-enters it instead of prompting.  One-shot. */
inline bool g_autorestore_menu_resume = false;

#endif

void update_timer_request(Interp &in);

bool timer_event_second(Interp &in);

/* ---------------------------------------------------------------- output -- */

/* While an output section is being recorded this captures every character
 * render_html writes into the main window, so HideOutputSection can hand the
 * exact text back to garglk_unput_string_count_uni (a case-insensitive TAIL
 * match -- see g_out_log). */
inline std::u32string *g_tee = nullptr;

/* Bumped by draw_image_inline; render_html compares it across a chunk to see
 * whether that chunk put an unmatchable attachment into the window. */
inline size_t g_images_drawn = 0;

void put_uni_char(glui32 c);
void put_uni_string(const std::string &utf8);

/* Current inline style, tracked as nesting counters so tags may overlap.
 * Mapped onto the closest Glk style, like the classic runner's set_style. */
inline int g_bold = 0, g_italic = 0, g_under = 0;

void apply_style();

/* What one <span>/<a> contributed to the counters, so its close undoes it. */
struct StylePush { int b = 0, i = 0, u = 0; };

std::string tag_attr(const std::string &tag, const std::string &attr);
bool tag_flag(const std::string &tag, const std::string &attr);
StylePush css_style(const std::string &css);
void push_style(const StylePush &p);
void pop_style(const StylePush &p);

std::string decode_entities(const std::string &s);

LinkAction link_action(const std::string &tag);
glui32 decode_entity(const std::string &s, size_t &i);
std::string decode_entities(const std::string &s);

std::string plain_text(const std::string &html, const char *brsep = " ");

std::string tag_name(const std::string &tag, bool &closing);
std::string element_inner_text(const std::string &html, size_t pos,
                               const std::string &name);
std::vector<std::string> audio_sources(const std::string &html, size_t pos,
                                       const std::string &open_tag, size_t *end);
bool swallow_echo_chunk(const std::string &html);
std::string strip_hidden_elements(const std::string &html);

std::u32string u32_from_utf8(const std::string &s);
bool unput_exact(const std::u32string &s);

/* ------------------------------------------------------ output sections -- */

/* The reference player wraps JS.StartOutputSection..EndOutputSection output in
 * a <div> it can later hide.  A Glk buffer cannot hide anything, but
 * garglk_unput_string_count_uni retracts text that still matches the END of
 * the window (GlkTextBufferWindow+Output.m:221, a case-insensitive tail
 * compare returning the count removed).  A section is rarely the tail when it
 * is hidden -- GamebookCore prints the chosen option's label BEFORE DoPage
 * hides the options block above it -- so hiding means: retract everything from
 * the section's first chunk to the end of the window, then replay the chunks
 * that came after the section closed.
 *
 * That needs a record of what each chunk actually wrote, which is what g_tee
 * collects.  Anything that fails the tail match (a prompt or host echo landed
 * in between, an image attachment sits inside the range) leaves the transcript
 * exactly as it is today: the retract is best-effort, never destructive. */

struct OutChunk {
    std::string html;       /* raw payload, for replay through render_html */
    std::u32string text;    /* exactly what it wrote into the window */
    bool image = false;     /* drew an image: an attachment char we cannot match */
};

inline std::vector<OutChunk> g_out_log;

struct OutSection {
    std::string name;
    size_t start = 0;                       /* first chunk index */
    size_t end = (size_t) -1;               /* one past the last, -1 while open */
};

inline std::vector<OutSection> g_sections;

/* Sections are only ever hidden a page or two after they close, but nothing
 * says so -- bound the log rather than grow it for the length of a game. */
inline const size_t kMaxSections = 8;

/* win_unprint copies into the 64 KB message buffer as uint16_t
 * (glkimp/connect.c:220), so a retract longer than that would overrun it.
 * Referenced only by the SPATTERLIGHT unput path; kMaxLogChunks below is
 * sized against it in every build. */
inline const size_t kMaxUnput = 16384;

void render_html(const std::string &raw);

/* A hide can only ever retract kMaxUnput chars, so chunks past this many are
 * dead weight -- without the cap a game sitting inside <= kMaxSections (or one
 * never-closed section) records every print for the whole session and
 * re-serializes the lot into every autosave. */
inline const size_t kMaxLogChunks = 4096;

void trim_out_log();
void start_output_section(const std::string &name);
void end_output_section(const std::string &name);
void hide_output_section(const std::string &name);

/* Bumped by clear_screen_ui.  A PromptBreak in flight compares it against the
 * value it captured: after a clear there is no stale prompt left to retract
 * (and an unput would fail into a stray blank line at the top of the cleared
 * window). */
inline unsigned g_clear_gen = 0;

void clear_screen_ui();
void render_html_inner(const std::string &raw);
bool take_custom_ui(const std::string &raw);
void render_html(const std::string &raw);
std::vector<std::string> custom_status_lines();

std::string plain_text(const std::string &html, const char *brsep);
Element *game_element(World &w);
Element *object_field(Interp &in, Element *e, const char *attr);
void update_banner(Interp &in);

void ensure_pane_open();
void close_side_pane();
void fill_pane_divider();
std::string template_text_or(World &w, const char *name, const char *fallback);
std::vector<std::string> compass_directions(Interp &in);
void redraw_side_pane(Interp &in);
void handle_arrange(Interp &in);

/* ----------------------------------------------------------------- input -- */

/* What ended an input request. */
enum class InEnd {
    Line,       /* text is the entered line */
    Command,    /* text is a hyperlink's command, to run as if typed */
    Event,      /* an ASLEvent hyperlink already ran; state may have changed */
    State,      /* a timer changed engine state (menu/question/wait/finish) */
};

struct InResult {
    InEnd kind = InEnd::Line;
    std::string text;
};

void echo_input(const std::string &line);
void echo_metaverb(const std::string &cmd);
void echo_metaverb_command(const std::string &cmd);
void run_asl_event(Interp &in, const LinkAction &act);

/* (ASLEvent function name, its parameter) pairs -- what a scanned JS body
 * fires, and what the bridge queues. */
using JsEvents = std::vector<std::pair<std::string, std::string>>;

/* JS callback completions queued by the js_event_bridge hook: (js function,
 * its last argument).  Fired between turns, never mid-script. */
inline JsEvents g_js_events;

/* Completions a game's OWN bundled JS would fire from inside a zero-argument
 * JS.func() -- indexed once per session from the <javascript src> files (see
 * index_bundled_js).  Maps the JS function name to the ASLEvent(func, param)
 * calls in its body, in source order.  Spondre-style games pass the callback
 * name as the call's last argument and never reach this; Moquette's
 * JS.act0Clear() takes no argument and buries ASLEvent("FinishAct0Clear","")
 * in moquette.js, so without this the game stalls on the first train. */
inline std::map<std::string, JsEvents> g_js_callbacks;

void fire_js_events(Interp &in);
bool engine_state_pending(Interp &in);

/* Scope guard: reroute engine prints so the first output emitted while a
 * prompt sits on the screen retracts that stale prompt first -- an
 * interrupting timer then reads as a clean run of text with a single prompt
 * at the bottom, exactly like the reference player, instead of a bare "> "
 * stranded above every tick.  broke tells the caller a reprint of the prompt
 * is needed.
 *
 * The retract is attempted only on the first output, after the caller has
 * cancelled the line request: with echo control the cancel already removed
 * any typed text (GlkTextBufferWindow+Input.m cancelLine), so the prompt
 * really is the window tail and unput_exact removes it whole.  Whenever it
 * does not (the window moved on, no echo control so the typed text is still
 * on screen, CheapGlk's no-op unput), the fallback is the old behaviour:
 * break the line so the output lands cleanly under the stale prompt.  Typed
 * text survives either way -- the caller re-requests input with the
 * cancelled line as preload (ce.val1). */
struct PromptBreak {
    Interp &in;
    const char *prompt;
    bool broke = false;
    unsigned clear_gen = g_clear_gen;
    std::function<void(const std::string &)> saved;
    std::function<void(const std::string &)> saved_hide;
    /* Take the stale prompt off the screen (or break the line under it). */
    void clear_prompt() {
        if (broke) return;
        /* A ClearScreen since this prompt went up already took it off
         * the screen; retracting (or breaking the line) would only put
         * a stray blank at the top of the cleared window. */
        if (g_clear_gen == clear_gen &&
            !unput_exact(u32_from_utf8(prompt)))
            glk_put_char('\n');
        broke = true;
    }
    PromptBreak(Interp &i, const char *p) : in(i), prompt(p) {
        if (!prompt) return;
        saved = in.print;  /* restore the PRIOR handler on exit -- a nested
                            * redirect must not be stomped back to render_html */
        in.print = [this](const std::string &h) {
            if (!h.empty())
                clear_prompt();
            render_html(h);
        };
        /* A section can only be retracted as the window tail, and the prompt
         * is sitting on it: a clicked Core menu option hides its list before
         * (or without) printing anything. */
        saved_hide = in.hide_output_section;
        in.hide_output_section = [this](const std::string &n) {
            clear_prompt();
            if (saved_hide) saved_hide(n);
        };
    }
    ~PromptBreak() {
        if (prompt) {
            in.print = saved;
            in.hide_output_section = saved_hide;
        }
    }
};

/* Request one line of input.  `prompt` is printed first (and reprinted after
 * any engine output that interrupts the request); pass nullptr when the
 * caller printed its own or none applies.  `on_screen` = the prompt text is
 * already at the end of the window (an autorestored snapshot ends with it),
 * so it is not printed on entry but still describes what an interrupting
 * tick must retract or reprint -- passing nullptr instead would leave
 * PromptBreak blind and a tick's output would land ON the prompt line with
 * no prompt reprinted after it.  `echo` = the HOST echoes the
 * accepted line (host-owned prompts, and every prompt-first line).  It is
 * false only for legacy-mode parser commands, which the GAME side echoes
 * itself -- Core's game.echocommand prints "> cmd" into the transcript (the
 * engine does it for pre-v520 games), exactly like the reference player,
 * whose input box leaves no other record.  Library echo is off either way
 * (when controllable), so the typed line is atomically replaced by whichever
 * echo applies.  Timer events tick the engine; if a tick opens a prompt or
 * ends the game the request is cancelled and InEnd::State returned. */
/* Defined below; an inline object link opens its verb menu from inside the
 * link branch here, the same nested-read_line shape a timer-opened menu has. */
bool run_menu_ui(Interp &in, const MenuData &m, std::string &key,
                 bool resumed = false);

bool run_question_ui(Interp &in, const std::string &q, bool &answer,
                     bool resumed = false);

bool run_input_ui(Interp &in, std::string &text, bool resumed = false);

bool run_verb_menu(Interp &in, bool resumed, std::string &cmd);

InResult read_line(Interp &in, bool echo, const char *prompt = nullptr,
                   bool want_line = true, bool on_screen = false);
void show_continue_link(Interp &in, bool on_screen = false);
bool lift_continue_link();
void hide_continue_link();
void read_keypress(Interp &in, bool resumed = false);

/* What a host prompt wrote under a v600+ inline option list: the prompt, the
 * echoed answer, any "please choose" retry.  The engine hides the list once
 * the prompt resolves (Interp::end_inline_prompt -> HideOutputSection), and
 * hide_output_section can only retract a section when it knows every
 * character between it and the end of the window -- so the prompt's own text
 * goes into the chunk log, with no html: it is retracted with the options and
 * not replayed.  Best-effort like every retract here: after engine output
 * landed in between (a timer tick) nothing is logged and the list stays. */
struct InlinePromptTrail {
    bool live;
    std::u32string text;
    explicit InlinePromptTrail(Interp &in) : live(in.inline_prompts()) {}
    void wrote(const std::string &s) { if (live) text += u32_from_utf8(s); }
    void interrupted() { live = false; }
    void commit()
    {
        if (!live || text.empty() || g_sections.empty())
            return;
        OutChunk c;
        c.text = text;
        g_out_log.push_back(c);
        trim_out_log();
        text.clear();
    }
};

bool run_menu_ui(Interp &in, const MenuData &m, std::string &key, bool resumed);
bool run_verb_menu(Interp &in, bool resumed, std::string &cmd);
bool run_question_ui(Interp &in, const std::string &q, bool &answer,
                     bool resumed);
bool run_input_ui(Interp &in, std::string &text, bool resumed);

void draw_character_form(const aslxform::Layout &lay, bool links);
bool run_character_grid(Interp &in, aslxform::CharacterForm &f);
void run_character_prompts(Interp &in, aslxform::CharacterForm &f);
void run_character_dialog(Interp &in);
void seed_custom_status(Interp &in);

/* ---------------------------------------------------------- save/restore -- */

std::string core_dir_path();   /* defined in the core section below */

inline const char *g_storyfile = nullptr;

const char *story_path();
void do_save_ui(Interp &in);
bool do_restore_ui(std::string &data);

bool handle_help_command(const std::string &raw);
void hint_system_commands(const std::string &raw, bool parser_game);
bool handle_status_command(const std::string &raw);
bool handle_sidebar_command(Interp &in, const std::string &raw);
bool handle_transcript_command(const std::string &raw);
bool handle_verbs_command(Interp &in, const std::string &raw);

std::string core_dir_path();

/* ------------------------------------------------------------- resources -- */

/* The .quest package's entry table, listed once at startup. Empty for raw
 * .aslx games, whose resources live next to the game file instead
 * (QuestViva's FileDirectoryGameDataProvider). */
inline std::vector<ZipEntryInfo> g_package_entries;

inline bool g_is_package = false;

inline std::string g_story_dir;

void init_resources(const char *storyfile);
bool read_file_range(const std::string &path, size_t offset, size_t len,
                     std::string &out);
bool resource_bytes(const std::string &name, std::string &out);
bool js_string_literal(const std::string &js, size_t &i, std::string &out);
void js_scan_aslevents(const std::string &body, JsEvents &out);
void js_scan_calls(const std::string &body, const std::set<std::string> &known,
                   std::vector<std::string> &out);
void index_bundled_js(World &w);

#ifdef SPATTERLIGHT

/* Quest's image and sound files are external and arbitrarily named, so
 * each is registered with glkimp (gli_add_resource_from_path, as the classic
 * runner does -- see questglk-common.cc), which gives it a resource number
 * that glk_image_draw* / glk_schannel_play* then find. */

/* filename (case-folded) -> registered resource number; 0 = known-failed. */
inline std::map<std::string, glui32> g_image_ids;

inline std::map<std::string, glui32> g_sound_ids;

bool stage_temp_file(const std::string &bytes, std::string &path,
                     bool keep_for_session = false);
glui32 load_media_resource(const std::string &name, bool sound);
glui32 cached_media_id(std::map<std::string, glui32> &ids,
                       const std::string &name, bool sound);

#endif /* SPATTERLIGHT */

/* One sound channel, like the reference player's single <audio> element: a
 * new `play sound` replaces the current one, `stop sound` silences it
 * (shared single-channel semantics -- see questglk-common.cc). */
inline schanid_t g_schannel = nullptr;

void stop_sound_ui();
bool play_audio_inline(const std::vector<std::string> &srcs, bool loop);
void wait_for_sound(Interp &in, glui32 id);
void play_sound_ui(Interp &in, const std::string &name, bool sync, bool loop);
void do_wait_ui(Interp &in);
void do_pause_ui(Interp &in, int ms);
bool draw_image_inline(const std::string &name);

/* The frame picture (JS.setPanelContents) -------------------------------
 *
 * Quest's picture frame is a PERSISTENT panel above the transcript, not a
 * picture in it: Core sets it from the room's `picture` attribute on every
 * room change (OnEnterRoom -> SetFramePicture) and re-sets it at boot and
 * restore (InitInterface). The reference player's #gamePanel is
 * `position: sticky; top: 32px`, full width, contents centred, hidden when
 * empty, with the image capped at half the window height
 * (playercore.js: `(window.innerHeight - 30) * 0.5`) -- a sibling of
 * #gridPanel, which is why this gets the same treatment as the grid map.
 *
 * Drawn inline instead, a room picture ate most of the viewport, pushed the
 * room description below the fold, and then scrolled away for good, since a
 * re-set of the same file is deduped (The Shack: every room has art).
 *
 * Hosts without graphics (CheapGlk, the smoke harness) keep the old inline
 * path, so their transcripts are unchanged. */
inline winid_t gpanelwin = nullptr;    /* the band, open only while a picture is set */

inline std::string g_panel_last;       /* case-folded filename currently in the frame */

inline glui32 g_panel_id = 0;          /* its Glk image id */

bool panel_band_available();
void panel_set_background();
void panel_close();
void panel_relayout();
void panel_arrange();
void panel_contents(const std::string &html);
void flush_warnings(World &w, size_t &seen);

/* How a session ended, and what the next one boots from.  Resume never leaves
 * run_session: it is the post-game menu telling the turn loop to carry on. */
enum class SessionEnd { Quit, Restart, Restore, Resume };

bool post_game_undo(Interp &in);
SessionEnd post_game_menu(Interp &in, std::string &restore_data);
void update_timer_request(Interp &in);

#ifdef SPATTERLIGHT

using questglk::AutosaveRecord;
using questglk::blob_num;
using questglk::blob_str;
using questglk::BlobReader;

std::string utf8_from_u32(const std::u32string &s);

/* The blob's format stamp, bumped on every layout change (most recently when
 * the undo history and the replay record left it for the autosave record
 * both engines share): an older blob has different fields, and an
 * autorestore that mis-parses is worse than one discarded for a fresh
 * start.  aslx_recover_frontend rejects anything that does not match. */
inline const char *const kAslxBlobMagic = "ASLXGLK-AUTOSAVE 8";

std::string aslx_encode_frontend(Interp &in, const RngStreams *rngs = nullptr);
bool aslx_recover_frontend(const std::string &blob);
void panel_restore_picture();
void capture_turn_start(Interp &in, const std::string &state);
void aslx_do_autosave(Interp &in);
void note_turn_start(Interp &in, const std::string &cmd);
void aslx_do_menu_autosave(Interp &in, bool at_wait);

/* The hooks a replay mutes: everything that writes to the story window or
 * the frontend's record of it (the blob restored those as they were under
 * the menu), and everything that would block.  The pane, banner, map and
 * frame hooks keep running -- they redraw from engine state that the replay
 * brings back to what it was.  Emptied hooks stay SET, as no-ops: an unset
 * hook changes engine behaviour (see play_sound). */
inline std::vector<std::function<void(Interp &)>> g_replay_unmute;

/* Mute one hook, leaving behind what end_replay needs to put it back. */
template <typename F, typename Mute>
void mute_hook(Interp &in, F Interp::*hook, Mute mute)
{
    g_replay_unmute.push_back(
        [hook, saved = in.*hook](Interp &i) { i.*hook = saved; });
    if (in.*hook)
        in.*hook = mute;
}

void start_replay(Interp &in);
void end_replay(Interp &in);
bool replay_answer(const char *kinds, std::string *text);

/* A replay has come to a prompt with its record used up: this is the prompt
 * the autosave was taken under, already on screen and waiting.  False (and
 * the replay abandoned) if the record has an answer left that this prompt
 * could not take. */
void replay_diverged(Interp &in, const char *where);

bool replay_reached(Interp &in, const char *where);
void arm_prompt_autosave(Interp &in, bool resumed);
void replay_diverged(Interp &in, const char *where);
bool game_menu_pending(Interp &in);

#endif /* SPATTERLIGHT */

void install_host_hooks(Interp &in, bool &restart_requested);
SessionEnd run_session(const char *storyfile, std::string &restore_data);
void reset_frontend_state();

}  // namespace aslxglk

#endif /* QUESTION_ASLXGLK_INTERNAL_HH */
