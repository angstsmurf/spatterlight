/*$Id: //depot/prj/geas/master/code/geasglk.cc#10 $
  geasglk.cc

  User interface bridge from Geas Core to Glk.

  Copyright (C) 2006 David Jones.  Distribution or modification in any
  form permitted.

  Some code is taken from the public domain
  http://www.eblong.com/zarf/glk/model.c written by Andrew Plotkin.

  By the way, I can't write C++.  Sorry about that.


  Glk Window arrangement.

    +-------------------+
    |         B         |
    +-----------+---+---+
    |     M     | D | O |
    |           |   |   |
    +-----------+---+---+
    |         I         |
    +-------------------+

  B is a one line status bar (a TextGrid), kept in the global bannerwin.  It
  shows the current room name (left) and the game's status variables such as
  score/health/money (right).  It's optional, null if unavailable.  The game's
  title/author/version banner is printed once into the main window at startup,
  not here.
  M is the main window where the text of the game appears.  Kept in the
  global variable mainglkwin.
  I is a one line "input window" where the user inputs their commands.
  Kept in the global variable inputwin, it's optional, and if not separate
  is set to mainglkwin.
  O is an optional right-hand pane (objwin) listing the current room's objects
  and exits; it is opened only when there is something to list and closed
  otherwise (see update_objwin).  D (gfxwin) is a thin graphics window drawn in
  the text colour as a divider between M and O.
*/

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <cstdlib>

#include "GeasRunner.hh"

/* Presentation helpers shared with the Quest 5 frontend (aslxglk.cc): the
 * status banner, side pane + divider, transcript metaverb, save-file
 * prompts, string/UTF-8 utilities and resource registration. */
#include "questglk-common.inc"
using namespace questglk;

class GeasGlkInterface : public GeasInterface
{
protected:
    virtual std::string get_file (const std::string &) const;
    virtual GeasResult print_normal (const std::string &);
    virtual GeasResult print_newline ();

    virtual void set_foreground (const std::string &);
    virtual void set_background (const std::string &);
    virtual GeasResult set_style (const GeasFontStyle &);

    virtual std::string get_string ();
    virtual uint make_choice (const std::string &, std::vector<std::string>);
    virtual GeasResult play_sound (const std::string &filename, bool looped, bool sync);
    virtual GeasResult show_image (const std::string &filename, const std::string &resolution,
				   const std::string &caption, ...);
    virtual GeasResult wait_keypress (const std::string &);
    virtual bool has_objects_window ();

  virtual std::string absolute_name (const std::string &, const std::string &) const;
public:
    GeasGlkInterface() { ; }
};

static void glk_put_cstring(const char *);

/* The native Quest 5 engine (aslxglk.cc).  glk_main below sniffs the story
 * file and dispatches .aslx/.quest games there; .asl/.cas stay here. */
extern "C" int aslx_is_quest5_file(const char *path);
extern "C" void aslx_glk_main(const char *path);

extern "C" {

#include <assert.h>
#include "glk.h"

winid_t mainglkwin;
winid_t inputwin;
winid_t bannerwin;
winid_t objwin;                          /* right-hand pane: room objects + menus */
winid_t gfxwin;                          /* thin divider between main and objwin */
strid_t inputwinstream;
static strid_t transcriptstr = nullptr;  /* open transcript file, or null */
static bool g_manual_echo = false;       /* Glk line echo off; we echo input ourselves */
static bool g_output_seen = false;       /* set when print_* actually writes to the window */
static bool g_use_objpane = false;       /* host supports a side pane; objwin may be
                                          * momentarily closed (when empty) yet still "in use" */
static bool g_hyperlinks = false;        /* host supports clickable hyperlinks in a
                                          * text buffer (the object pane's entries) */

/* One click command per objwin hyperlink, indexed by (link value - 1).  Rebuilt
 * on every update_objwin: an object runs "verbs <alias>" (its verb menu, like
 * the Quest 5 pane), an exit runs its navigation command (a direction, "out",
 * or "go to <place>"). */
static std::vector<std::string> g_objlinks;

extern const char *storyfilename;  /* defined in geasglkterm.c */
extern int use_inputwindow;

/* Spatterlight's "slow draw / real-time delays" preference, defined in the
 * glkimp layer. When off (as in the deterministic import tests) we skip the
 * real-time timer heartbeat below. */
extern int gli_sa_delays;

static int ignore_lines = 0;  /* count of lines to ignore in game output */

static std::string banner;
static void draw_banner();
static void update_objwin(GeasRunner *gr);
static void fill_divider();
static void ensure_objwin_open();
static void close_objwin();
static std::string g_last_objlist;   /* last room-object list echoed to a transcript */
static std::string g_status_line;    /* status vars joined for the banner, rebuilt each turn */
static std::string g_room_name;      /* current room name, shown left-aligned in the banner */

/* Handle the transcript metaverb ("transcript"/"script" on/off).  Returns true
 * if the command was a transcript command (and should not reach the game).  A
 * running transcript echoes every line printed to the main window to the file. */
static bool
handle_transcript_command(const std::string &raw)
{
    int on = match_transcript_command(raw);
    if (!on)
        return false;
    toggle_transcript(on, mainglkwin, &transcriptstr);
    return true;
}

/* Prompt for a save file and restore it.  Returns true if the game state was
 * successfully restored (and is now running). */
static bool
do_restore(GeasRunner *gr)
{
    std::string data;
    if (!prompt_read_save(data))
        return false;
    if (gr->load_state(data)) {
        glk_put_string((char *) "\nGame restored.\n");
        return true;
    }
    glk_put_string((char *) "Sorry, that does not look like a saved game for this story.\n");
    return false;
}

/* Handle the SAVE / RESTORE metaverbs.  Returns true if the command was one of
 * them (and should not reach the game).  The whole game state goes through a
 * single Glk file; geas does the (de)serialising. */
static bool
handle_saverestore_command(const std::string &raw, GeasRunner *gr)
{
    std::string c = lower(trim(raw));
    bool save = (c == "save" || c == "save game");
    bool restore = (c == "restore" || c == "restore game" ||
                    c == "load" || c == "load game");
    if (!save && !restore)
        return false;

    if (save)
        prompt_write_save(gr->save_state());
    else
        do_restore(gr);
    return true;
}

/* Handle the QUIT metaverb: print a farewell and ask the loop to stop, rather
 * than letting the game end the session silently. */
static bool
handle_quit_command(const std::string &raw, bool &quitting)
{
    std::string c = lower(trim(raw));
    if (c != "quit" && c != "q" && c != "quit game")
        return false;

    glk_put_string((char *) "\nThanks for playing. Goodbye!\n");
    quitting = true;
    return true;
}

/* Handle the RESTART metaverb: clear the screen and start the game over. */
static bool
handle_restart_command(const std::string &raw, GeasRunner *gr)
{
    std::string c = lower(trim(raw));
    if (c != "restart" && c != "restart game")
        return false;

    glk_window_clear(mainglkwin);
    gr->restart();
    return true;
}

/* Handle the #HELP metaverb: list and explain the engine-level metaverbs this
 * port adds on top of the game's own commands.  Kept distinct from the game's
 * plain HELP (which prints Quest's in-game quick help) so it never shadows it. */
static bool
handle_help_command(const std::string &raw)
{
    std::string c = lower(trim(raw));
    if (c != "#help" && c != "#commands" && c != "metaverbs")
        return false;

    glk_put_string((char *)
        "\nThese system commands are handled by the interpreter, outside the game:\n"
        "\n"
        "  SAVE              Save the whole game to a file.\n"
        "  RESTORE  (LOAD)   Restore a previously saved game.\n"
        "  RESTART           Start the game over from the beginning.\n"
        "  UNDO              Take back the last turn.\n"
        "  QUIT     (Q)      Stop playing and leave Geas.\n"
        "\n"
        "  SCRIPT   (TRANSCRIPT)       Start recording the game text to a file.\n"
        "  SCRIPT OFF  (UNSCRIPT)      Stop recording the transcript.\n"
        "\n"
        "  OOPS <word>       Re-run your last command with a mistyped object\n"
        "                    word replaced by <word>.\n"
        "  VERBS <object>    List the actions available for an object.\n"
        "  ABOUT             Show the game's title, author, version and info.\n"
        "  HELP              Show the game's basic in-game help.\n"
        "  #HELP             Show this list of system commands.\n");
    return true;
}

/* After the game ends, ask the player what to do.  Returns 1 = undo,
 * 2 = restore, 3 = restart, 4 = quit.  Accepts the number or the word. */
static int
post_game_menu()
{
    glk_put_cstring("\nThe story has ended.  You can:\n");
    glk_put_cstring("  1. UNDO the last turn\n");
    glk_put_cstring("  2. RESTORE a saved game\n");
    glk_put_cstring("  3. RESTART\n");
    glk_put_cstring("  4. QUIT\n");
    if (g_manual_echo)
        glk_set_echo_line_event(inputwin, 1);   /* auto-echo the choice */
    char b[64];
    for (;;) {
        glk_put_string_stream(inputwinstream, (char *) "> ");
        glk_request_line_event(inputwin, b, (sizeof b) - 1, 0);
        event_t ev;
        do {
            glk_select(&ev);
            if (ev.type == evtype_Arrange || ev.type == evtype_Redraw) {
                draw_banner();
                fill_divider();
            }
        } while (!(ev.type == evtype_LineInput && ev.win == inputwin));
        /* lower-case, trimmed word (restart vs restore both start with 'r',
         * so match the whole word, not just the first letter) */
        std::string w;
        for (int i = 0; i < (int) ev.val1; i++)
            w += tolower((unsigned char) b[i]);
        w = trim(w);
        if (w == "1" || w == "u" || w == "undo")    return 1;
        if (w == "2" || w == "restore" || w == "load") return 2;
        if (w == "3" || w == "restart")             return 3;
        if (w == "4" || w == "q" || w == "quit")    return 4;
        glk_put_cstring("Please type 1, 2, 3 or 4 (or undo / restore / restart / quit).\n");
    }
}

void glk_main(void)
{
    char err_buf[1024];
    char cur_buf[1024];

    /* Quest 5 games (.quest zip with game.aslx inside, or raw <asl> XML) run
     * on the native aslx engine, a separate runner sharing this frontend. */
    if (storyfilename && aslx_is_quest5_file(storyfilename)) {
        aslx_glk_main(storyfilename);
        return;
    }

    glk_stylehint_set(wintype_TextBuffer, style_User2, stylehint_ReverseColor, 1);
    /* Open the main window. */
    mainglkwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 1);
    if (!mainglkwin) {
        /* It's possible that the main window failed to open. There's
            nothing we can do without it, so exit. */
        return; 
    }
    glk_set_window(mainglkwin);

    if (!storyfilename) {
	snprintf(err_buf, sizeof(err_buf), "No game name or more than one game name given.\n"
			 "Try -h for help.\n");
	glk_put_string(err_buf);
        return;
    }

    glk_stylehint_set (wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
    bannerwin = glk_window_open(mainglkwin,
                                winmethod_Above | winmethod_Fixed,
                                1, wintype_TextGrid, 0);

    if (use_inputwindow)
        inputwin = glk_window_open(mainglkwin,
                                   winmethod_Below | winmethod_Fixed,
                                   1, wintype_TextBuffer, 0);
    else
        inputwin = NULL;

    if (!inputwin)
        inputwin = mainglkwin;

    inputwinstream = glk_window_get_stream(inputwin);

    /* A right-hand pane listing the objects/characters in the current room
     * (and, later, menus).  Open it once to probe whether the host supports a
     * side pane, then close it again: update_objwin manages it dynamically,
     * opening it only when it has something to list and closing it (reclaiming
     * the width) when empty.  Probing-then-closing avoids leaving an empty pane
     * on screen during the game's intro / name prompt, which runs (and renders)
     * before the first update_objwin.  If it can't be opened we fall back to
     * listing objects in the main text (see has_objects_window). */
    ensure_objwin_open();
    g_use_objpane = (objwin != nullptr);
    close_objwin();

    /* Clickable hyperlinks for the object-pane entries (objects and exits), as
     * in the Quest 5 frontend.  Only when the host both draws and accepts
     * hyperlink input in a text buffer -- otherwise the pane stays plain text. */
    g_hyperlinks = glk_gestalt(gestalt_Hyperlinks, 0) &&
                   glk_gestalt(gestalt_HyperlinkInput, wintype_TextBuffer);

    /* We can turn off Glk's automatic line-input echo and echo entered text
     * ourselves; this is used for the command loop so that a timer cancelling
     * the input doesn't leave a stray newline and partial line on screen.
     * (get_string keeps auto-echo on -- it never cancels for a timer -- so its
     * input echoes inline at the prompt.)  echo is toggled per request. */
    g_manual_echo = (inputwin == mainglkwin) &&
                    glk_gestalt(gestalt_LineInputEcho, 0);

    if (!glk_gestalt(gestalt_Timer, 0)) {
	snprintf(err_buf, sizeof(err_buf),"\nNote -- The underlying Glk library does not support"
                         " timers.  If this game tries to use timers, then some"
                         " functionality may not work correctly.\n\n");
	glk_put_string(err_buf);
    }

    GeasRunner *gr = GeasRunner::get_runner(new GeasGlkInterface());
    gr->set_game(storyfilename);
    banner = gr->get_banner();

    /* Tell the host UI the game's title.  get_banner() returns
     * "<name>, v<version> | <author>"; pass just the leading name part. */
    std::string title = banner;
    std::string::size_type cut = title.find(", v");
    std::string::size_type bar = title.find(" | ");
    if (bar != std::string::npos && (cut == std::string::npos || bar < cut))
        cut = bar;
    if (cut != std::string::npos)
        title.erase(cut);
    if (!title.empty())
        garglk_set_story_title(title.c_str());

    /* The game's title/version/author banner is printed once into the main
     * window by the runner (set_game), before the game's own opening text --
     * the status bar is reserved for the current room name (left) and any
     * status vars (right). */
    draw_banner();
    update_objwin(gr);

    /* Only arm the 1-second heartbeat if the game actually defines timers and
     * real-time delays are enabled. Geas used to request timer events
     * unconditionally. The set of timers is fixed at load time, so a timerless
     * game never needs the heartbeat at all. And when real-time delays are off
     * -- as in the deterministic import tests --
     * suppressing the wall-clock ticks keeps the glk_select/NEXTEVENT count in
     * the input loop deterministic even for games that do use timers (e.g.
     * Gathered in Darkness). Gating on gli_sa_delays rather than gli_determinism
     * leaves room to still exercise a game's real-time events deterministically. */
    if (gr->has_timers() && gli_sa_delays)
        glk_request_timer_events(1000);

    char buf[200];
    bool quitting = false;

    while(!quitting) {
    while(gr->is_running() && !quitting) {
	strncpy(cur_buf, "> ", sizeof(cur_buf));
        if (inputwin != mainglkwin)
            glk_window_clear(inputwin);
        else
            glk_put_cstring("\n");
        glk_put_string_stream(inputwinstream, cur_buf);

        /* Echo off for the command line, so a timer cancelling it is clean. */
        if (g_manual_echo)
            glk_set_echo_line_event(inputwin, 0);
        glk_request_line_event(inputwin, buf, (sizeof buf) - 1, 0);

        event_t ev;
        ev.type = evtype_None;

        while(ev.type != evtype_LineInput) {
            glk_select(&ev);

            switch(ev.type) {
            case evtype_LineInput:
                if(ev.win == inputwin) {
                    std::string cmd = std::string(buf, ev.val1);
                    /* Auto-echo is off, so echo the entered command ourselves at
                     * the prompt (which was already printed above), so every
                     * command -- including the metaverbs below -- shows up. */
                    if (g_manual_echo)
                        echo_input_line(cmd, false);
                    if(!handle_transcript_command(cmd) &&
                       !handle_saverestore_command(cmd, gr) &&
                       !handle_restart_command(cmd, gr) &&
                       !handle_help_command(cmd) &&
                       !handle_quit_command(cmd, quitting)) {
                        if(inputwin == mainglkwin)
                            ignore_lines = 2;
                        gr->run_command(cmd);
                    }
                }
                break;

            case evtype_Timer:
                if (gr->timer_will_fire()) {
                    /* A timer fires (and may print or end the game) on this
                     * tick.  Cancel the pending input first -- glk forbids
                     * printing to a window with a live line-input request.
                     * With echo off the cancel prints nothing and leaves the
                     * "> " prompt in place (it is before the input fence). */
                    event_t ce;
                    glk_cancel_line_event(inputwin, &ce);
                    g_output_seen = false;
                    gr->tick_timers();
                    draw_banner();
                    if (gr->is_running()) {
                        /* If the timer printed something (e.g. surviving the
                         * dynamite), the old prompt now has text after it, so
                         * show a fresh prompt.  A silent timer (the interval-0
                         * mayor-door check) leaves the existing prompt alone. */
                        if (g_output_seen) {
                            if (inputwin != mainglkwin)
                                glk_window_clear(inputwin);
                            else
                                glk_put_cstring("\n");
                            glk_put_string_stream(inputwinstream, (char *) "> ");
                        }
                        glk_request_line_event(inputwin, buf,
                                               (sizeof buf) - 1, ce.val1);
                    }
                } else {
                    /* Just counting down: no output, so the live input is fine. */
                    gr->tick_timers();
                }
                break;

            case evtype_Hyperlink:
                /* A click on an object-pane entry runs its command as if typed
                 * (objects -> "verbs <object>", exits -> their navigation). */
                if (g_hyperlinks && ev.win == objwin &&
                    ev.val1 >= 1 && ev.val1 <= (glui32) g_objlinks.size()) {
                    std::string cmd = g_objlinks[ev.val1 - 1];
                    /* Cancel the live line input first -- Glk forbids printing to
                     * a window with a pending request; with echo off the cancel
                     * leaves nothing on screen. */
                    event_t ce;
                    glk_cancel_line_event(inputwin, &ce);
                    /* Echo the clicked command so the player sees what ran.  With
                     * input in the main window the "> " prompt is already there
                     * and run_command's own "> cmd" echo is suppressed
                     * (ignore_lines below), so echo the command here -- there is
                     * no typed text for the library to echo.  With a separate
                     * input window run_command prints its own "> cmd" into the
                     * main text (ignore_lines stays 0), so no manual echo. */
                    if (inputwin == mainglkwin)
                        echo_input_line(cmd, false);
                    if (!handle_transcript_command(cmd) &&
                        !handle_saverestore_command(cmd, gr) &&
                        !handle_restart_command(cmd, gr) &&
                        !handle_help_command(cmd) &&
                        !handle_quit_command(cmd, quitting)) {
                        if (inputwin == mainglkwin)
                            ignore_lines = 2;
                        gr->run_command(cmd);
                    }
                    /* Treat the click as this turn's input so the loop exits,
                     * refreshes the pane (re-arming the link) and re-prompts. */
                    ev.type = evtype_LineInput;
                } else if (g_hyperlinks && objwin) {
                    /* Out-of-range (pane changed under the click): just re-arm. */
                    glk_request_hyperlink_event(objwin);
                }
                break;

            case evtype_Arrange:
            case evtype_Redraw:
                draw_banner();
                update_objwin(gr);
                fill_divider();
                break;
            }

            /* A timer (e.g. World's End's dynamite) may have ended the game
             * while we were waiting at the prompt.  Stop now and show the
             * ending, rather than leaving the prompt up.  (The Timer case has
             * already cancelled the line request in that case.) */
            if (!gr->is_running())
                break;
        }
        /* The command (or a timer) may have changed room; refresh the bar
         * and the room-objects pane. */
        draw_banner();
        update_objwin(gr);
    }

    if (quitting)
        break;

    /* The game has ended (death or win); offer undo / restart / quit
     * instead of just closing the session. */
    for (;;) {
        int c = post_game_menu();
        if (c == 1) {                       /* UNDO */
            if (gr->undo())
                break;                      /* resurrected; resume play */
            glk_put_cstring("There is nothing to undo.\n");
        } else if (c == 2) {                /* RESTORE */
            if (do_restore(gr))
                break;                      /* restored; resume play */
        } else if (c == 3) {                /* RESTART */
            glk_window_clear(mainglkwin);
            gr->restart();
            break;
        } else {                            /* QUIT */
            glk_put_cstring("\nThanks for playing. Goodbye!\n");
            quitting = true;
            break;
        }
    }
    if (gr->is_running()) {
        draw_banner();
        update_objwin(gr);
        fill_divider();
    }
    }
}

} /* extern "C" */

void
draw_banner()
{
  /* The current room name, left-aligned, and the status vars right-aligned.
   * (The game's title/version/author banner is shown once at startup in the
   * main window, not here.)  Byte mode: classic geas text passes through
   * verbatim. */
  draw_status_banner(bannerwin, g_room_name, g_status_line, false);
}

/* Open the right-hand pane (and its divider), if not already open and the host
 * supports it.  Mirrors the startup arrangement so it can be reopened after the
 * pane was closed for an empty room. */
static void
ensure_objwin_open()
{
    open_side_pane_windows(mainglkwin, &objwin, &gfxwin);
}

/* Close the pane and its divider so the main window reclaims the full width. */
static void
close_objwin()
{
    close_side_pane_windows(&objwin, &gfxwin);
}

/* Write one pane entry (on its own line) to the objwin stream.  When the host
 * supports hyperlinks the label is made clickable: its link value is its
 * 1-based index in g_objlinks, and a click runs `command` as if the player had
 * typed it (see the evtype_Hyperlink handling in glk_main). */
static void
put_objwin_link(strid_t s, const std::string &label, const std::string &command)
{
    glui32 linkval = 0;
    if (g_hyperlinks) {
        g_objlinks.push_back(command);
        linkval = (glui32) g_objlinks.size();
    }
    put_pane_link(s, label, linkval, false);
}

/* Redraw the right-hand pane with the objects/characters in the current room.
 * The pane (and its divider) is shown only when it has something to list --
 * objects or exits; the room-name header alone does not justify it, so a room
 * with neither closes the pane and gives the width back to the main text.
 * If a transcript is running, also echo the list to it (the pane is not part
 * of the main window's echo stream), but only when it changes. */
static void
update_objwin(GeasRunner *gr)
{
    /* Gather everything first so we can decide whether the pane has any real
     * content before opening or closing it. */
    std::string room = gr->get_location();
    if (!room.empty())
        room[0] = toupper((unsigned char) room[0]);
    g_room_name = room;

    v2string contents = gr->get_room_contents();
    std::string flat;
    for (std::vector<std::string> &item : contents) {
        if (item.empty())
            continue;
        if (!flat.empty())
            flat += ", ";
        flat += item[0];
    }

    /* Each exit is a {display label, click command} pair. */
    v2string exits = gr->get_room_exits();
    std::string flatexits;
    for (std::vector<std::string> &exit : exits) {
        if (exit.empty() || exit[0].empty())
            continue;
        if (!flatexits.empty())
            flatexits += ", ";
        flatexits += exit[0];
    }

    /* Status variables (Quest's "collectables": money/health/score etc.,
     * defined via `define variable ... display <...>`).  Show them right-aligned
     * in the status bar instead of the object pane. */
    vstring status = gr->get_status_vars();
    std::string flatstatus;
    g_status_line.clear();
    for (std::string &var : status) {
        if (var.empty())
            continue;
        if (!flatstatus.empty())
            flatstatus += ", ";
        flatstatus += var;
        if (!g_status_line.empty())
            g_status_line += " | ";
        g_status_line += var;
    }
    draw_banner();

    /* Show the pane only when there are objects or exits to list. */
    bool show = !flat.empty() || !flatexits.empty();
    if (g_use_objpane) {
        if (show)
            ensure_objwin_open();
        else
            close_objwin();
    }

    /* Rebuild the click-command table for the pane's hyperlinks from scratch;
     * the entries below append to it in draw order (its indices are the link
     * values). */
    g_objlinks.clear();

    if (objwin) {
        glk_window_clear(objwin);
        strid_t s = glk_window_get_stream(objwin);

        /* List the room's objects.  Clicking one lists its verb menu, matching
         * the Quest 5 pane's default ("verbs <object>"). */
        for (std::vector<std::string> &item : contents) {
            if (item.empty())
                continue;
            put_objwin_link(s, cap_first(item[0]), "verbs " + item[0]);
        }

        /* List the room's exits below the objects, under their own subheader
         * (the original Quest runner showed exits in a separate pane).  Clicking
         * an exit runs its navigation command (a direction, "out", or
         * "go to <place>"). */
        if (!exits.empty()) {
            /* Separate exits from the objects above, but only when there are
             * objects -- with none, "Exits" sits at the very top of the pane. */
            if (!flat.empty())
                glk_put_char_stream(s, '\n');
            put_pane_header(s, "Exits", false);
            for (std::vector<std::string> &exit : exits) {
                if (exit.empty() || exit[0].empty())
                    continue;
                std::string command = exit.size() > 1 ? exit[1] : exit[0];
                put_objwin_link(s, cap_first(exit[0]), command);
            }
        }
        fill_divider();

        /* Re-arm the pane's hyperlink input: clearing the window above drops any
         * pending request, and reopening it (for a previously empty room) makes
         * a fresh window with none.  The main input loop consumes the event. */
        if (g_hyperlinks)
            glk_request_hyperlink_event(objwin);
    }

    std::string key = room + "\x01" + flat + "\x01" + flatexits + "\x01" + flatstatus;
    if (transcriptstr && key != g_last_objlist) {
        std::string line = "[ " + (room.empty() ? std::string("Here") : room) +
            ": " + (flat.empty() ? std::string("nothing") : flat) +
            (flatexits.empty() ? std::string("") : "; exits: " + flatexits) +
            (flatstatus.empty() ? std::string("") : "; status: " + flatstatus) +
            " ]\n";
        glk_put_string_stream(transcriptstr, (char *) line.c_str());
    }
    g_last_objlist = key;
}

bool
GeasGlkInterface::has_objects_window ()
{
    /* True whenever the host is using a side pane, even if it is momentarily
     * closed for an empty room -- so the runner consistently routes object and
     * exit listings to the pane rather than duplicating them in the main text. */
    return g_use_objpane;
}

/* Paint the divider window in the current text colour.  Graphics windows are
 * blanked on resize, so this is also called on Arrange/Redraw. */
static void
fill_divider()
{
    fill_side_divider(mainglkwin, gfxwin);
}

void
glk_put_cstring(const char *s)
{
    /* The cast to remove const is necessary because glk_put_string
     * receives a "char *" despite the fact that it could equally well use
     * "const char *". */
    glk_put_string((char *)s);
}

GeasResult
GeasGlkInterface::print_normal (const std::string &s)
{
    if(!ignore_lines)
      {
	glk_put_cstring(s.c_str());
	g_output_seen = true;
      }
    return r_success;
}

GeasResult
GeasGlkInterface::print_newline ()
{
    if (!ignore_lines)
      {
	glk_put_cstring("\n");
	g_output_seen = true;
      }
    else
      {
	ignore_lines--;
      }
    return r_success;
}


GeasResult
GeasGlkInterface::set_style (const GeasFontStyle &style)
{
    // Glk styles are defined before the window opens, so at this point we can only
    // pick the most suitable style, not define a new one.
    glui32 match;
    if (style.is_italic && style.is_bold)
      {
	match = style_Alert;
      }
    else if (style.is_italic)
      {
	match = style_Emphasized;
      }
    else if (style.is_bold)
      {
	match = style_Subheader;
      }
    else if (style.is_underlined)
      {
	match = style_User2;
      }
    else
      {
	match = style_Normal;
      }

    glk_set_style_stream(glk_window_get_stream(mainglkwin), match);
    return r_success;
}

void
GeasGlkInterface::set_foreground (const std::string &s)
{ 
    if (s != "") 
    {
    }
}

void
GeasGlkInterface::set_background (const std::string &s)
{ 
    if (s != "") 
    {
    }
}


/* Code lifted from GeasWindow.  Should be common.  Maybe in
 * GeasInterface?
 */
std::string
GeasGlkInterface::get_file (const std::string &fname) const
{
  std::ifstream ifs;
  ifs.open(fname.c_str(), std::ios::in | std::ios::binary);
  if (! ifs.is_open())
    {
      /* Report to the log, not the game window: a missing file is usually a
       * standard Quest library the game !includes (e.g. Typelib.qlb) that Geas
       * either implements natively or doesn't need, so the player should never
       * see a raw "Couldn't open <path>" line. */
      std::cerr << "Couldn't open " << fname << "\n";
      return "";
    }
  std::string rv;
  char ch;
  ifs.get(ch);
  while (!ifs.eof())
    { 
      rv += ch;
      ifs.get(ch);
    } 
  return rv;
}

/* Show the message (if any) and wait for a single keypress.  Used for the
 * intro's "|w" and the game's "wait <...>" pauses. */
GeasResult
GeasGlkInterface::wait_keypress (const std::string &msg)
{
  if (!msg.empty())
    print_formatted(msg);
  glk_request_char_event(mainglkwin);
  /* A click on a pane hyperlink also dismisses the wait, like any keypress
   * (matching the Quest 5 frontend); the click's command is not run here. */
  if (g_hyperlinks && objwin)
    glk_request_hyperlink_event(objwin);
  event_t ev;
  for (;;)
    {
      glk_select(&ev);
      if (ev.type == evtype_CharInput && ev.win == mainglkwin)
        break;
      if (ev.type == evtype_Hyperlink && ev.win == objwin)
        {
          glk_cancel_char_event(mainglkwin);
          break;
        }
      if (ev.type == evtype_Arrange || ev.type == evtype_Redraw)
        {
          draw_banner();
          fill_divider();
        }
      /* timers deliberately ignored: the game is paused for the keypress */
    }
  return r_success;
}

std::string
GeasGlkInterface::get_string ()
{
  char buf[200];
  /* Use Glk's own echo here: get_string ignores timers, so it never cancels
   * its input, and auto-echo places the entry inline at the prompt. */
  if (g_manual_echo)
      glk_set_echo_line_event(inputwin, 1);
  glk_request_line_event(inputwin, buf, (sizeof buf) - 1, 0);
  while(1) {
    event_t ev;

    glk_select(&ev);

    if (ev.type == evtype_LineInput && ev.win == inputwin) {
      return std::string(buf, ev.val1);
    }
    /* All other events, including timer, are deliberately
     * ignored.
     */
  }
}

uint
GeasGlkInterface::make_choice (const std::string &label, std::vector<std::string> v)
{
    size_t n;

    /* Only clear a *separate* input window; if input shares the main window
     * this would wipe the whole screen before every menu. */
    if (inputwin != mainglkwin)
        glk_window_clear(inputwin);

    glk_put_cstring(label.c_str());
    glk_put_char(0x0a);
    n = v.size();
    for (size_t i=0; i<n; ++i)
      {
	std::stringstream t;
	std::string s;
	t << i+1;
	t >> s;
	glk_put_cstring(s.c_str());
	glk_put_cstring(": ");
	glk_put_cstring(v[i].c_str());
	glk_put_cstring("\n");
      }

    std::stringstream t;
    std::string s;
    std::string s1;
    t << n;
    t >> s;
    s1 = "Choose [1-" + s + "]> ";
    glk_put_string_stream(inputwinstream, (char *)(s1.c_str()));

    int choice = atoi(get_string().c_str());
    if (choice < 1)
      {
	choice = 1;
      }
    if ((size_t)choice > n)
      {
	choice = (int)n;
      }

    /* The chosen line was already echoed by get_string; just leave a blank
     * line after the menu. */
    glk_put_cstring("\n");

    return choice - 1;
}

std::string GeasGlkInterface::absolute_name (const std::string &rel_name, const std::string &parent) const {
  std::cerr << "absolute_name ('" << rel_name << "', '" << parent << "')\n";
  if (parent[0] != '/')
    {
      return rel_name;
    }

  if (rel_name[0] == '/')
    {
      std::cerr << "  --> " << rel_name << "\n";
      return rel_name;
    }
  std::vector<std::string> path;
  uint dir_start = 1, dir_end;
  while (dir_start < parent.length())
    {
      dir_end = dir_start;
      while (dir_end < parent.length() && parent[dir_end] != '/')
	{
	  dir_end ++;
	}
      path.push_back (parent.substr (dir_start, dir_end - dir_start));
      dir_start = dir_end + 1;
    }
  path.pop_back();
  dir_start = 0;
  std::string tmp;
  while (dir_start < rel_name.length())
    {
      dir_end = dir_start;
      while (dir_end < rel_name.length() && rel_name[dir_end] != '/')
	{
	  dir_end ++;
	}
      tmp = rel_name.substr (dir_start, dir_end - dir_start);
      dir_start = dir_end + 1;
      if (tmp == ".")
	{
	  continue;
	}
      else if (tmp == "..")
	{
	  path.pop_back();
	}
      else
	{
	  path.push_back (tmp);
	}
    }
  std::string rv;
  for (const auto &i: path)
    {
      rv = rv + "/" + i;
    }
  std::cerr << " ---> " << rv << "\n";
  return rv;
}

/* Audio and images go through the by-name resource registration shared with
 * the Quest 5 frontend (register_path_resource in questglk-common.inc). */

static schanid_t geas_soundchannel = NULL;

GeasResult
GeasGlkInterface::play_sound (const std::string &filename, bool looped, bool /*sync*/)
{
  /* Quest's "sync" flag blocks until the sound finishes; we play
   * asynchronously to avoid stalling the turn loop. */

  /* An empty filename is Geas's request to stop all sound. */
  if (filename.empty())
    {
      stop_single_sound (&geas_soundchannel);
      return r_success;
    }

  std::string parent = storyfilename ? storyfilename : "";
  std::string path = absolute_name (filename, parent);

  /* Assign each distinct file a stable resource number and load it once. */
  static std::map<std::string, int> sound_ids;
  int resno = register_path_resource (sound_ids, path, true);
  if (!resno)
    {
      std::cerr << "play_sound: cannot open " << path << "\n";
      return r_not_supported;
    }

  /* One sound at a time: a new sound replaces the previous one. */
  if (!play_single_sound (&geas_soundchannel, (glui32) resno, looped, 0))
    return r_not_supported;   /* sound disabled or unsupported */
  return r_success;
}

GeasResult
GeasGlkInterface::show_image (const std::string &filename, const std::string &resolution,
			     const std::string & /*caption*/, ...)
{
  if (filename.empty())
    return r_not_supported;
  /* Need a Glk that can draw images inline in the text-buffer window. */
  if (!glk_gestalt (gestalt_Graphics, 0) ||
      !glk_gestalt (gestalt_DrawImage, wintype_TextBuffer))
    return r_not_supported;

  std::string parent = storyfilename ? storyfilename : "";
  std::string path = absolute_name (filename, parent);

  static std::map<std::string, int> image_ids;
  int resno = register_path_resource (image_ids, path, false);
  if (!resno)
    {
      std::cerr << "show_image: cannot open " << path << "\n";
      return r_not_supported;
    }

  /* Draw on its own line in the main window.  Honour the optional "<W>x<H>"
   * display size Quest attaches to a picture ("file@523x348"); fall back to the
   * image's native size when it is absent or unparseable. */
  glk_put_char (0x0a);
  glui32 w = 0, h = 0;
  std::string::size_type x = resolution.find_first_of ("xX");
  if (x != std::string::npos)
    {
      w = (glui32) strtoul (resolution.c_str(), nullptr, 10);
      h = (glui32) strtoul (resolution.c_str() + x + 1, nullptr, 10);
    }
  if (w > 0 && h > 0)
    glk_image_draw_scaled (mainglkwin, (glui32) resno, imagealign_InlineCenter, 0, w, h);
  else
    glk_image_draw (mainglkwin, (glui32) resno, imagealign_InlineCenter, 0);
  glk_put_char (0x0a);
  return r_success;
}




#if 0
void GeasGlkInterface::debug_print (std::string s) { ; }

GeasResult GeasGlkInterface::pause (int msec) { return r_success; }
#endif
