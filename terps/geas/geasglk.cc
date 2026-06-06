/*$Id: //depot/prj/geas/master/code/geasglk.cc#10 $
  geasglk.cc

  User interface bridge from Geas Core to Glk.

  Copyright (C) 2006 David Jones.  Distribution or modification in any
  form permitted.

  Some code is taken from the public domain
  http://www.eblong.com/zarf/glk/model.c written by Andrew Plotkin.

  By the way, I can't write C++.  Sorry about that.


  Glk Window arrangment.

    +---------+
    |    B    |
    +---------+
    |    M    |
    |         |
    +---------+
    |    I    |
    +---------+

  B is a one line "banner window", showing the game name and author.  Kept
  in the global variable, it's optional, null if unavailable.
  optional.
  M is the main window where the text of the game appears.  Kept in the
  global variable mainglkwin.
  I is a one line "input window" where the user inputs their commands.
  Kept in the global variable inputwin, it's optional, and if not separate
  is set to mainglkwin.

  Maybe in future revisions there will be a status window (including a
  compass rose).
*/

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <cstdlib>

#include "GeasRunner.hh"

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
    virtual bool has_objects_window ();

  virtual std::string absolute_name (const std::string &, const std::string &) const;
public:
    GeasGlkInterface() { ; }
};

static void glk_put_cstring(const char *);

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

extern const char *storyfilename;  /* defined in geasglkterm.c */
extern int use_inputwindow;

static int ignore_lines = 0;  /* count of lines to ignore in game output */

static std::string banner;
static void draw_banner();
static void update_objwin(GeasRunner *gr);
static void fill_divider();
static std::string g_last_objlist;   /* last room-object list echoed to a transcript */

/* Handle the transcript metaverb ("transcript"/"script" on/off).  Returns true
 * if the command was a transcript command (and should not reach the game).  A
 * running transcript echoes every line printed to the main window to the file. */
static bool
handle_transcript_command(const std::string &raw)
{
    std::string c;
    for (char ch : raw)
        c += tolower((unsigned char) ch);
    std::string::size_type a = c.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return false;
    std::string::size_type b = c.find_last_not_of(" \t\r\n");
    c = c.substr(a, b - a + 1);

    bool on  = (c == "transcript" || c == "transcript on" ||
                c == "script" || c == "script on");
    bool off = (c == "transcript off" || c == "script off" ||
                c == "notranscript" || c == "noscript");
    if (!on && !off)
        return false;

    if (on)
      {
        if (transcriptstr)
          {
            glk_put_string((char *) "A transcript is already being recorded.\n");
            return true;
          }
        frefid_t fref = glk_fileref_create_by_prompt(
            fileusage_Transcript | fileusage_TextMode, filemode_Write, 0);
        if (!fref)
          {
            glk_put_string((char *) "Transcript cancelled.\n");
            return true;
          }
        transcriptstr = glk_stream_open_file(fref, filemode_Write, 0);
        glk_fileref_destroy(fref);
        if (!transcriptstr)
          {
            glk_put_string((char *) "Could not open the transcript file.\n");
            return true;
          }
        glk_window_set_echo_stream(mainglkwin, transcriptstr);
        glk_put_string((char *) "Transcript on: game text is now being saved to a file.\n");
      }
    else
      {
        if (!transcriptstr)
          {
            glk_put_string((char *) "No transcript is being recorded.\n");
            return true;
          }
        glk_put_string((char *) "Transcript off.\n");
        glk_window_set_echo_stream(mainglkwin, nullptr);
        glk_stream_close(transcriptstr, nullptr);
        transcriptstr = nullptr;
      }
    return true;
}

/* Handle the SAVE / RESTORE metaverbs.  Returns true if the command was one of
 * them (and should not reach the game).  The whole game state goes through a
 * single Glk file; geas does the (de)serialising. */
static bool
handle_saverestore_command(const std::string &raw, GeasRunner *gr)
{
    std::string c;
    for (char ch : raw)
        c += tolower((unsigned char) ch);
    std::string::size_type a = c.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return false;
    std::string::size_type b = c.find_last_not_of(" \t\r\n");
    c = c.substr(a, b - a + 1);

    bool save = (c == "save" || c == "save game");
    bool restore = (c == "restore" || c == "restore game" ||
                    c == "load" || c == "load game");
    if (!save && !restore)
        return false;

    glui32 usage = fileusage_SavedGame | fileusage_BinaryMode;
    if (save)
      {
        frefid_t fref = glk_fileref_create_by_prompt(usage, filemode_Write, 0);
        if (!fref) { glk_put_string((char *) "Save cancelled.\n"); return true; }
        strid_t str = glk_stream_open_file(fref, filemode_Write, 0);
        glk_fileref_destroy(fref);
        if (!str) { glk_put_string((char *) "Could not open the save file.\n"); return true; }
        std::string data = gr->save_state();
        glk_put_buffer_stream(str, (char *) data.data(), (glui32) data.size());
        glk_stream_close(str, nullptr);
        glk_put_string((char *) "Game saved.\n");
      }
    else
      {
        frefid_t fref = glk_fileref_create_by_prompt(usage, filemode_Read, 0);
        if (!fref) { glk_put_string((char *) "Restore cancelled.\n"); return true; }
        strid_t str = glk_stream_open_file(fref, filemode_Read, 0);
        glk_fileref_destroy(fref);
        if (!str) { glk_put_string((char *) "Could not open the save file.\n"); return true; }
        std::string data;
        char buf[4096];
        glui32 n;
        while ((n = glk_get_buffer_stream(str, buf, sizeof buf)) > 0)
            data.append(buf, n);
        glk_stream_close(str, nullptr);
        if (gr->load_state(data))
            glk_put_string((char *) "\nGame restored.\n");
        else
            glk_put_string((char *) "Sorry, that does not look like a saved game for this story.\n");
      }
    return true;
}

/* Handle the QUIT metaverb: print a farewell and ask the loop to stop, rather
 * than letting the game end the session silently. */
static bool
handle_quit_command(const std::string &raw, bool &quitting)
{
    std::string c;
    for (char ch : raw)
        c += tolower((unsigned char) ch);
    std::string::size_type a = c.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return false;
    std::string::size_type b = c.find_last_not_of(" \t\r\n");
    c = c.substr(a, b - a + 1);

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
    std::string c;
    for (char ch : raw)
        c += tolower((unsigned char) ch);
    std::string::size_type a = c.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return false;
    std::string::size_type b = c.find_last_not_of(" \t\r\n");
    c = c.substr(a, b - a + 1);

    if (c != "restart" && c != "restart game")
        return false;

    glk_window_clear(mainglkwin);
    gr->restart();
    return true;
}

void glk_main(void)
{
    char err_buf[1024];
    char cur_buf[1024];
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
     * (and, later, menus).  If it can't be opened we fall back to listing
     * objects in the main text (see GeasGlkInterface::has_objects_window). */
    objwin = glk_window_open(mainglkwin,
                             winmethod_Right | winmethod_Proportional,
                             20, wintype_TextBuffer, 0);

    /* A thin graphics window between the main text and the pane, drawn in the
     * text colour, as a divider. */
    if (objwin && glk_gestalt(gestalt_Graphics, 0))
        gfxwin = glk_window_open(objwin, winmethod_Left | winmethod_Fixed,
                                 2, wintype_Graphics, 0);
    fill_divider();

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
    draw_banner();
    update_objwin(gr);

    glk_request_timer_events(1000);

    char buf[200];
    bool quitting = false;

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
                    if (g_manual_echo) {
                        glk_set_style(style_Input);
                        glk_put_cstring(cmd.c_str());
                        glk_set_style(style_Normal);
                        glk_put_char('\n');
                    }
                    if(!handle_transcript_command(cmd) &&
                       !handle_saverestore_command(cmd, gr) &&
                       !handle_restart_command(cmd, gr) &&
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
}

} /* extern "C" */

void
draw_banner()
{
  glui32 width;
  int index;
  if (bannerwin)
    {
      glk_window_clear(bannerwin);
      glk_window_move_cursor(bannerwin, 0, 0);
      strid_t stream = glk_window_get_stream(bannerwin);

      glk_set_style_stream(stream, style_User1);
      glk_window_get_size (bannerwin, &width, NULL);
      for (index = 0; index < (int) width; index++)
        glk_put_char_stream (stream, ' ');

      /* The game title.  (The current room name is shown as the objects-pane
       * header, not here.) */
      std::string title = banner.empty() ? std::string("Geas 0.4") : banner;
      glk_window_move_cursor(bannerwin, 1, 0);
      glk_put_string_stream(stream, (char*) title.c_str());
    }
}

/* Redraw the right-hand pane with the objects/characters in the current room.
 * If a transcript is running, also echo the list to it (the pane is not part
 * of the main window's echo stream), but only when it changes. */
static void
update_objwin(GeasRunner *gr)
{
    if (!objwin)
        return;
    glk_window_clear(objwin);
    strid_t s = glk_window_get_stream(objwin);

    /* The room name is the pane's header (shown even when the room is empty). */
    std::string room = gr->get_location();
    if (!room.empty())
        room[0] = toupper((unsigned char) room[0]);
    glk_set_style_stream(s, style_Subheader);
    glk_put_string_stream(s, (char *) (room + "\n").c_str());
    glk_set_style_stream(s, style_Normal);

    v2string contents = gr->get_room_contents();
    std::string flat;
    for (std::vector<std::string> &item : contents) {
        if (item.empty())
            continue;
        glk_put_string_stream(s, (char *) item[0].c_str());
        glk_put_char_stream(s, '\n');
        if (!flat.empty())
            flat += ", ";
        flat += item[0];
    }

    std::string key = room + "\x01" + flat;
    if (transcriptstr && key != g_last_objlist) {
        std::string line = "[ " + (room.empty() ? std::string("Here") : room) +
            ": " + (flat.empty() ? std::string("nothing") : flat) + " ]\n";
        glk_put_string_stream(transcriptstr, (char *) line.c_str());
    }
    g_last_objlist = key;
}

bool
GeasGlkInterface::has_objects_window ()
{
    return objwin != nullptr;
}

/* Paint the divider window in the current text colour.  Graphics windows are
 * blanked on resize, so this is also called on Arrange/Redraw. */
static void
fill_divider()
{
    if (!gfxwin)
        return;
    glui32 color;
    if (!glk_style_measure(mainglkwin, style_Normal, stylehint_TextColor, &color))
        color = 0;   /* fall back to black */
    glui32 w = 0, h = 0;
    glk_window_get_size(gfxwin, &w, &h);
    glk_window_fill_rect(gfxwin, color, 0, 0, w, h);
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
      glk_put_cstring("Couldn't open ");
      glk_put_cstring(fname.c_str());
      glk_put_char(0x0a);
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

    std::stringstream u;
    u << choice;
    u >> s;
    s1 = "Chosen: " +  s + "\n";
    glk_put_cstring(s1.c_str());

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

/* glkimp helpers (declared in glkimp.h, which we don't include here).  Quest/
 * Geas reference audio by file name (e.g. "die.wav"), not by numbered Blorb
 * resource, so we pre-register each file under a synthetic resource number
 * with win_loadsound().  glk_schannel_play_ext() then short-circuits its
 * SND<n> lookup because win_findsound() reports the number already loaded. */
extern "C" int  win_findsound (int resno);
extern "C" void win_loadsound (int resno, char *filename, int offset, int reslen);

static schanid_t geas_soundchannel = NULL;

GeasResult
GeasGlkInterface::play_sound (const std::string &filename, bool looped, bool /*sync*/)
{
  /* Quest's "sync" flag blocks until the sound finishes; we play
   * asynchronously to avoid stalling the turn loop. */

  /* An empty filename is Geas's request to stop all sound. */
  if (filename.empty())
    {
      if (geas_soundchannel)
	glk_schannel_stop (geas_soundchannel);
      return r_success;
    }

  if (!geas_soundchannel)
    {
      geas_soundchannel = glk_schannel_create (0);
      if (!geas_soundchannel)
	return r_not_supported;   /* sound disabled or unsupported */
    }

  /* Quest plays one sound at a time; a new sound replaces the previous one. */
  glk_schannel_stop (geas_soundchannel);

  std::string parent = storyfilename ? storyfilename : "";
  std::string path = absolute_name (filename, parent);

  /* Assign each distinct file a stable resource number and load it once. */
  static std::map<std::string, int> sound_ids;
  auto it = sound_ids.find (path);
  int resno;
  if (it == sound_ids.end())
    resno = sound_ids[path] = (int) sound_ids.size() + 1;
  else
    resno = it->second;

  if (!win_findsound (resno))
    {
      std::ifstream f (path.c_str(), std::ios::binary | std::ios::ate);
      if (!f.is_open())
	{
	  std::cerr << "play_sound: cannot open " << path << "\n";
	  return r_not_supported;
	}
      int len = (int) f.tellg();
      win_loadsound (resno, (char *) path.c_str(), 0, len);
    }

  glui32 repeats = looped ? 0xffffffffu : 1u;   /* 0xffffffff == loop forever */
  glk_schannel_play_ext (geas_soundchannel, (glui32) resno, repeats, 0);
  return r_success;
}




#if 0
void GeasGlkInterface::debug_print (std::string s) { ; }

GeasResult GeasGlkInterface::pause (int msec) { return r_success; }
#endif
