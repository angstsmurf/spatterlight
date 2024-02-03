// Copyright 2010-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <list>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
#ifdef SPATTERLIGHT
#include "glkimp.h"
#endif
}

#ifdef SPATTERLIGHT
#include "random.h"
#include "spatterlight-autosave.h"
#include "draw_png.h"
#include "draw_image.hpp"
#include "arthur.hpp"
#include "journey.hpp"
#include "shogun.hpp"
#include "zorkzero.hpp"
#include "find_graphics_files.hpp"
#include "extract_apple_2.h"

#include "entrypoints.hpp"

extern long last_random_seed;
extern int random_calls_count;
#endif

#ifdef ZERP_GLK_WINGLK
// rpcndr.h, eventually included via WinGlk.h, defines a type “byte”
// which conflicts with the “byte” from memory.h. Temporarily redefine
// it to avoid a compile error.
#define byte rpc_byte
// Windows Glk puts non-standard declarations (specifically for this
// file, those guarded by GLK_MODULE_GARGLKTEXT) in WinGlk.h, so include
// it to get color/style extensions.
#include <WinGlk.h>
#undef byte
#endif
#endif

#include "screen.h"
#include "branch.h"
#include "dict.h"
#include "iff.h"
#include "io.h"
#include "memory.h"
#include "meta.h"
#include "objects.h"
#include "osdep.h"
#include "process.h"
#include "sound.h"
#include "stack.h"
#include "stash.h"
#include "types.h"
#include "unicode.h"
#include "util.h"
#include "zterp.h"

// Somewhat ugly hack to get around the fact that some Glk functions may
// not exist. These function calls should all be guarded (e.g.
// if (have_unicode), with have_unicode being set iff GLK_MODULE_UNICODE
// is defined) so they will never be called if the Glk implementation
// being used does not support them, but they must at least exist to
// prevent link errors.
#ifdef ZTERP_GLK
#ifndef GLK_MODULE_UNICODE
#define glk_put_char_uni(...)		die("bug %s:%d: glk_put_char_uni() called with no unicode", __FILE__, __LINE__)
#define glk_put_char_stream_uni(...)	die("bug %s:%d: glk_put_char_stream_uni() called with no unicode", __FILE__, __LINE__)
#define glk_request_char_event_uni(...)	die("bug %s:%d: glk_request_char_event_uni() called with no unicode", __FILE__, __LINE__)
#define glk_request_line_event_uni(...)	die("bug %s:%d: glk_request_line_event_uni() called with no unicode", __FILE__, __LINE__)
#endif
#ifndef GLK_MODULE_LINE_ECHO
#define glk_set_echo_line_event(...)	die("bug: %s %d: glk_set_echo_line_event() called with no line echo", __FILE__, __LINE__)
#endif
#endif

GraphicsType graphics_type = kGraphicsTypeBlorb;
bool centeredText = false;

// Flag describing whether the header bit meaning “fixed font” is set.
static bool header_fixed_font;

// This variable stores whether Unicode is supported by the current Glk
// implementation, which determines whether Unicode or Latin-1 Glk
// functions are used. In addition, for both Glk and non-Glk versions,
// this affects the behavior of @check_unicode. In non-Glk versions,
// this is always true.
static bool have_unicode;

static bool cursor_on;

uint16_t letterwidth = 0;
uint16_t letterheight = 0;

V6ScreenMode screenmode = MODE_NORMAL;

std::array<Window, 8> windows;
Window *mainwin = &windows[0], *curwin = &windows[0];
static Window *mousewin = nullptr;

#ifdef ZTERP_GLK
// This represents a line of input from Glk; if the global variable
// “have_unicode” is true, then the “unicode” member is used; otherwise,
// “latin1”.
struct Line {
    union {
        std::array<char, 256> latin1;
        std::array<glui32, 256> unicode;
    };
    glui32 len;
};

Window *upperwin = &windows[1];
static Window statuswin;
//static long upper_window_height = 0;
//static long upper_window_width = 0;
static winid_t errorwin;
#endif

// In all versions but 6, styles and colors are global and stored in
// mainwin. For V6, they’re tracked per window and thus stored in each
// individual window. For convenience, this function returns the “style
// window” for any version.
static Window *style_window()
{
    return zversion == 6 ? curwin : mainwin;
}

// Output stream bits.
enum {
    STREAM_SCREEN = 1,
    STREAM_TRANS  = 2,
    STREAM_MEMORY = 3,
    STREAM_SCRIPT = 4,
};

static std::bitset<5> streams;
static std::unique_ptr<IO> scriptio, transio, perstransio;

class StreamTables {
public:
    void push(uint16_t addr) {
        ZASSERT(m_tables.size() < MAX_STREAM_TABLES, "too many stream tables");
        m_tables.emplace_back(addr);
    }

    void pop() {
        if (!m_tables.empty()) {
            m_tables.pop_back();
        }
    }

    void write(uint16_t c) {
        ZASSERT(!m_tables.empty(), "invalid stream table");
        m_tables.back().write(c);
    }

    size_t size() const {
        return m_tables.size();
    }

    void clear() {
        m_tables.clear();
    }

private:
    static constexpr size_t MAX_STREAM_TABLES = 16;
    class Table {
    public:
        explicit Table(uint16_t addr) : m_addr(addr) {
            user_store_word(m_addr, 0);
        }

        Table(const Table &) = delete;
        Table &operator=(const Table &) = delete;

        ~Table() {
            user_store_word(m_addr, m_idx - 2);
            if (zversion == 6) {
                int wordwidth;
                if (is_game(Game::Shogun) || is_game(Game::Arthur)) {
                    wordwidth = (m_idx - 4) * letterwidth;
                } else if (is_game(Game::Journey)) {
                    wordwidth = (m_idx - 2) * gcellw;
                } else {
                    fprintf(stderr,  "Storing %f as stream 3 width (word width)\n", (m_idx - 1) * gcellw);
                    wordwidth = (m_idx - 1) * gcellw;
                }
                if (wordwidth < 0)
                    wordwidth = 0;
                store_word(0x30, (uint16_t)wordwidth);
            }
        }

        void write(uint8_t c) {
            user_store_byte(m_addr + m_idx++, c);
        }

    private:
        uint16_t m_addr;
        uint16_t m_idx = 2;
    };

    std::list<Table> m_tables;
};

static StreamTables stream_tables;

static int istream = ISTREAM_KEYBOARD;
static std::unique_ptr<IO> istreamio;

struct Input {
    enum class Type { Char, Line } type;

    // ZSCII value of key read for @read_char.
    uint8_t key;

    // Unicode line of chars read for @read.
    std::array<uint16_t, 256> line;
    uint8_t maxlen;
    uint8_t len;
    uint8_t preloaded;

    // Character used to terminate input. If terminating keys are not
    // supported by the Glk implementation being used (or if Glk is not
    // used at all) this will be ZSCII_NEWLINE; or in the case of
    // cancellation, 0.
    uint8_t term;
};

float imagescalex = 1.0, imagescaley = 1.0;
int last_z6_preferred_graphics = 0;

static int a2_graphical_banner_height = 0;
static int shogun_graphical_banner_width_left = 0;
static int shogun_graphical_banner_width_right = 0;

// Convert a 15-bit color to a 24-bit color.
uint32_t screen_convert_color(uint16_t color)
{
    // Map 5-bit color values to 8-bit.
    const uint32_t table[] = {
        0x00, 0x08, 0x10, 0x19, 0x21, 0x29, 0x31, 0x3a,
        0x42, 0x4a, 0x52, 0x5a, 0x63, 0x6b, 0x73, 0x7b,
        0x84, 0x8c, 0x94, 0x9c, 0xa5, 0xad, 0xb5, 0xbd,
        0xc5, 0xce, 0xd6, 0xde, 0xe6, 0xef, 0xf7, 0xff
    };

    return table[(color >>  0) & 0x1f] << 16 |
           table[(color >>  5) & 0x1f] <<  8 |
           table[(color >> 10) & 0x1f] <<  0;
}

// Convert a 24-bit color to a 15-bit color.
uint16_t screen_convert_colour_to_15_bit(glui32 color)
{

    float red = 0.0, green = 0.0, blue = 0.0;
    float k = (float) 0xff / (float)0x1f;

    red = ((color >> 16) & 0xff) / k;
    green = ((color >> 8) & 0xff) / k;
    blue = (color & 0xff) / k;

    uint16_t result = (((int)round(red) & 0x1f) << 10) | (((int)round(green) & 0x1f) << 5) | ((int)round(blue) & 0x1f);
//    fprintf(stderr, "Converting 0x%6x to 0x%x red: %x green: %x blue: %x\n", color, result, (int)red, (int)green, (int)blue);
    return result;
}

#ifdef GLK_MODULE_GARGLKTEXT
static glui32 zcolor_map[] = {
    static_cast<glui32>(zcolor_Current),
    static_cast<glui32>(zcolor_Default),

    0x000000,	// Black
    0xef0000,	// Red
    0x00d600,	// Green
    0xefef00,	// Yellow
    0x006bb5,	// Blue
    0xff00ff,	// Magenta
    0x00efef,	// Cyan
    0xffffff,	// White
    0xb5b5b5,	// Light grey
    0x8c8c8c,	// Medium grey
    0x5a5a5a,	// Dark grey
    0x000000,   // HACK! System foreground
    0xffffff,   // HACK! System background
};

glui32 user_selected_foreground = 0, user_selected_background = 0xffffff;

void update_color(int which, unsigned long color)
{
    if (which < 2 || which > 14) {
        return;
    }

    zcolor_map[which] = (glui32)color;
}

void update_user_defined_colors(void) {
    if (fg_global_idx == 0 || bg_global_idx == 0)
        return;
    user_selected_foreground = zcolor_map[get_global(fg_global_idx)];
    user_selected_background = zcolor_map[get_global(bg_global_idx)];
}


// Provide descriptive aliases for Gargoyle styles.
enum {
    GStyleBoldItalicFixed = style_Note,
    GStyleBoldItalic      = style_Alert,
    GStyleBoldFixed       = style_User1,
    GStyleItalicFixed     = style_User2,
    GStyleBold            = style_Subheader,
    GStyleItalic          = style_Emphasized,
    GStyleFixed           = style_Preformatted,
};

static int gargoyle_style(const Style &style)
{
    if (style.test(STYLE_BOLD) && style.test(STYLE_ITALIC) && style.test(STYLE_FIXED)) {
        return GStyleBoldItalicFixed;
    } else if (style.test(STYLE_BOLD) && style.test(STYLE_ITALIC)) {
        return GStyleBoldItalic;
    } else if (style.test(STYLE_BOLD) && style.test(STYLE_FIXED)) {
        return GStyleBoldFixed;
    } else if (style.test(STYLE_ITALIC) && style.test(STYLE_FIXED)) {
        return GStyleItalicFixed;
    } else if (style.test(STYLE_BOLD)) {
        return GStyleBold;
    } else if (style.test(STYLE_ITALIC)) {
        return GStyleItalic;
    } else if (style.test(STYLE_FIXED)) {
        return GStyleFixed;
    }

    return style_Normal;
}

static glui32 gargoyle_color(const Color &color)
{
    switch (color.mode) {
    case Color::Mode::ANSI:
        return zcolor_map[color.value];
    case Color::Mode::True:
        return screen_convert_color(color.value);
    }

    return zcolor_Current;
}
#endif

#ifdef ZTERP_GLK
// These functions make it so that code elsewhere needn’t check have_unicode before printing.
static void xglk_put_char(uint16_t c)
{
    if (!have_unicode) {
        glk_put_char(unicode_to_latin1[c]);
    } else {
        glk_put_char_uni(c);
    }
}

static void xglk_put_char_stream(strid_t s, uint32_t c)
{
    if (!have_unicode) {
        glk_put_char_stream(s, unicode_to_latin1[c]);
    } else {
        glk_put_char_stream_uni(s, c);
    }
}
#endif

static void set_window_style(const Window *win)
{
#ifdef ZTERP_GLK
    auto style = win->style;

#ifdef GLK_MODULE_GARGLKTEXT
    if (curwin->font == Window::Font::Fixed || header_fixed_font) {
        style.set(STYLE_FIXED);
    }

    if (options.disable_fixed) {
        style.reset(STYLE_FIXED);
    }

    if (is_game(Game::ZorkZero) && win == upperwin && win->id && win->id->type == wintype_TextGrid) {
        if (graphics_type == kGraphicsTypeMacBW || graphics_type == kGraphicsTypeCGA) {
            garglk_set_zcolors(0, zcolor_Current);
        } else if (!(graphics_type == kGraphicsTypeApple2 && screenmode == MODE_HINTS)) {
           garglk_set_zcolors(0xffffff, zcolor_Current);
        }
        return;
    }

    int calculated_style = gargoyle_style(style);

    if (centeredText) {
        if (calculated_style == style_Normal || calculated_style == style_Preformatted) {
            // setting style to centered roman
            fprintf(stderr, "centeredText is on. setting style to centered roman\n");
            calculated_style = style_User2;
        } else {
            //setting style to centered bold
            fprintf(stderr, "centeredText is on. setting style to centered bold\n");
            calculated_style = style_User1;
        }
//    } else {
//        if (calculated_style == style_Normal) {
//            fprintf(stderr, "setting style to non-centered roman\n");
//        } else {
//            fprintf(stderr, "setting style to non-centered bold\n");
//        }
    }

    glk_set_style(calculated_style);


//    if (is_game(Game::Shogun) && win == upperwin && screenmode == MODE_NORMAL) {
//        garglk_set_reversevideo(1);
//        return;
//    }


    garglk_set_reversevideo(style.test(STYLE_REVERSE));

    glui32 foreground = gargoyle_color(win->fg_color);
    glui32 background = gargoyle_color(win->bg_color);
    if (foreground == gfgcol && background == gbgcol) {
        foreground = zcolor_Default;
        background = zcolor_Default;
    }

    garglk_set_zcolors(foreground, background);
#else
    // Yes, there are three ways to indicate that a fixed-width font should be used.
    bool use_fixed_font = style.test(STYLE_FIXED) || curwin->font == Window::Font::Fixed || header_fixed_font;

    // Glk can’t mix other styles with fixed-width, but the upper window
    // is always fixed, so if it is selected, there is no need to
    // explicitly request it here. In addition, the user can disable
    // fixed-width fonts or tell Bocfel to assume that the output font is
    // already fixed (e.g. in an xterm); in either case, there is no need
    // to request a fixed font.
    // This means that another style can also be applied if applicable.
    if (use_fixed_font && !options.disable_fixed && !options.assume_fixed && curwin != upperwin) {
        glk_set_style(style_Preformatted);
        return;
    }

    // According to standard 1.1, if mixed styles aren’t available, the
    // priority is Fixed, Italic, Bold, Reverse.
    if (style.test(STYLE_ITALIC)) {
        glk_set_style(style_Emphasized);
    } else if (style.test(STYLE_BOLD)) {
        glk_set_style(style_Subheader);
//    } else if (style.test(STYLE_REVERSE)) {
//        glk_set_style(style_Alert);
    } else {
        glk_set_style(style_Normal);
    }
#endif
#else
    zterp_os_set_style(win->style, win->fg_color, win->bg_color);
#endif
}

void set_current_style()
{
    set_window_style(curwin);
}

// The following implements a circular buffer to track the state of the
// screen so that recent history can be stored in save files for
// playback on restore.
constexpr size_t HISTORY_SIZE = 2000;

class History {
public:
    struct Entry {
// Suppress a dubious shadow warning (see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55776)
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
        // These values are part of the Bfhs chunk so must remain stable.
        enum class Type {
            Style = 0,
            FGColor = 1,
            BGColor = 2,
            InputStart = 3,
            InputEnd = 4,
            Char = 5,
        } type;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

        union Contents {
            Color color;
            uint8_t style;
            uint32_t c;

            Contents() : c(0) { }
            explicit Contents(Color color_) : color(color_) { }
            explicit Contents(uint8_t style_) : style(style_) { }
            explicit Contents(uint32_t c_) : c(c_) { }
        } contents;

        explicit Entry(Type type_) : type(type_) { }
        Entry(Type type_, const Color &color) : type(type_), contents(color) { }
        Entry(Type type_, uint8_t style) : type(type_), contents(style) { }
        Entry(Type type_, uint32_t c) : type(type_), contents(c) { }

        static Entry style(uint8_t style) {
            return {Type::Style, style};
        }

        static Entry color(Type color_type, const Color &color) {
            return {color_type, color};
        }

        static Entry character(uint32_t c) {
            return {Type::Char, c};
        }
    };

    std::deque<Entry>::size_type size() {
        return m_entries.size();
    }

    void add_style() {
        auto style = mainwin->style;

        if (mainwin->font == Window::Font::Fixed || header_fixed_font) {
            style.set(STYLE_FIXED);
        }

        add(Entry::style(style.to_ulong()));
    }

    void add_fg_color(const Color &color) {
        add(Entry::color(Entry::Type::FGColor, color));
    }

    void add_bg_color(const Color &color) {
        add(Entry::color(Entry::Type::BGColor, color));
    }

    void add_input(const uint16_t *string, size_t len) {
        add(Entry(Entry::Type::InputStart));

        for (size_t i = 0; i < len; i++) {
            add(Entry::character(string[i]));
        }

        add(Entry::character(UNICODE_LINEFEED));
        add(Entry(Entry::Type::InputEnd));
    }

    void add_input_start() {
        add(Entry(Entry::Type::InputStart));
    }

    void add_input_end() {
        add(Entry(Entry::Type::InputEnd));
    }

    void add_char(uint32_t c) {
        add(Entry::character(c));
    }

    const std::deque<Entry> &entries() {
        return m_entries;
    }

private:
    void add(const Entry &entry) {
        while (m_entries.size() >= HISTORY_SIZE) {
            m_entries.pop_front();
        }

        m_entries.push_back(entry);
    }

    std::deque<Entry> m_entries;
};

static History history;

void screen_set_header_bit(bool set)
{
    if (set != header_fixed_font) {
        header_fixed_font = set;
        history.add_style();
        set_current_style();
    }
}


// Full-window size background or in front
// Background during normal play with text windows on top,
// or in front during Zork 0 map or "slideshows"
winid_t current_graphics_buf_win = NULL;

winid_t graphics_win_glk = NULL; // 7
static int graphics_zpos = 0;

static winid_t buffer_win_glk = NULL;
static unsigned buffer_zpos = 0;

static unsigned max_zpos = 0;

//static void set_cursor(uint16_t y, uint16_t x, uint16_t winid);

void transcribe(uint32_t c)
{
    if (streams.test(STREAM_TRANS)) {
        transio->putc(c);
    }

    if (perstransio != nullptr) {
        perstransio->putc(c);
    }
}

#pragma mark Remap windows

bool is_win_covered(Window *win, int zpos) {
    if (win->id == NULL || win->x_size == 0 || win->y_size == 0)
        return false;

    winid_t win1 = win->id;

    for (auto &window : windows) {
        if ((&window)->id != NULL && (&window)->id != win->id && (&window)->zpos >= zpos) {
            winid_t win2 = (&window)->id;
            if (win2->bbox.x0 <= win1->bbox.x0 && win2->bbox.x1 >= win1->bbox.x1 &&
                win2->bbox.y0 <= win1->bbox.y0 && win2->bbox.y1 >= win1->bbox.y1) {
                    fprintf(stderr, "window %d {%ld,%ld,%hu,%hu} is covered by window %d {%ld,%ld,%hu,%hu}\n", win->index, win->x_origin, win->y_origin, win->x_size, win->y_size, window.index, window.x_origin, window.y_origin, window.x_size, window.y_size);
                    return true;
            }
        }
    }

//    if (buffer_win_glk != NULL && buffer_win_glk != win->id && buffer_zpos >= zpos) {
//        winid_t win2 = buffer_win_glk;
//        if (win2->bbox.x0 <= win1->bbox.x0 && win2->bbox.x1 >= win1->bbox.x1 &&
//            win2->bbox.y0 <= win1->bbox.y0 && win2->bbox.y1 >= win1->bbox.y1) {
//            fprintf(stderr, "window is covered by buffer window\n");
//            return true;
//        }
//    }

    return false;
}

int lastx0 = 0, lasty0 = 0, lastx1 = 0, lasty1 = 0, lastpeer = -1;


void v6_sizewin(Window *win) {
    if (win->id == nullptr)
        return;

    if (is_game(Game::Shogun) && screenmode == MODE_CREDITS && win == &windows[7]) {
        return;
    }

    fprintf(stderr, "v6sizewin: resizing window %d of type ", win->index);
    switch(win->id->type) {
        case wintype_Graphics:
            fprintf(stderr, "graphics");
            break;
        case wintype_TextGrid:
            fprintf(stderr, "grid");
            break;
        case wintype_TextBuffer:
            fprintf(stderr, "buffer");
            break;
        case wintype_Blank:
            fprintf(stderr, "blank");
            break;
        case wintype_Pair:
            fprintf(stderr, "pair");
            break;
        case wintype_AllTypes:
            fprintf(stderr, "all");
            break;
        default:
            fprintf(stderr, "unknown (%d)", win->id->type);
    }

    int x0 = win->x_origin <= 1 ? 0 : (int)win->x_origin - 1; //(int)(win->x_origin / letterwidth + 1) * letterwidth;
//    if (is_game(Game::Journey))
//        x0 = (int)(win->x_origin / letterwidth + ((win->x_origin == 1) ? 0 : 1)) * letterwidth;
    int y0 = win->y_origin <= 1 ? 0 : (int)win->y_origin - 1;


//    if (is_game(Game::Journey) && screenmode == MODE_NORMAL && win == &windows[1]) {
//        win->x_origin = 0;
//        win->y_origin = 0;
//        win->x_size = gscreenw;
//        win->y_size = gscreenh;
//        x0 = 0;
//        y0 = 0;
//    }

    int x1 = x0 + win->x_size;
    if (x1 + letterwidth > gscreenw)
        x1 = gscreenw;
    int y1 = y0 + win->y_size;
    if (y1 + letterheight > gscreenh && win->y_size > letterheight)
        y1 = gscreenh;

    lastpeer = win->id->peer;
    lastx0 = x0;
    lastx1 = x1;
    lasty0 = y0;
    lasty1 = y1;

    win_sizewin(win->id->peer, x0, y0, x1, y1);
    win->id->bbox.x0 = x0;
    win->id->bbox.y0 = y0;
    win->id->bbox.x1 = x1;
    win->id->bbox.y1 = y1;

    fprintf(stderr, " to x:%d y:%d width:%d height:%d. ", x0, y0, x1 - x0, y1 - y0);
    if (win->id->type == wintype_TextGrid)
        set_cursor(win->y_cursor, win->x_cursor, win->index);
}

winid_t v6_new_glk_window(glui32 type, glui32 rock) {
    max_zpos++;
    winid_t result = gli_new_window(type, rock);
    switch (type) {
        case wintype_Graphics:
            graphics_zpos = max_zpos;
            break;
        case wintype_TextBuffer:
            buffer_zpos = max_zpos;
            glk_set_echo_line_event(result, 0);
            break;
        case wintype_TextGrid:
            if (is_game(Game::ZorkZero)) {
                win_maketransparent(result->peer);
                glk_request_mouse_event(result);
            }
//            else win_setbgnd(result->peer, user_selected_background);
            break;
        default:
            break;
    }
    return result;
}

void v6_delete_glk_win(winid_t glkwin) {
    if (glkwin == nullptr)
        return;

    if (graphics_win_glk == glkwin) {
        graphics_win_glk = nullptr;
    }
    if (buffer_win_glk == glkwin) {
        buffer_win_glk = nullptr;
    }

    if (current_graphics_buf_win == glkwin) {
        current_graphics_buf_win = nullptr;
    }

    for (auto &window : windows) {
        if (window.id == glkwin) {
            window.id = nullptr;
        }
    }

    gli_delete_window(glkwin);
}


void v6_delete_win(Window *win) {
    if (win == nullptr || win->id == nullptr)
        return;
    v6_delete_glk_win(win->id);
    win->id = nullptr;
    win->zpos = 0;
}

void v6_restore_hacks(void) {
    if (is_game(Game::Journey)) {
        resize_journey_windows_after_restore();
    }
}

void v6_remap_win_to_buffer(Window *win) {

    if (is_game(Game::Shogun) && win == &windows[7]) {
        return;
    }

    if (win->id && win->id->type == wintype_TextBuffer) {
        return;
    }

//    if (buffer_win_glk) {
//        win->id = buffer_win_glk;
//        if(is_win_covered(win, buffer_zpos)) {
//            fprintf(stderr, "Window is covered by another window. Re-creating it in front\n");
//            v6_delete_win(win);
//            win->id = v6_new_glk_window(wintype_TextBuffer, 0);
//        }
//    } else {
        win->id = v6_new_glk_window(wintype_TextBuffer, 0);
//    }

    //    glk_request_char_event_uni(win->id);
    fprintf(stderr, " and creating a new buffer window with peer %d\n", win->id->peer);
    win_setbgnd(win->id->peer, gargoyle_color(win->bg_color));
    v6_sizewin(win);
    glk_stream_set_current(win->id->str);
    buffer_win_glk = win->id;
    win->zpos = buffer_zpos;
    glk_set_echo_line_event(win->id, 0);
}

static void clear_window(Window *window);

void v6_remap_win_to_grid(Window *win) {

    if (win->id) {
        if (win->id->type == wintype_TextGrid)
            return;
    }

    win->id = v6_new_glk_window(wintype_TextGrid, 0);
    fprintf(stderr, " and creating a new grid window with peer %d\n", win->id->peer);
    if (win == upperwin && is_game(Game::Arthur))
        win->style.set(STYLE_REVERSE);

    glk_window_set_background_color(win->id, gargoyle_color(win->bg_color));

//    win_setreverse(win->id->peer, win->style.test(STYLE_REVERSE));
//    win_setzcolor(win->id->peer, gargoyle_color(win->fg_color),  gargoyle_color(win->bg_color));

    win->zpos = max_zpos;

    if (win == curwin) {
        glk_set_window(win->id);
        set_current_style();
    }
    v6_sizewin(win);
}


Window *v6_remap_win_to_graphics(Window *win) {
    if (is_game(Game::Shogun) && win == &windows[7]) {
        return nullptr;
    }
    if (win->id != nullptr) {
        if (win->id->type == wintype_Graphics)
            return win;
    }

    Window *result = win;
    bool recreated = false;

    winid_t target_win;

    target_win = graphics_win_glk;

    if (!target_win) {
        result->id = v6_new_glk_window(wintype_Graphics, 0);
        if (result->id != nullptr) {
            recreated = true;
            fprintf(stderr, " and creating a new graphics window with peer %d\n", win->id->peer);
            fprintf(stderr, "bg: 0x%06x\n", gargoyle_color(win->bg_color));
        }
    }
    else {
        result->id = target_win;
    }

    if (is_win_covered(result, result->zpos)) {
        fprintf(stderr, "Window is covered by another window. Re-creating it in front\n");
        bool charevent = result->id && (result->id->char_request || result->id->char_request_uni);
        if (charevent)
            glk_cancel_char_event(result->id);
        v6_delete_win(result);
        result->id = v6_new_glk_window(wintype_Graphics, 0);
        if (result->id != nullptr)
            recreated = true;
    }
    v6_sizewin(result);

    if (curwin != &windows[2]) {
        glui32 bg = gargoyle_color(result->bg_color);
        if (result->id != nullptr)
            glk_window_set_background_color(result->id, bg);
    }

    if (recreated) {
        glk_window_clear(result->id);
    }
    graphics_win_glk = result->id;
    result->zpos = graphics_zpos;
    return result;
}

void SCENE_SELECT(void) {
    v6_delete_win(mainwin);
    v6_delete_win(&windows[2]);
    mainwin->id = v6_new_glk_window(wintype_TextGrid, 0);
    win_setbgnd(mainwin->id->peer, user_selected_background);
    mainwin->x_origin = shogun_graphical_banner_width_left + letterwidth;
    if (mainwin->x_origin == 0)
        mainwin->x_origin = letterwidth * 2;
    mainwin->x_size = gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right - letterwidth * 2;
    v6_sizewin(mainwin);
    windows[2].id = v6_new_glk_window(wintype_TextGrid, 0);
}

void adjust_image_scale(void) {
    imagescalex = (float)gscreenw / hw_screenwidth;
    imagescaley = imagescalex / pixelwidth;
}

int arthur_text_top_margin = -1;

extern uint8_t global_map_grid_y_idx;

void adjust_shogun_window(void) {
    mainwin->x_cursor = 0;
    mainwin->y_cursor = 0;
    mainwin->x_size = gscreenw;

    if (graphics_type == kGraphicsTypeApple2) {
        screenmode = MODE_NORMAL;
        upperwin->y_size = 2 * letterheight;
        v6_sizewin(upperwin);
        windows[6].y_origin = upperwin->y_size + 1;
        windows[6].y_size = a2_graphical_banner_height;
        v6_sizewin(&windows[6]);
        mainwin->y_origin = windows[6].y_origin + windows[6].y_size;
        mainwin->y_size = gscreenh - mainwin->y_origin;
    } else {
        if (upperwin->id == nullptr)
            upperwin->id = v6_new_glk_window(wintype_TextGrid, 0);
        upperwin->x_origin = shogun_graphical_banner_width_left;
        upperwin->x_size = gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right;
        upperwin->fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        upperwin->bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        upperwin->style.reset(STYLE_REVERSE);
        glk_window_set_background_color(mainwin->id, user_selected_foreground);
        mainwin->fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
        mainwin->bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
        win_setbgnd(mainwin->id->peer, user_selected_background);
        mainwin->x_origin = upperwin->x_origin;
        mainwin->x_size = upperwin->x_size;
        v6_sizewin(upperwin);
    }
    v6_sizewin(mainwin);
    glk_set_window(mainwin->id);
}


void after_SETUP_TEXT_AND_STATUS(void) {
    if (screenmode == MODE_NORMAL) {
        adjust_shogun_window();
        if (graphics_type == kGraphicsTypeApple2) {
            if (upperwin->id == nullptr) {
                upperwin->id = v6_new_glk_window(wintype_TextGrid, 0);
            }
            v6_sizewin(upperwin);
            glk_window_clear(upperwin->id);
        }
    } else if (screenmode == MODE_HINTS) {
        upperwin->y_size = 3 * gcellh + 2 * ggridmarginy;
        v6_delete_win(mainwin);
        v6_remap_win_to_grid(mainwin);
        win_setbgnd(mainwin->id->peer, user_selected_background);
        if (graphics_type == kGraphicsTypeApple2) {
            windows[7].y_origin = upperwin->y_size + 1;
            mainwin->y_origin = windows[7].y_origin + windows[7].y_size;
            mainwin->y_size = gscreenh - mainwin->y_origin;
        } else if (graphics_type != kGraphicsTypeMacBW && graphics_type != kGraphicsTypeCGA) {
            win_maketransparent(upperwin->id->peer);
        } else if (graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeAmiga) {
            upperwin->x_origin = shogun_graphical_banner_width_left;
            upperwin->x_size = gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right;
            mainwin->x_origin = upperwin->x_origin;
            mainwin->x_size = upperwin->x_size;
            v6_sizewin(upperwin);
            v6_sizewin(mainwin);
        }
    }
}

// Shogun
void after_V_HINT(void) {
    screenmode = MODE_NORMAL;
    v6_delete_win(mainwin);
    mainwin->id = v6_new_glk_window(wintype_TextBuffer, 0);
    adjust_shogun_window();
//    if (graphics_type == kGraphicsTypeApple2)
//        win_setbgnd(upperwin->id->peer, 0xffffff);
}


void GOTO_SCENE(void) {
    screenmode = MODE_NORMAL;
    v6_delete_win(mainwin);
    mainwin->id = v6_new_glk_window(wintype_TextBuffer, 0);
    v6_delete_win(&windows[7]);
    adjust_shogun_window();
}

void V_VERSION(void) {
    if (screenmode == MODE_SLIDESHOW) {
        if (windows[7].id != graphics_win_glk)
            v6_delete_win(&windows[7]);
        if (graphics_type == kGraphicsTypeApple2) {
            windows[7].id = v6_new_glk_window(wintype_TextGrid, 0);
            if (a2_graphical_banner_height == 0) {
                int width, height;
                get_image_size(3, &width, &height);
                a2_graphical_banner_height = height * (gscreenw / ((float)width * 2.0));
            }
            windows[7].y_origin = a2_graphical_banner_height + 1;
            windows[7].x_origin = 0;
            windows[7].x_size = gscreenw;
            windows[7].y_size = gscreenh - (2 * a2_graphical_banner_height + 4 * letterheight);
        } else {
            v6_delete_win(upperwin);
            windows[7].id = v6_new_glk_window(wintype_TextBuffer, 0);
            windows[7].y_origin = letterheight * 3;
            windows[7].x_origin = shogun_graphical_banner_width_left + imagescalex;
            windows[7].x_size = gscreenw - shogun_graphical_banner_width_left - shogun_graphical_banner_width_right - letterwidth;
            windows[7].y_size = gscreenh - 7 * letterheight;
        }
        v6_sizewin(&windows[7]);
        windows[7].zpos = max_zpos;
        glk_set_window(windows[7].id);
        centeredText = true;
        set_current_style();
        screenmode = MODE_CREDITS;
    }
}

void after_V_VERSION(void) {
    centeredText = false;
    set_current_style();
}

void V_CREDITS(void) {
    if (!centeredText) {
        centeredText = true;
        set_current_style();
    }
}

void after_V_CREDITS(void) {
    if (centeredText) {
        centeredText = false;
        set_current_style();
    }
}

void V_DEFINE(void) {
    update_user_defined_colors();
    glk_window_set_background_color(graphics_win_glk, user_selected_background);
    glk_window_clear(graphics_win_glk);
    screenmode = MODE_DEFINE;
    if (!is_game(Game::ZorkZero)) {
        v6_delete_win(mainwin);
        v6_delete_win(upperwin);
        if (!windows[2].id)
            windows[2].id = v6_new_glk_window(wintype_TextGrid, 0);
        glk_request_mouse_event(windows[2].id);
        windows[2].style.reset(STYLE_REVERSE);
    }
}

void after_V_DEFINE(void) {
    screenmode = MODE_NORMAL;
    v6_delete_win(&windows[2]);
    v6_remap_win_to_buffer(mainwin);
}

void after_SPLIT_BY_PICTURE(void) {
    if (screenmode == MODE_HINTS) {
        upperwin->y_size = 3 * gcellh + 2 * ggridmarginy;
        v6_sizewin(upperwin);
        v6_delete_win(mainwin);
        v6_remap_win_to_grid(mainwin);
        if (graphics_type == kGraphicsTypeApple2) {
            glk_request_char_event(mainwin->id);
            glk_request_mouse_event(mainwin->id);
            windows[7].y_origin = upperwin->y_size + 1;
            int width, height;
            get_image_size(8, &width, &height);

            float scale = gscreenw / ((float)width * 2.0);
            a2_graphical_banner_height = height * scale;
            windows[7].y_size = a2_graphical_banner_height;
            windows[7].x_size = gscreenw;
            if (windows[7].id != graphics_win_glk) {
                fprintf(stderr, "windows[7].id != graphics_win_glk!\n");
                v6_delete_win(&windows[7]);
            }
            glk_window_set_background_color(graphics_win_glk, user_selected_background);
            glk_window_clear(graphics_win_glk);
            mainwin->y_origin = windows[7].y_origin + windows[7].y_size;
            mainwin->y_size = gscreenh - mainwin->y_origin;
            v6_sizewin(mainwin);
            mainwin->fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
            mainwin->bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
            set_current_style();
            glk_window_clear(mainwin->id);
        }
    } else if (screenmode == MODE_NOGRAPHICS) {
//        v6_delete_win(upperwin);
//        upperwin->id = gli_new_window(wintype_TextGrid, 0);
//        win_setbgnd(upperwin->id->peer, user_selected_foreground);
//        upperwin->style.set(STYLE_REVERSE);
        mainwin->x_size = gscreenw;
        v6_sizewin(mainwin);
    }
}



void V_REFRESH(void) {
    fprintf(stderr, "V-REFRESH\n");
    if (screenmode == MODE_DEFINE) {
        screenmode = MODE_NORMAL;
        v6_delete_win(&windows[2]);
        v6_remap_win_to_buffer(mainwin);
    }
}

int buffer_xpos = 0;

void CENTER(void) {
    V_CREDITS();
    buffer_xpos = 0;
}

void V_MODE(void) {
    if (screenmode != MODE_NOGRAPHICS) {
        screenmode = MODE_NOGRAPHICS;
        glk_window_clear(current_graphics_buf_win);
    } else {
        screenmode = MODE_NORMAL;
    }
}

void MAC_II(void) {
    windows[7].x_size = 640;
}

void after_MAC_II(void) {
    windows[7].x_size = gscreenw;
}




bool flowbreak_after_next_newline = false;
bool pending_flowbreak = false;

void add_char_to_last_line(Window *win, uint16_t c) {
    int idx = win->x_cursor / letterwidth;
    if (idx > 511)
        idx = 0;
    win->lastline[idx] = c;
}

void linebreak(Window *win) {
    int i = 0;
    int idx = win->x_cursor / letterwidth;

    if (idx > 511 || idx == 0)
        return;

    win->lastline[idx + 1] = 0;

    glui32 glkwidth;

    glk_window_get_size(win->id, &glkwidth, nullptr);
    if (glkwidth < idx) {
        idx = glkwidth;
    }


    for (i = idx; i > 0; i--) {
        if (win->lastline[i] == UNICODE_SPACE) {
            break;
        }
    }
    if (i == 0) {
        win->x_cursor = 1;
        win->y_cursor += letterheight;
        return; // Nothing to do
    }
    glui32 ypos = win->y_cursor / letterheight;
    glk_window_move_cursor(win->id, i, ypos);
    for (int j = i - 1; j < glkwidth; j++) {
        xglk_put_char(UNICODE_SPACE);
    }
    glk_window_move_cursor(win->id, 0, ypos + 1);
    idx = 0;
    int endpos = win->x_size / letterwidth + 1;
    for (int j = i + 1; j < endpos; j++) {
        uint16_t c = win->lastline[j];
        if (c == UNICODE_LINEFEED)
            break;
        xglk_put_char(c);
        win->lastline[idx++] = win->lastline[j];
    }

    win->x_cursor = idx * letterwidth;
    win->y_cursor += letterheight;
}

// Print out a character. The character is in “c” and is either Unicode
// or ZSCII; if the former, “unicode” is true. This is meant for any
// output produced by the game, as opposed to output produced by the
// interpreter, which should use Glk (or standard I/O) calls only, to
// avoid interacting with the Z-machine’s streams: interpreter-provided
// output should not be considered part of a transcript, nor should it
// be included in the memory stream.
static void put_char_base(uint16_t c, bool unicode)
{
    if (c == 0) {
        return;
    }

    if (streams.test(STREAM_MEMORY)) {
        // When writing to memory, ZSCII should always be used (§7.5.3).
        if (unicode) {
            c = unicode_to_zscii_q[c];
        }

        stream_tables.write(c);
    } else {
        // For screen and transcription, always prefer Unicode.
        if (!unicode) {
            c = zscii_to_unicode[c];
        }

        if (c != 0) {
            uint8_t zscii = 0;

            // §16 makes no mention of what a newline in font 3 should map to.
            // Other interpreters that implement font 3 assume it stays a
            // newline, and this makes the most sense, so don’t do any
            // translation in that case.
            if (curwin->font == Window::Font::Character && !options.disable_graphics_font && c != UNICODE_LINEFEED) {
#ifdef SPATTERLIGHT
                // Spatterlight uses a real Font 3 and does not need any conversion.
                // We use the BlockQuote style to mark Font 3 because it has no other special use.
                glk_set_style(style_BlockQuote);
#else
                zscii = unicode_to_zscii[c];

                // These four characters have a “built-in” reverse video (see §16).
                if (zscii >= 123 && zscii <= 126) {
                    style_window()->style.flip(STYLE_REVERSE);
                    set_current_style();
                }

                c = zscii_to_font3[zscii];
#endif
            }
#ifdef ZTERP_GLK

            if (is_game(Game::Arthur) && screenmode == MODE_SLIDESHOW && current_graphics_buf_win != nullptr && current_graphics_buf_win != graphics_win_glk) {
                gli_delete_window(current_graphics_buf_win);
                current_graphics_buf_win = nullptr;
            }

            if (curwin->id == nullptr) {
                v6_remap_win_to_grid(curwin);
                if (is_game(Game::ZorkZero) && screenmode != MODE_HINTS && curwin == upperwin) {
                    curwin->x_origin += 3 * letterwidth;
                    curwin->y_origin += letterheight;
                    curwin->x_size += 2 * letterwidth;
//                    win_maketransparent(curwin->id->peer);
                }
                v6_sizewin(curwin);
            }
            if (streams.test(STREAM_SCREEN) && curwin->id != nullptr) {
                if (curwin->id->type == wintype_TextGrid) {
                    // Interpreters seem to have differing ideas about what
                    // happens when the cursor reaches the end of a line in the
                    // upper window. Some wrap, some let it run off the edge (or,
                    // at least, stop the text at the edge). The standard, from
                    // what I can see, says nothing on this issue. Follow Windows
                    // Frotz and don’t wrap.
                    if (c == UNICODE_LINEFEED) {
//                        if (curwin->ypos <= curwin->y_size - gcellh) {
                            // Glk wraps, so printing a newline when the cursor has
                            // already reached the edge of the screen will produce two
                            // newlines.
                            if (curwin->x_cursor <= curwin->x_size - letterwidth) {
                                xglk_put_char(c);
                            }

                            // Even if a newline isn’t explicitly printed here
                            // (because the cursor is at the edge), setting
                            // upperwin->x to 0 will cause the next character to be on
                            // the next line because the text will have wrapped.
                            curwin->x_cursor = 1;
                            curwin->y_cursor += gcellh;
//                        }
                    } else  {
                        glui32 glkwidth;
                        glk_window_get_size(curwin->id, &glkwidth, nullptr);
                        bool off_the_grid = (curwin->x_cursor - 1) / letterwidth >= glkwidth;
                        bool wrapping = (curwin->attribute & 1) == 1;
                        if (off_the_grid) {
                            fprintf(stderr, "Printing character %c (%d) outside text window width\n", c, c);
                            fprintf(stderr, "Window width: %d xpos: %d\n", glkwidth, (curwin->x_cursor - 1) / letterwidth);
                        }
                        if (c == UNICODE_SPACE || !wrapping || !off_the_grid) {
                            curwin->x_cursor += letterwidth;
                            add_char_to_last_line(curwin, c);
                            if (!(off_the_grid && !wrapping))
                                xglk_put_char(c);
                        } else {
                            linebreak(curwin);
                            xglk_put_char(c);
                        }
                    }
                } else {
                    xglk_put_char(c);
                    if (c == UNICODE_LINEFEED) {
                        if (pending_flowbreak) {
                            flowbreak_after_next_newline = true;
                            pending_flowbreak = false;
                        } else if (flowbreak_after_next_newline) {
                            glk_window_flow_break(curwin->id);
                            flowbreak_after_next_newline = false;
                        }
                    } else {
                        flowbreak_after_next_newline = false;
                    }
                }
            }
#else
            if (streams.test(STREAM_SCREEN) && curwin == mainwin) {
#ifdef ZTERP_DOS
                // DOS doesn’t support Unicode, but instead uses code
                // page 437. Special-case non-Glk DOS here, by writing
                // bytes (not UTF-8 characters) from code page 437.
                IO::standard_out().write8(unicode_to_437(c));
#else
                IO::standard_out().putc(c);
#endif
            }
#endif

            if (curwin == mainwin) {
                // Don’t check streams here: for quote boxes (which are in the
                // upper window, and thus not transcribed), both Infocom and
                // Inform games turn off the screen stream and write a duplicate
                // copy of the quote, so it appears in a transcript (if any is
                // occurring). In short, assume that if a game is writing text
                // with the screen stream turned off, it’s doing so with the
                // expectation that it appear in a transcript, which means it also
                // ought to appear in the history.
                history.add_char(c);

                transcribe(c);
            }

            // If the reverse video bit was flipped (for the character font), flip it back.
            if (zscii >= 123 && zscii <= 126) {
                style_window()->style.flip(STYLE_REVERSE);
                set_current_style();
            }
        }
    }
}

static void put_char_u(uint16_t c)
{
    put_char_base(c, true);
}

void put_char(uint8_t c)
{
    fprintf(stderr, "%c", c);
    put_char_base(c, false);
}

// Print a C string (UTF-8) directly to the main window. This is meant
// to print text from the interpreter, not the game. Text will also be
// written to any transcripts which are active, as well as to the
// history buffer. For convenience, carriage returns are ignored under
// the assumption that they are coming from a Windows text stream.
void screen_print(const std::string &s)
{
    auto io = std::make_unique<IO>(std::vector<uint8_t>(s.begin(), s.end()), IO::Mode::ReadOnly);
#ifdef ZTERP_GLK
    strid_t stream = glk_window_get_stream(mainwin->id);
#endif
    for (long c = io->getc(false); c != -1; c = io->getc(false)) {
        if (c != UNICODE_CARRIAGE_RETURN) {
            transcribe((uint32_t)c);
            history.add_char((uint32_t)c);
#ifdef ZTERP_GLK
            xglk_put_char_stream(stream, (uint32_t)c);
#endif
        }
    }
#ifndef ZTERP_GLK
    std::cout << s;
#endif
}

void screen_printf(const char *fmt, ...)
{
    std::va_list ap;
    std::string message;

    va_start(ap, fmt);
    message = vstring(fmt, ap);
    va_end(ap);

    screen_print(message);
}

void screen_puts(const std::string &s)
{
    screen_print(s);
    screen_print("\n");
}

void show_message(const char *fmt, ...)
{
    std::va_list ap;
    std::string message;

    va_start(ap, fmt);
    message = vstring(fmt, ap);
    va_end(ap);

#ifdef ZTERP_GLK
    static glui32 error_lines = 0;

    if (errorwin != nullptr) {
        glui32 w, h;

        // Allow multiple messages to stack, but force at least 5 lines to
        // always be visible in the main window. This is less than perfect
        // because it assumes that each message will be less than the width
        // of the screen, but it’s not a huge deal, really; even if the
        // lines are too long, at least Gargoyle and glktermw are graceful
        // enough.
        glk_window_get_size(mainwin->id, &w, &h);

        if (h > 5) {
            if (zversion != 6) {
                glk_window_set_arrangement(glk_window_get_parent(errorwin), winmethod_Below | winmethod_Fixed, ++error_lines, errorwin);
            } else {
                error_lines++;
                win_sizewin(errorwin->peer, (int)mainwin->x_origin - 1, (int)mainwin->y_origin + mainwin->y_size - error_lines * gbufcellh, (int)mainwin->x_origin + mainwin->x_size - 1, (int)mainwin->y_origin + mainwin->y_size);
            }
        }

        glk_put_char_stream(glk_window_get_stream(errorwin), LATIN1_LINEFEED);
    } else {
        if (zversion != 6) {
            errorwin = glk_window_open(mainwin->id, winmethod_Below | winmethod_Fixed, error_lines = 2, wintype_TextBuffer, 0);
        } else {
            errorwin = gli_new_window(wintype_TextBuffer, 0);
            error_lines = 2;
            win_sizewin(errorwin->peer, (int)mainwin->x_origin - 1, (int)mainwin->y_origin + mainwin->y_size - error_lines * gbufcellh, (int)mainwin->x_origin + mainwin->x_size - 1, (int)mainwin->y_origin + mainwin->y_size);
        }
    }

    // If windows are not supported (e.g. in cheapglk or no Glk), messages
    // will not get displayed. If this is the case, print to the main
    // window.
    strid_t stream;
    if (errorwin != nullptr) {
        stream = glk_window_get_stream(errorwin);
        glk_set_style_stream(stream, style_Alert);
    } else {
        stream = glk_window_get_stream(mainwin->id);
        message = "\n[" + message + "]\n";
    }

    for (size_t i = 0; message[i] != 0; i++) {
        xglk_put_char_stream(stream, char_to_unicode(message[i]));
    }
#else
    {
        std::cout << "\n[" << message << "]\n";
    }
#endif
}

void screen_message_prompt(const std::string &message)
{
    screen_puts(message);
    if (curwin == mainwin) {
        screen_print("\n>");
    }
}

// See §7.
// This returns true if the stream was successfully selected.
// Deselecting a stream is always successful.
bool output_stream(int16_t number, uint16_t table)
{
    ZASSERT(std::labs(number) <= (zversion >= 3 ? 4 : 2), "invalid output stream selected: %ld", static_cast<long>(number));

    if (number == 0) {
        return true;
    } else if (number > 0) {
        streams.set(number);
    } else if (number < 0) {
        if (number != -3 || stream_tables.size() == 1) {
            streams.reset(-number);
        }
    }

    if (number == 2) {
        store_word(0x10, word(0x10) | FLAGS2_TRANSCRIPT);
        if (transio == nullptr) {
            try {
                transio = std::make_unique<IO>(options.transcript_name.get(), options.overwrite_transcript ? IO::Mode::WriteOnly : IO::Mode::Append, IO::Purpose::Transcript);
            } catch (const IO::OpenError &) {
                store_word(0x10, word(0x10) & ~FLAGS2_TRANSCRIPT);
                streams.reset(STREAM_TRANS);
                warning("unable to open the transcript");
            }
        }
    } else if (number == -2) {
        store_word(0x10, word(0x10) & ~FLAGS2_TRANSCRIPT);
    }

    if (number == 3) {
        stream_tables.push(table);
    } else if (number == -3) {
        stream_tables.pop();
    }

    if (number == 4) {
        if (scriptio == nullptr) {
            try {
                scriptio = std::make_unique<IO>(options.record_name.get(), IO::Mode::WriteOnly, IO::Purpose::Input);
            } catch (const IO::OpenError &) {
                streams.reset(STREAM_SCRIPT);
                warning("unable to open the script");
            }
        }
    }
    // XXX V6 has even more handling

    return number < 0 || streams.test(number);
}

void zoutput_stream()
{
    output_stream(as_signed(zargs[0]), zargs[1]);
}

// See §10.
// This returns true if the stream was successfully selected.
bool input_stream(int which)
{
    istream = which;

    if (istream == ISTREAM_KEYBOARD) {
        istreamio.reset();
    } else if (istream == ISTREAM_FILE) {
        if (istreamio == nullptr) {
            try {
                istreamio = std::make_unique<IO>(options.replay_name.get(), IO::Mode::ReadOnly, IO::Purpose::Input);
            } catch (const IO::OpenError &) {
                warning("unable to open the command script");
                istream = ISTREAM_KEYBOARD;
            }
        }
    } else {
        ZASSERT(false, "invalid input stream: %d", istream);
    }

    return istream == which;
}

void zinput_stream()
{
    input_stream(zargs[0]);
}

#pragma mark set_current_window
void set_current_window(Window *window)
{
    curwin = window;

    if (is_game(Game::Arthur)) {
        if (curwin == &windows[2])
            adjust_arthur_windows();

        // Handle Arthur bottom "error window"
        else if (curwin == &windows[3]) {
            if (curwin->id && curwin->id->type != wintype_TextGrid) {
                if (curwin->id != upperwin->id)
                    v6_delete_win(curwin);
            }
            curwin->y_origin = gscreenh - 2 * (letterheight + ggridmarginy);
            curwin->y_size = letterheight + 2 * ggridmarginy;
            if (curwin->id == nullptr) {
                v6_remap_win_to_grid(curwin);
            }
//            curwin->fg_color = Color(Color::Mode::ANSI, get_global((fg_global_idx)));
//            curwin->bg_color = Color(Color::Mode::ANSI, get_global((bg_global_idx)));

            fprintf(stderr, "internal fg:%d internal bg: %d\n", get_global(fg_global_idx), get_global(bg_global_idx));
            curwin->style.flip(STYLE_REVERSE);
            glui32 bgcol = gsfgcol;
            glui32 zfgcol = get_global(fg_global_idx);
            curwin->fg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
            curwin->bg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
            if (zfgcol > 1) {
                bgcol = zcolor_map[zfgcol];
                curwin->fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
                curwin->bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
            }
            glk_window_set_background_color(curwin->id, bgcol);
            mainwin->y_size = curwin->y_origin - mainwin->y_origin;
            v6_sizewin(mainwin);
        }
    } // is_game(Game::Arthur)

//#ifdef ZTERP_GLK
//    if (curwin == upperwin && upperwin->id != nullptr) {
//        upperwin->xpos = upperwin->ypos = 0;
//        glk_window_move_cursor(upperwin->id, 0, 0);
//    }

    if (curwin->id != nullptr) {
        fprintf(stderr, "set_current_window peer %d\n", window->id->peer);
        glk_set_window(curwin->id);
    }
//#endif

    if (curwin->id && (curwin->id->type == wintype_TextGrid || curwin->id->type == wintype_TextBuffer))
        set_current_style();
}

// Find and validate a window. If window is -3 and the story is V6,
// return the current window.
static Window *find_window(uint16_t window)
{
    int16_t w = as_signed(window);

    ZASSERT(zversion == 6 ? w == -3 || (w >= 0 && w < 8) : w == 0 || w == 1, "invalid window selected: %d", w);

    if (w == -3) {
        return curwin;
    }

    return &windows[w];
}

#ifdef ZTERP_GLK
static void perform_upper_window_resize(glui32 new_height)
{
    glui32 actual_height;

    glk_window_set_arrangement(glk_window_get_parent(upperwin->id), winmethod_Above | winmethod_Fixed, new_height, upperwin->id);
    upperwin->y_size = new_height;

    // Glk might resize the window to a smaller height than was requested,
    // so track the actual height, not the requested height.
    glk_window_get_size(upperwin->id, nullptr, &actual_height);
    if (actual_height != upperwin->y_size) {
        // This message probably won’t be seen in a window since the upper
        // window is likely covering everything, but try anyway.
        show_message("Unable to fulfill window size request: wanted %u, got %u", new_height, static_cast<glui32>(actual_height));
        upperwin->y_size = actual_height;
    }
}

// When resizing the upper window, the screen’s contents should not
// change (§8.6.1); however, the way windows are handled with Glk makes
// this slightly impossible. When an Inform game tries to display
// something with “box”, it expands the upper window, displays the quote
// box, and immediately shrinks the window down again. This is a
// problem under Glk because the window immediately disappears. Other
// games, such as Bureaucracy, expect the upper window to shrink as soon
// as it has been requested. Thus the following system is used:
//
// If a request is made to shrink the upper window, it is granted
// immediately if there has been user input since the last window resize
// request. If there has not been user input, the request is delayed
// until after the next user input is read.
static glui32 delayed_window_shrink = -1;
static bool saw_input;

static void update_delayed()
{
    if (delayed_window_shrink != -1 && upperwin->id != nullptr) {
        perform_upper_window_resize(delayed_window_shrink);
        delayed_window_shrink = -1;
    }
}

static void clear_window(Window *window)
{
    if (window->id == nullptr) {
        return;
    }

    int type = window->id->type;
    v6_sizewin(window);

    glk_window_clear(window->id);

    window->x_cursor = window->y_cursor = 1;
    if (type == wintype_TextGrid) {
        glk_window_move_cursor(window->id, 0, 0);
    }
}
#endif

static void resize_upper_window(long nlines, bool from_game)
{
#ifdef ZTERP_GLK
    upperwin->y_size = nlines * gcellh + 2 * ggridmarginy;
    if (upperwin->id == nullptr) {
        return;
    }

#ifdef SPATTERLIGHT
    // Hack to clear upper window when its height is set to 0.
    // This probably doesn't need to be here, but there is currently
    // an incompatibility with Hugo if this is done in the common window code.
    if(nlines == 0)
    {
        glk_window_clear(upperwin->id);
    }
#endif

    long previous_height = upperwin->y_size;

    if (from_game) {
        delayed_window_shrink = (glui32)nlines;
        if (upperwin->y_size <= nlines || saw_input) {
            update_delayed();
        }
#ifdef SPATTERLIGHT
        else if (!is_game(Game::MadBomber) && zversion != 6 && gli_enable_quoteboxes) {
            win_quotebox(upperwin->id->peer, (int)nlines);
            update_delayed();
        }
#endif
        saw_input = false;

        // §8.6.1.1.2
        if (zversion == 3) {
            clear_window(upperwin);
        }
    } else {
        perform_upper_window_resize((glui32)nlines);
    }

    // If the window is being created, or if it’s shrinking and the cursor
    // is no longer inside the window, move the cursor to the origin.
    if (previous_height == 0 || upperwin->y_origin >= nlines) {
        upperwin->x_origin = upperwin->y_origin = 0;
        if (nlines > 0) {
            glk_window_move_cursor(upperwin->id, 0, 0);
        }
    }

    // As in a few other areas, changing the upper window causes reverse
    // video to be deactivated, so reapply the current style.
    if (zversion != 6)
        set_current_style();
#endif
}

void close_upper_window()
{
    // The upper window is never destroyed; rather, when it’s closed, it
    // shrinks to zero height.
    resize_upper_window(0, true);

#ifdef ZTERP_GLK
    delayed_window_shrink = -1;
    saw_input = false;
#endif

    set_current_window(mainwin);
}

//Provide screen size in character cells (as in text grid windows).
void get_screen_size(unsigned int &width, unsigned int &height)
{
//#ifdef ZTERP_GLK
//    glui32 w, h;

    // The main window can be proportional, and if so, its width is not
    // generally useful because games tend to care about width with a
    // fixed font. If a status window is available, or if an upper window
    // is available, use that to calculate the width, because these
    // windows will have a fixed-width font. The height is the combined
    // height of all windows.
//    glk_window_get_size(mainwin->id, &w, &h);
//    height = h;
//    if (statuswin.id != nullptr) {
//        glk_window_get_size(statuswin.id, &w, &h);
//        height += h;
//    }
//    if (upperwin->id != nullptr) {
//        glk_window_get_size(upperwin->id, &w, &h);
//        height += h;
//    }
//    width = w;
//#else
//    std::tie(width, height) = zterp_os_get_screen_size();
//#endif
//
//    // XGlk does not report the size of textbuffer windows, and
//    // zterp_os_get_screen_size() may not be able to get the screen
//    // size, so use reasonable defaults in those cases.
//    if (width == 0) {
//        width = 80;
//    }
//    if (height == 0) {
//        height = 24;
//    }
//
//    // Terrible hack: Because V6 is not properly supported, the window to
//    // which Journey writes its story is completely covered up by window
//    // 1. For the same reason, only the bottom 6 lines of window 1 are
//    // actually useful, even though the game expands it to cover the whole
//    // screen. By pretending that the screen height is only 6, the main
//    // window, where text is actually sent, becomes visible.
//    if (is_game(Game::Journey) && height > 6) {
//        height = 6;
//    }
    fprintf(stderr, "(Creating dummy window to measure width)\n");
    winid_t dummywin = gli_new_window(wintype_TextGrid, 0);
    dummywin->bbox.x0 = 0;
    dummywin->bbox.y0 = 0;
    dummywin->bbox.x1 = gscreenw;
    dummywin->bbox.y1 = gscreenh;
    glui32 actual_width_in_chars;
    glui32 actual_height_in_chars;
    glk_window_get_size(dummywin, &actual_width_in_chars, &actual_height_in_chars);
    gli_delete_window(dummywin);

    fprintf(stderr, "actual width in chars: %d\n", actual_width_in_chars);
    letterwidth = (uint16_t)gcellw;
    letterheight = (uint16_t)gcellh;
    width = actual_width_in_chars;
    height = actual_height_in_chars;
}

#ifdef ZTERP_GLK
#ifdef GLK_MODULE_LINE_TERMINATORS
std::vector<uint32_t> term_keys;
#endif
static bool term_mouse = false;

void term_keys_reset()
{
#ifdef GLK_MODULE_LINE_TERMINATORS
    term_keys.clear();
#endif
    term_mouse = false;
}

static void insert_key(uint32_t key)
{
#ifdef GLK_MODULE_LINE_TERMINATORS
    term_keys.push_back(key);
#endif
}

void term_keys_add(uint8_t key)
{
    switch (key) {
    case ZSCII_UP:    insert_key(keycode_Up); break;
    case ZSCII_DOWN:  insert_key(keycode_Down); break;
    case ZSCII_LEFT:  insert_key(keycode_Left); break;
    case ZSCII_RIGHT: insert_key(keycode_Right); break;
    case ZSCII_F1:    insert_key(keycode_Func1); break;
    case ZSCII_F2:    insert_key(keycode_Func2); break;
    case ZSCII_F3:    insert_key(keycode_Func3); break;
    case ZSCII_F4:    insert_key(keycode_Func4); break;
    case ZSCII_F5:    insert_key(keycode_Func5); break;
    case ZSCII_F6:    insert_key(keycode_Func6); break;
    case ZSCII_F7:    insert_key(keycode_Func7); break;
    case ZSCII_F8:    insert_key(keycode_Func8); break;
    case ZSCII_F9:    insert_key(keycode_Func9); break;
    case ZSCII_F10:   insert_key(keycode_Func10); break;
    case ZSCII_F11:   insert_key(keycode_Func11); break;
    case ZSCII_F12:   insert_key(keycode_Func12); break;

#ifdef SPATTERLIGHT
            // Spatterlight supports keypad 0–9
        case ZSCII_KEY0:  insert_key(keycode_Pad0); break;
        case ZSCII_KEY1:  insert_key(keycode_Pad1); break;
        case ZSCII_KEY2:  insert_key(keycode_Pad2); break;
        case ZSCII_KEY3:  insert_key(keycode_Pad3); break;
        case ZSCII_KEY4:  insert_key(keycode_Pad4); break;
        case ZSCII_KEY5:  insert_key(keycode_Pad5); break;
        case ZSCII_KEY6:  insert_key(keycode_Pad6); break;
        case ZSCII_KEY7:  insert_key(keycode_Pad7); break;
        case ZSCII_KEY8:  insert_key(keycode_Pad8); break;
        case ZSCII_KEY9:  insert_key(keycode_Pad9); break;
#else
    // Keypad 0–9 should be here, but Glk doesn’t support that.
    case ZSCII_KEY0: case ZSCII_KEY1: case ZSCII_KEY2: case ZSCII_KEY3:
    case ZSCII_KEY4: case ZSCII_KEY5: case ZSCII_KEY6: case ZSCII_KEY7:
    case ZSCII_KEY8: case ZSCII_KEY9:
        break;
#endif

    case ZSCII_CLICK_SINGLE:
        term_mouse = true;
        break;

    case ZSCII_CLICK_MENU: case ZSCII_CLICK_DOUBLE:
        term_mouse = true;
        break;

    case 255:
        for (int i = 129; i <= 154; i++) {
            term_keys_add(i);
        }
        for (int i = 252; i <= 254; i++) {
            term_keys_add(i);
        }
        break;

    default:
        break;
    }
}

// Look in the terminating characters table (if any) and reset the
// current state of terminating characters appropriately. Then enable
// terminating characters for Glk line input on the specified window. If
// there are no terminating characters, disable the use of terminating
// characters for Glk input.
static void check_terminators(const Window *window)
{
    if (header.terminating_characters_table != 0) {
        term_keys_reset();

        for (uint32_t addr = header.terminating_characters_table; user_byte(addr) != 0; addr++) {
            term_keys_add(user_byte(addr));
        }

#ifdef GLK_MODULE_LINE_TERMINATORS
        if (glk_gestalt(gestalt_LineTerminators, 0)) {
            glk_set_terminators_line_event(window->id, term_keys.data(), (glui32)term_keys.size());
        }
    } else {
        if (glk_gestalt(gestalt_LineTerminators, 0)) {
            glk_set_terminators_line_event(window->id, nullptr, 0);
        }
#endif
    }
}
#endif

// Decode and print a zcode string at address “addr”. This can be
// called recursively thanks to abbreviations; the initial call should
// have “in_abbr” set to false.
// Each time a character is decoded, it is passed to the function
// “outc”.
static int print_zcode(uint32_t addr, bool in_abbr, void (*outc)(uint8_t))
{
    enum class TenBit { None, Start, Half } tenbit = TenBit::None;
    int abbrev = 0, shift = 0;
    int c, lastc = 0; // initialize to appease g++
    uint16_t w;
    uint32_t counter = addr;
    int current_alphabet = 0;

    do {
        ZASSERT(counter < memory_size - 1, "string runs beyond the end of memory");

        w = word(counter);

        for (int i : {10, 5, 0}) {
            c = (w >> i) & 0x1f;

            if (tenbit == TenBit::Start) {
                lastc = c;
                tenbit = TenBit::Half;
            } else if (tenbit == TenBit::Half) {
                outc((lastc << 5) | c);
                tenbit = TenBit::None;
            } else if (abbrev != 0) {
                uint32_t new_addr;

                new_addr = user_word(header.abbr + 64 * (abbrev - 1) + 2 * c);

                // new_addr is a word address, so multiply by 2
                print_zcode(new_addr * 2, true, outc);

                abbrev = 0;
            } else {
                switch (c) {
                case 0:
                    outc(ZSCII_SPACE);
                    shift = 0;
                    break;
                case 1:
                    if (zversion == 1) {
                        outc(ZSCII_NEWLINE);
                        shift = 0;
                        break;
                    }
                    // fallthrough
                case 2: case 3:
                    if (zversion >= 3 || (zversion == 2 && c == 1)) {
                        ZASSERT(!in_abbr, "abbreviation being used recursively");
                        abbrev = c;
                        shift = 0;
                    } else {
                        shift = c - 1;
                    }
                    break;
                case 4: case 5:
                    if (zversion <= 2) {
                        current_alphabet = (current_alphabet + (c - 3)) % 3;
                        shift = 0;
                    } else {
                        shift = c - 3;
                    }
                    break;
                case 6:
                    if (zversion <= 2) {
                        shift = (current_alphabet + shift) % 3;
                    }

                    if (shift == 2) {
                        shift = 0;
                        tenbit = TenBit::Start;
                        break;
                    }
                    // fallthrough
                default:
                    if (zversion <= 2 && c != 6) {
                        shift = (current_alphabet + shift) % 3;
                    }

                    outc(atable[(26 * shift) + (c - 6)]);
                    shift = 0;
                    break;
                }
            }
        }

        counter += 2;
    } while ((w & 0x8000) == 0);

    return counter - addr;
}

// Prints the string at addr “addr”.
//
// Returns the number of bytes the string took up. “outc” is passed as
// the character-print function to print_zcode(); if it is null,
// put_char is used.
int print_handler(uint32_t addr, void (*outc)(uint8_t))
{
    return print_zcode(addr, false, outc != nullptr ? outc : put_char);
}

void zprint()
{
    fprintf(stderr, "zprint\n");
    pc += print_handler(pc, nullptr);
}

void znew_line()
{
    put_char(ZSCII_NEWLINE);
}

void zprint_ret()
{
    zprint();
    znew_line();
    zrtrue();
}

#pragma mark zerase_window

void zerase_window()
{
    fprintf(stderr, "zerase_window %d\n", as_signed(zargs[0]));
#ifdef ZTERP_GLK

    int win = as_signed(zargs[0]);

    if (is_game(Game::Arthur)) {
        if ((win < 0 || (win > 1 && win != 3)) && !(win == -3 && (curwin == upperwin || curwin == &windows[3]))) {
            clear_image_buffer();
        }
    }

    if (is_game(Game::ZorkZero)) {
        if (win < 0 || win == 7 || win == 1)
            clear_image_buffer();
    }

    if (win == 0 && screenmode == MODE_HINTS ) {
//        clear_window(upperwin);
        if (is_game(Game::ZorkZero) && graphics_type != kGraphicsTypeApple2) {
            v6_delete_win(&windows[7]);
        }
    }

    if (is_game(Game::Shogun) && win == 2) {
        v6_delete_win(&windows[2]);
    }

    switch (as_signed(zargs[0])) {
    case -2:
        // clears the screen without unsplitting it.
        for (auto &window : windows) {
            clear_window(&window);
        }
            for (int i = 2; i < 8; i++) {
                if (windows[i].id != nullptr &&
                    !(is_game(Game::Arthur) && i == 7)) {
                    v6_delete_glk_win(windows[i].id);
                    windows[i].zpos = 0;
                }
            }
        break;
    case -1:
            // unsplits the screen and clears the lot
//            close_upper_window();
        // fallthrough
            clear_image_buffer();
            for (auto &window : windows) {
                clear_window(&window);
            }
//            clear_window(&windows[0]);
//            clear_window(&windows[1]);
            for (int i = 2; i < 8; i++) {
                Window *win = &windows[i];
                win->x_origin = 1;
                win->y_origin = 1;
//                win->x_size = 0;
//                win->y_size = 0;
                if (win->id != nullptr &&
                    !((is_game(Game::Arthur) || is_game(Game::ZorkZero) || is_game(Game::Shogun)) && i == 7)) {
                    v6_delete_glk_win(win->id);
                    win->zpos = 0;
                }
            }

            break;
        // 8.7.3.2.1 says V5+ should have the cursor set to 1, 1 of the
        // erased window; V4 the lower window goes bottom left, the upper
        // to 1, 1. Glk doesn’t give control over the cursor when
        // clearing, and that doesn’t really seem to be an issue; so just
        // call glk_window_clear().
    default:
        clear_window(find_window(zargs[0]));
        break;
    }

    if (is_game(Game::Journey)) {
        if (win == 3)
            return;
        if (mainwin->font == Window::Font::Fixed) {
            fprintf(stderr, "Windows 2 font is fixed!\n");
        }
        mainwin->font = Window::Font::Normal;
    }

    // glk_window_clear() kills reverse video in Gargoyle. Reapply style.
    set_current_style();
#endif
}

/*
 * units_left
 *
 * Return the #screen units from the cursor to the end of the line.
 *
 */
static int units_left(void)
{
    return curwin->x_size - curwin->right - curwin->x_cursor + 1;
} /* units_left */


void zerase_line()
{
#ifdef ZTERP_GLK
    fprintf(stderr, "zerase_line %d\n", zargs[0]);
    if (curwin->id == nullptr) {
        v6_remap_win_to_grid(curwin);
//        set_current_style();
        glk_window_move_cursor(curwin->id, (curwin->x_cursor - 1) / letterwidth, (curwin->y_cursor - 1) / letterheight);
        return;
    }

    uint16_t pixels = zargs[0];

    /* Clipping at the right margin of the current window */
    if (--pixels == 0 || pixels > units_left())
        pixels = units_left();

    uint16_t chars = pixels / letterwidth;
    glui32 width;
    glk_window_get_size(curwin->id, &width, NULL);
//    if (is_game(Game::Journey) && options.int_number == INTERP_AMIGA) {
//        width -= 4;
//        chars = pixels / gcellw;
//    }
    if ((curwin->x_cursor - 1) / gcellw + chars > width) {
        chars = width - ((curwin->x_cursor - 1) / gcellw);
    }

    if (chars < 1)
        chars = 1;

    fprintf(stderr, "Erasing line by printing %d blank spaces at x:%d y:%d. Window charwidth:%d Last character is at pos %d\n", chars, (curwin->x_cursor - 1) / letterwidth, (curwin->y_cursor - 1) / letterheight, width, (curwin->x_cursor - 1) / letterwidth + chars);

    for (long i = 0; i < chars; i++) {
        xglk_put_char(UNICODE_SPACE);
    }

    glk_window_move_cursor(curwin->id, (curwin->x_cursor - 1) / letterwidth, (curwin->y_cursor - 1) / letterheight);
#endif
}

static void adjust_zork0(void) {
    fprintf(stderr, "adjust_zork0\n");
//    float ypos;
//    if (graphics_type == kGraphicsTypeMacBW)
//        ypos = imagescaley * 45.0;
//    else
//        ypos = imagescaley * 34.0;
//    ypos += gcellh;
//    mainwin->y_origin = (int)ypos;
//    mainwin->y_size = gscreenh - mainwin->y_origin - gcellh;
//    mainwin->x_size = gli_screenwidth - mainwin->x_origin * 2;
//    v6_sizewin(mainwin);
    upperwin->y_origin = 4 * imagescaley;
    if (graphics_type == kGraphicsTypeMacBW)
        upperwin->y_origin = 9 * imagescaley;
    v6_sizewin(upperwin);
}

#pragma mark set_cursor

void set_cursor(uint16_t y, uint16_t x, uint16_t winid)
{
#ifdef ZTERP_GLK
    Window *win = find_window(winid);

    // -1 and -2 are V6 only, but at least Zracer passes -1 (or it’s
    // trying to position the cursor to line 65535; unlikely!)
    if (as_signed(y) == -1) {
        cursor_on = false;
        fprintf(stderr, "bocfel: screen: set_cursor(-1), cursor off\n");
        return;
    }

    if (win->id != nullptr && win->id->type == wintype_Graphics && screenmode != MODE_SLIDESHOW && !is_game(Game::Shogun)) {
        if (curwin == upperwin || curwin == &windows[7]) {
            if (win->id && win->id->type == wintype_Graphics && win->id != graphics_win_glk) {
                v6_delete_win(win);
//                win->id = nullptr;
                screenmode = MODE_NORMAL;
                current_graphics_buf_win = graphics_win_glk;
            }
            v6_remap_win_to_grid(win);
            fprintf(stderr, "Trying to move cursor in graphics window. Remapping to grid\n");
//            if (is_game(Game::ZorkZero)) {
//                glk_request_mouse_event(grid_win_glk);
//            }
        } else {
            fprintf(stderr, "Trying to move cursor in graphics window. Remapping to buffer(?)\n");
            v6_remap_win_to_buffer(win);
        }
    }
    if (as_signed(y) == -2) {
        fprintf(stderr, "bocfel: screen: set_cursor(-2), cursor on\n");
        cursor_on = true;
        return;
    }

    fprintf(stderr, "bocfel: screen: set_cursor(y:%d,x:%d)\n", y, x);
    fprintf(stderr, "bocfel: screen: previous pos: (y:%d,x:%d) letterwidth:%d\n", win->y_cursor, win->x_cursor, letterwidth);

    // §8.7.2.3 says 1,1 is the top-left, but at least one program (Paint
    // and Corners) uses @set_cursor 0 0 to go to the top-left; so
    // special-case it.
//    if (y == 0) {
//        y = 1;
//    }

    // This handles 0, but also takes care of working around a bug in Inform’s
//    // “box" statement, which causes “x” to be negative if the box’s text is
//    // wider than the screen.
//    if (as_signed(x) < 1) {
//        x = 1;
//    }

    /* Protect the margins */
    if (y == 0)        /* use cursor line if y-coordinate is 0 */
        y = win->y_cursor;
    if (x == 0)        /* use cursor column if x-coordinate is 0 */
        x = win->x_cursor;
    if ((x <= win->left || x > win->x_size - win->right) && win->x_size != 0) {
        fprintf(stderr, "set_cursor: tried to move x_cursor outside window. win->x_size: %d x:%d changing to %d\n", win->x_size, x, win->left + 1);
        x = win->left + 1;
    }

    win->x_cursor = x;
    win->y_cursor = y;

    glsi32 cellypos = (y - 1) / letterheight;
    float cellxpos = (float)(x - 1) / (float)letterwidth;

    if (!is_game(Game::Journey) && !(curwin->id != nullptr && curwin->id->type == wintype_TextBuffer)) {
        if (cellxpos >= 2) {
            cellxpos--;
            if (is_game(Game::ZorkZero) && curwin == upperwin && screenmode == MODE_NORMAL) {
                if (x > win->x_size / 2) {
                    cellxpos -= 3;
                    switch (graphics_type) {
                        case kGraphicsTypeMacBW:
                            cellxpos -= 1;
                            break;
                        case kGraphicsTypeApple2:
                            cellxpos -= 2;
                            break;
                        default:
                            break;
                    }
                } else if (graphics_type == kGraphicsTypeMacBW) {
                    cellxpos += 1;
                }
            }
        } else if (cellxpos > 0) {
            cellxpos = 1;
        }
    }

    if (is_game(Game::Shogun) && win == upperwin && cellxpos == 1) {
        cellxpos = 0;
    }

    if (win->id != nullptr) {
        if (win->id->type
            == wintype_TextGrid) {
            glk_window_move_cursor(win->id, (glsi32)cellxpos, cellypos);
            fprintf(stderr, "Moving cursor in text grid window to %f, %d\n", cellxpos, cellypos);
        } else {
            fprintf(stderr, "set_cursor: glk window is not text grid, but ");
            if (win->id->type
                == wintype_Graphics) {
                fprintf(stderr, "graphics");
            } else if (win->id->type
                       == wintype_TextBuffer) {
                fprintf(stderr, "buffer");
                if (centeredText && is_game(Game::ZorkZero)) {
                    int spaces_to_print = cellxpos;
                    if (buffer_xpos != 0) {
                        spaces_to_print = cellxpos - buffer_xpos;
                        if (spaces_to_print <= 2) {
                            spaces_to_print = 1;
                        }
                    }

                    for (int i = 0; i < spaces_to_print; i++) {
                        glk_put_char_uni(0x00A0);
                    }
                    buffer_xpos += spaces_to_print + (user_word(0x30) / letterwidth);
                } else {
                    fprintf(stderr, "centeredText is false\n");
                }
            }
            fprintf(stderr, ".\n");
        }
    } else {
        fprintf(stderr, "set_cursor: window %d (%d) has no glk counterpart.\n", win->index, as_signed(winid));
        v6_remap_win_to_grid(win);
        if (&windows[0] != mainwin) {
            fprintf(stderr, "&windows[0] != mainwin\n");
        }
    }
#endif
}

void zset_cursor()
{

//VAR:239 F 4 set_cursor line column
//    6 set_cursor line column window
//    Move cursor in the current window to the position (x,y) (in units) relative to (1,1) in the top left. (In Version 6 the window is supplied and need not be the current one. Also, if the cursor would lie outside the current margin settings, it is moved to the left margin of the current line.)
//    In Version 6, set_cursor -1 turns the cursor off, and either set_cursor -2 or set_cursor -2 0 turn it back on. It is not known what, if anything, this second argument means: in all known cases it is 0.
    uint16_t winid = zargs[2];
    if (znargs < 3)
        winid = -3;
    if (znargs < 3)
        fprintf(stderr, "zset_cursor %d %d\n", as_signed(zargs[0]), zargs[1]);
    else
        fprintf(stderr, "zset_cursor %d %d %d\n", as_signed(zargs[0]), zargs[1], zargs[2]);
    set_cursor(zargs[0], zargs[1], winid);
    fprintf(stderr, "xcursor of curwin: %d\n", curwin->x_cursor);
}

void zget_cursor()
{
    uint16_t y, x;

    y = curwin->y_cursor;
    x = curwin->x_cursor;

    fprintf(stderr, "zget_cursor: result: x: %d y: %d\n", x, y);

    user_store_word(zargs[0] + 0, y);
    user_store_word(zargs[0] + 2, x);
}

static bool prepare_color_opcode(int16_t &fg, int16_t &bg, Window *&win)
{
    // Glk (apart from Gargoyle) has no color support.
#if !defined(ZTERP_GLK) || defined(GLK_MODULE_GARGLKTEXT)
    if (options.disable_color) {
        return false;
    }

    fg = as_signed(zargs[0]);
    bg = as_signed(zargs[1]);
    win = znargs == 3 ? find_window(zargs[2]) : style_window();

    return true;
#else
    return false;
#endif
}

void zset_colour()
{
    fprintf(stderr, "zset_colour: fg %d bg %d win %d\n", zargs[0], zargs[1], zargs[2]);
    int16_t fg, bg;
    Window *win;

    if (prepare_color_opcode(fg, bg, win)) {
        // XXX -1 is a valid color in V6.
//        if (bg == win->fg_color.value && fg == win->bg_color.value && fg != bg) {
//            win->style.flip(STYLE_REVERSE);
//            set_current_style();
//            return;
//        }

        if (is_game(Game::Arthur) && screenmode == MODE_HINTS) {
            fg = get_global((bg_global_idx));
            bg = get_global((fg_global_idx));
        }

        if (is_game(Game::Journey)) {
            if (fg == SPATTERLIGHT_CURRENT_BACKGROUND_COLOUR && bg == SPATTERLIGHT_CURRENT_FOREGROUND_COLOUR) {
                win->style.set(STYLE_REVERSE);
                set_current_style();
                return;
            }
            if (fg == SPATTERLIGHT_CURRENT_FOREGROUND_COLOUR && bg == SPATTERLIGHT_CURRENT_FOREGROUND_COLOUR) {
                win->style.reset(STYLE_REVERSE);
                set_current_style();
                return;
            }
        }

        if (is_game(Game::ZorkZero) || is_game(Game::Shogun)) {
            if (screenmode == MODE_DEFINE && is_game(Game::ZorkZero)) {
                if (fg == 9 && bg == 2) {
                    fg = get_global((bg_global_idx));
                    bg = get_global((fg_global_idx));
                } else {
                    fg = get_global((fg_global_idx));
                    bg = get_global((bg_global_idx));
                }
            } else if (win == mainwin) {
                fg = get_global((fg_global_idx));
                bg = get_global((bg_global_idx));
            } else if (win == upperwin && screenmode == MODE_HINTS && graphics_type == kGraphicsTypeCGA) {
                fg = get_global((bg_global_idx));
                bg = get_global((fg_global_idx));
            }
        }

        if (fg == 1 && bg == 1) {
            if (graphics_type == kGraphicsTypeApple2) {
                fg = 9; bg = 2;
            }
        }
        if (fg >= 1 && fg <= 14) {
            win->fg_color = Color(Color::Mode::ANSI, fg);
        } else {
            fprintf(stderr, "Invalid foreground color %d (%x)\n", fg, fg);
        }
        if (bg >= 1 && bg <= 14) {
            win->bg_color = Color(Color::Mode::ANSI, bg);
        } else {
            if (bg == -1 && zversion == 6) {
                if (is_game(Game::Shogun) && (graphics_type == kGraphicsTypeMacBW || graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeApple2)) {
                    win->fg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
                } else if (!(is_game(Game::ZorkZero) && graphics_type == kGraphicsTypeMacBW)){
                    win->bg_color = Color();
                    if (win->id) {
                        win_maketransparent(win->id->peer);
                    }
                } else if (is_game(Game::ZorkZero) && graphics_type == kGraphicsTypeApple2) {
                    win->fg_color = Color(Color::Mode::ANSI, SPATTERLIGHT_CURRENT_FOREGROUND_COLOUR);
                    win->bg_color = Color(Color::Mode::ANSI, SPATTERLIGHT_CURRENT_BACKGROUND_COLOUR);
                    set_current_style();
                }
                fprintf(stderr, "Tried to make win %d transparent\n", win->index);
            } else {
                fprintf(stderr, "Invalid background color %d (%x)\n", bg, bg);
            }
        }
        if (win == mainwin) {
            history.add_fg_color(win->fg_color);
            history.add_bg_color(win->bg_color);
        }

        set_current_style();
    }
}

void zset_true_colour()
{
    fprintf(stderr, "zset_true_colour");
    int16_t fg, bg;
    Window *win;

    if (prepare_color_opcode(fg, bg, win)) {
        if (fg >= 0) {
            win->fg_color = Color(Color::Mode::True, fg);
        } else if (fg == -1) {
            win->fg_color = Color();
        }

        if (bg >= 0) {
            win->bg_color = Color(Color::Mode::True, bg);
        } else if (bg == -1) {
            win->bg_color = Color();
        }

        if (win == mainwin) {
            history.add_fg_color(win->fg_color);
            history.add_bg_color(win->bg_color);
        }

        set_current_style();
    }
}

#pragma mark zmove_window, creating a new window

// In effect create new window
void zmove_window()
{
    Window *win = find_window(zargs[0]);

    fprintf(stderr, "zmove_window win %d y %d x %d (y_size %d x_size %d)\n", zargs[0], zargs[1], zargs[2], win->y_size, win->x_size);

    int16_t y = zargs[1];
    int16_t x = zargs[2];
    win->y_origin = y;
    win->x_origin = x;
    if (win->y_size == 0)
        win->y_size = 1;
    if (win->x_size == 0)
        win->x_size = 1;
//    if (is_game(Game::Shogun) && screenmode == MODE_HINTS && win == mainwin) {
//        win->y_origin += letterheight;
////        win->y_size = gscreenh - win->y_origin;
//    }
//    int width = win->x_size;
//    int height = win->y_size;

//    if (win == curwin)
//        update_cursor();

//    glui32 text_color, background_color;
//    if (win->id && glk_style_measure(win->id, style_Normal, stylehint_TextColor, &text_color) && text_color != zcolor_Default && text_color != zcolor_Current) {
//        win->fg_color = Color(Color::Mode::True, screen_convert_colour_to_15_bit(text_color));
//    }
//    if (win->id && glk_style_measure(win->id, style_Normal, stylehint_BackColor, &background_color) && background_color != zcolor_Default && background_color != zcolor_Current) {
//        win->bg_color = Color(Color::Mode::True, screen_convert_colour_to_15_bit(background_color));
//    }

//    win->fg_color = Color(Color::Mode::True, screen_convert_colour_to_15_bit(user_selected_foreground));
//    win->bg_color = Color(Color::Mode::True, screen_convert_colour_to_15_bit(user_selected_background));

    if (win->id != nullptr)
        v6_sizewin(win);
}

/*
 * reset_cursor
 *
 * Reset the cursor of a given window to its initial position.
 *
 */
static void reset_cursor(uint16_t win)
{
    fprintf(stderr, "reset_cursor: win[%d] y_cursor was %d is %d\n", win, windows[win].y_cursor , (int)gcellh);
    fprintf(stderr, "reset_cursor: win[%d] x_cursor was %d is %d\n", win, windows[win].x_cursor , windows[win].left + 1);
    windows[win].y_cursor = 1;
    windows[win].x_cursor = windows[win].left + 1;

} /* reset_cursor */

#pragma mark zwindow_size

// Change size of current window in pixels
void zwindow_size()
{
    Window *win = find_window(zargs[0]);

    fprintf(stderr, "zwindow_size win %d height %d width %d\n", zargs[0], zargs[1], zargs[2]);

    int16_t height = zargs[1];
    int16_t width = zargs[2];

//    if (width % letterwidth == 1)
//        width = float((width - 1) / letterwidth + 1) * gcellw;
//    else
//        fprintf(stderr, "width (%d) %% letterwidth (%d) == %d\n", width, letterwidth, width % letterwidth);

    win->y_size = height;
    win->x_size = width;


    /* Keep the cursor within the window */

    if (win->y_cursor > zargs[1] || win->x_cursor > zargs[2])
        reset_cursor(zargs[0]);

    if (is_win_covered(win, win->zpos)) {
        fprintf(stderr, "Window is completely covered by another window. Deleting it and re-creating it in front\n");
        int type = win->id->type;
        if (graphics_win_glk != win->id)
            v6_delete_glk_win(win->id);
        win->id = nullptr;
        if (win == upperwin) {
            v6_remap_win_to_grid(win);
//            set_current_style();
        } else {
            win->id = v6_new_glk_window(type, 0);
            win->zpos = max_zpos;
//            set_current_style();
        }
    }

//    if (is_game(Game::Shogun) && screenmode == MODE_HINTS && win == upperwin && graphics_type == kGraphicsTypeMacBW) {
    if (screenmode == MODE_HINTS && win == upperwin) {
        upperwin->y_size = 3 * gcellh + 2 * ggridmarginy;
    }

    if (zargs[0] == 2 && screenmode == MODE_DEFINE) {
        win->x_size = 35 * gcellw + 1 +  2 * ggridmarginx;
    }

    if (zargs[0] == 2 && is_game(Game::Shogun)) {
        win->x_size = (win->x_size / letterwidth) * gcellw + 2 * ggridmarginx;
    }

    v6_sizewin(win);
}

void zwindow_style()
{
    fprintf(stderr, "zwindow_style window %d flags 0x%x operation %d\n", zargs[0], zargs[1], zargs[2]);

    Window *win = find_window(zargs[0]);
    uint16_t flags = zargs[1];

    /* Supply default arguments */

    if (znargs < 3)
        zargs[2] = 0;

    /* Set window style */

    switch (zargs[2]) {
        case 0:
            win->attribute = flags;
            break;
        case 1:
            win->attribute |= flags;
            break;
        case 2:
            win->attribute &= ~flags;
            break;
        case 3:
            win->attribute ^= flags;
            break;
    }

    if (curwin == win)
        set_current_style();
}

// V6 has per-window styles, but all others have a global style; in this
// case, track styles via the main window.
//Sets the text style to: Roman (if 0), Reverse Video (if 1), Bold (if 2), Italic (4), Fixed Pitch (8)
void zset_text_style()
{
    fprintf(stderr, "zset_text_style %d (", zargs[0]);
    // A style of 0 means all others go off.

    if (is_game(Game::Journey) && zargs[0] == get_global(0x6c)) {
        int xpos = windows[1].x_cursor / letterwidth;
        int ypos = windows[1].y_cursor / letterheight;
        fprintf(stderr, "Setting style to bold when cursor is at x:%d y:%d\n", xpos, ypos);
        selected_journey_line = ypos - get_global(0x0e) + 2;
        fprintf(stderr, "line: %d\n", selected_journey_line);
        selected_journey_column = 0;
        if (xpos == get_global(0x28) - 1)
            selected_journey_column = 1;
        fprintf(stderr, "CHR-COMMAND-COLUMN is global 0x28 (0x%x) (%d)\n", get_global(0x28), get_global(0x28));
        fprintf(stderr, "COMMAND-OBJECT-COLUMN is global 0xb0 (0x%x) (%d)\n", get_global(0xb0), get_global(0xb0));
        fprintf(stderr, "COMMAND-WIDTH is global 0xb8 (0x%x) (%d)\n", get_global(0xb8), get_global(0xb8));
        if (xpos == get_global(0x28) + get_global(0xb8) - 1)
            selected_journey_column = 2;
        if (xpos == get_global(0xb0) + get_global(0xb8) - 1)
            selected_journey_column = 3;
        fprintf(stderr, "column: %d\n", selected_journey_column);
    }


    switch (zargs[0]) {
        case 0: fprintf(stderr, "Roman");
            break;
        case 1: fprintf(stderr, "Reverse");
            break;
        case 2: fprintf(stderr, "Bold");
            break;
        case 3: fprintf(stderr, "Italic");
            break;
        case 8: fprintf(stderr, "Fixed pitch");
            break;
        default:
            fprintf(stderr, "Unknown/Bug");
            break;
    }
    fprintf(stderr, ")\n");


    if (zargs[0] == 0) {
        curwin->style.reset();
    } else if (zargs[0] < 16) {
        curwin->style |= zargs[0];
    }

    if (curwin == mainwin) {
        history.add_style();
    }

//    if (is_game(Game::ZorkZero) && screenmode == MODE_HINTS && zargs[0] < 2) {
    if (zargs[0] < 2) {
        garglk_set_reversevideo(zargs[0]);
    }

    set_current_style();
}

static bool is_valid_font(Window::Font font)
{
    return font == Window::Font::Normal ||
          (font == Window::Font::Character && !options.disable_graphics_font) ||
          (font == Window::Font::Fixed     && !options.disable_fixed);
}

void zset_font()
{
    if (znargs == 1)
        fprintf(stderr, "zset_font %d\n", zargs[0]);
    else if (znargs == 2)
        fprintf(stderr, "zset_font %d window %d\n", zargs[0], zargs[1]);
    Window *win = curwin;

    if (zversion == 6 && znargs == 2 && as_signed(zargs[1]) != -3) {
        ZASSERT(zargs[1] < 8, "invalid window selected: %d", as_signed(zargs[1]));
        win = &windows[zargs[1]];
    }

    if (static_cast<Window::Font>(zargs[0]) == Window::Font::Query) {
        store(static_cast<uint16_t>(win->font));
    } else if (is_valid_font(static_cast<Window::Font>(zargs[0]))) {
        store(static_cast<uint16_t>(win->font));
        win->font = static_cast<Window::Font>(zargs[0]);
        set_current_style();
        if (win == mainwin) {
            history.add_style();
        }
    } else {
        store(0);
    }
}

void zprint_table()
{
    uint16_t text = zargs[0], width = zargs[1], height = zargs[2], skip = zargs[3];
    fprintf(stderr, "zprint_table %d width %d height %d skip %d\n", text, width, height, skip);
    uint16_t n = 0;

#ifdef ZTERP_GLK
    uint16_t start = 0; // initialize to appease g++

//    if (is_game(Game::Shogun) && curwin->id != nullptr && curwin->id->type == wintype_TextGrid && is_win_covered(curwin, curwin->zpos)) {
//        v6_delete_glk_win(curwin->id);
////        curwin->x_size += 2 * letterwidth;
//    }
//
//    if (is_game(Game::Shogun) && curwin == &windows[7]) {
//        curwin = mainwin;
//    }

    if (curwin->id == NULL || (curwin == upperwin && curwin->id->type != wintype_TextGrid)) {
        v6_remap_win_to_grid(curwin);
        set_current_style();
    }

    if (curwin->id->type == wintype_TextGrid) {
        start = upperwin->x_cursor + 1;
    }
#endif

    if (znargs < 3) {
        height = 1;
    }
    if (znargs < 4) {
        skip = 0;
    }

    for (uint16_t i = 0; i < height; i++) {
        for (uint16_t j = 0; j < width; j++) {
            put_char(user_byte(text + n++));
        }

        if (i + 1 != height) {
            n += skip;
#ifdef ZTERP_GLK
            if (curwin->id->type == wintype_TextGrid) {
                set_cursor(upperwin->y_cursor + 2, start, -3);
            } else
#endif
            {
                put_char(ZSCII_NEWLINE);
            }
        }
    }
}

void zprint_char()
{
    fprintf(stderr, "zprint_char %d\n", zargs[0]);

    // Check 32 (space) first: a cursory examination of story files
    // indicates that this is the most common value passed to @print_char.
    // This appears to be due to V4+ games blanking the upper window.
#define valid_zscii_output(c)	((c) == 32 || (c) == 0 || (c) == 9 || (c) == 11 || (c) == 13 || ((c) > 32 && (c) <= 126) || ((c) >= 155 && (c) <= 251))
    ZASSERT(valid_zscii_output(zargs[0]), "@print_char called with invalid character: %u", static_cast<unsigned int>(zargs[0]));
#undef valid_zscii_output

    put_char(zargs[0]);
}

void zprint_num()
{
    fprintf(stderr, "zprint_num\n");
    std::ostringstream ss;

    ss << as_signed(zargs[0]);
    for (const auto &c : ss.str()) {
        put_char(c);
    }
}

void zprint_addr()
{
    fprintf(stderr, "zprint_addr\n");
    print_handler(zargs[0], nullptr);
}

void debug_print_str(uint8_t c) {
    fprintf(stderr, "%c", c);
}

static int string_length_counter = 0;

void count_print_str(uint8_t c) {
    string_length_counter++;
}

int count_characters_in_zstring(uint16_t str) {
    string_length_counter = 0;
    print_zcode(unpack_string(str), false, count_print_str);
    return string_length_counter;
}

void zprint_paddr()
{
//    fprintf(stderr, "zprint_paddr 0x%x (%x)\n", unpack_string(zargs[0]), zargs[0]);
//    fprintf(stderr, "\"");
//    print_zcode(unpack_string(zargs[0]), false, debug_print_str);
//    fprintf(stderr, "\"\n");
    print_handler(unpack_string(zargs[0]), nullptr);
}

void zsplit_window()
{

    Window *upper = &windows[1];
    Window *lower = &windows[0];
    uint16_t height = zargs[0];

    fprintf(stderr, "\nzsplit_window %d: before: status window (win 1) height: %d ypos: %ld main window (win 0) height: %d ypos:%ld\n", zargs[0], upper->y_size, upper->y_origin, lower->y_size, lower->y_origin);

    /* Cursor of upper window mustn't be swallowed by the lower window */
    upper->y_cursor += upper->y_origin - 1;

    upper->y_origin = 1;
    upper->y_size = height;

    if ((short)upper->y_cursor > (short)upper->y_size)
        reset_cursor(1);
    /* Cursor of lower window mustn't be swallowed by the upper window */
    lower->y_cursor += lower->y_origin - 1 - height;

    lower->y_origin = 1 + height;
    lower->y_size = gscreenh - height;
    if ((short)lower->y_cursor < 1)
        reset_cursor(0);

    // Called by Arthur INIT-HINT-SCREEN function
    if (zversion == 6 && screenmode == MODE_HINTS) {
        glk_window_clear(graphics_win_glk);
        v6_delete_win(lower);
        v6_remap_win_to_grid(lower);
        upperwin->y_size = 3 * gcellh + 2 * ggridmarginy;
        v6_sizewin(upper);
        return;
    }


    if (height == 0) {
        if (upper->id != nullptr)
            v6_delete_glk_win(upper->id);
    } else {
        if (upper->id == nullptr && screenmode != MODE_CREDITS) {
            v6_remap_win_to_grid(upper);
        }
        v6_sizewin(upper);
    }
    if (lower->id == nullptr) {
        lower->id = v6_new_glk_window(wintype_TextBuffer, 0);
        lower->zpos = max_zpos;
        glk_set_echo_line_event(lower->id, 0);
    }
    v6_sizewin(lower);

    fprintf(stderr, "z_split_window %d: result: status window (win 1) height: %d ypos: %ld main window (win 0) height: %d ypos:%ld\n", zargs[0], upper->y_size, upper->y_origin, lower->y_size, lower->y_origin);
}

void zset_window()
{
    fprintf(stderr, "zset_window %d\n", zargs[0]);
    set_current_window(find_window(zargs[0]));
}

#ifdef ZTERP_GLK

double perceived_brightness(glui32 col) {
    double red = (col >> 16) / 255.0;
    double green = ((col >> 8) & 0xff)  / 255.0;
    double blue = (col & 0xff) / 255.0;

    double r = (red <= 0.04045) ? red / 12.92 : pow((red + 0.055) / 1.055, 2.4);
    double g = (green <= 0.04045) ? green / 12.92 : pow((green + 0.055) / 1.055, 2.4);
    double b = (blue <= 0.04045) ? blue / 12.92 : pow((blue + 0.055) / 1.055, 2.4);

    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

glui32 darkest(glui32 col1, glui32 col2) {
    if (perceived_brightness(col1) >= perceived_brightness(col2))
        return col2;
    else
        return col1;
}

glui32 brightest(glui32 col1, glui32 col2) {
    if (perceived_brightness(col1) < perceived_brightness(col2))
        return col2;
    else
        return col1;
}

static void flush_image_buffer(void);

//extern uint32_t update_status_bar_address;
//extern uint32_t update_picture_address;
//extern uint32_t update_inventory_address;
//extern uint32_t update_stats_address;
//extern uint32_t update_map_address;
//extern uint8_t update_global_idx;


#pragma mark window change on resize events

int lastwidth = 0, lastheight = 0, lastbg = -1;
float lastcellw = 0, lastcellh = 0;
glui32 latest_picture = 0;

void window_change(void)
{
    // When a textgrid (the upper window) in Gargoyle is rearranged, it
    // forgets about reverse video settings, so reapply any styles to the
    // current window (it doesn’t hurt if the window is a textbuffer). If
    // the current window is not the upper window that’s OK, because
    // set_current_style() is called when a @set_window is requested.
    set_current_style();

    bool no_size_change = (gscreenw == lastwidth && gscreenh == lastheight && lastcellw == gcellw && lastcellh == gcellh);

    lastwidth = gscreenw;
    lastheight = gscreenh;
    lastcellw = gcellw;
    lastcellh = gcellh;

    // Track the new size of the upper window.
    if (zversion >= 3 && upperwin->id != nullptr && zversion != 6 && !no_size_change) {
        glui32 w, h;

        glk_window_get_size(upperwin->id, &w, &h);

        upperwin->x_size = w * letterwidth + 2 * ggridmarginx;

        resize_upper_window(h, false);
    }

    // §8.4
    // Only 0x20 and 0x21 are mentioned; what of 0x22 and 0x24? Zoom and
    // Windows Frotz both update the V5 header entries, so do that here,
    // too.
    //
    // Also, no version restrictions are given, but assume V4+ per §11.1.
    if (zversion >= 4) {

        letterwidth = gcellw;
        letterheight = gcellh;
        update_header_with_new_size();

        if (last_z6_preferred_graphics != gli_z6_graphics) {
            last_z6_preferred_graphics = gli_z6_graphics;
            no_size_change = false;
            // We may have switched preferred graphics to the one we already fell back on
            if (graphics_type != gli_z6_graphics) {
                free_images();
                size_t dummy_file_length;
                image_count = 0;

                bool found = false;
                bool was_apple = (graphics_type == kGraphicsTypeApple2);
                if (gli_z6_graphics == kGraphicsTypeApple2) {
                    size_t dummy_file_length;
                    image_count = 0;
                    uint8_t *dummy = extract_apple_2((const char *)game_file.c_str(), &dummy_file_length, &raw_images, &image_count, &pixversion);
                    if (image_count > 0) {
                        found = true;
                        graphics_type = kGraphicsTypeApple2;
                        options.int_number = INTERP_APPLE_IIE;
                        store_byte(0x1e, options.int_number);
                        hw_screenwidth = 140;
                        pixelwidth = 2.0;
                    }
                    if (dummy != nullptr)
                        free(dummy);
                }

                if (!found) {
                    find_graphics_files();
                    load_best_graphics();
                }

                // If we were using Apple 2 graphics and found no other kind, fall back to Apple 2 graphics again
                if (graphics_type == kGraphicsTypeNoGraphics && was_apple) {
                    uint8_t *dummy = extract_apple_2((const char *)game_file.c_str(), &dummy_file_length, &raw_images, &image_count, &pixversion);
                    if (dummy != nullptr) {
                        graphics_type = kGraphicsTypeApple2;
                        free(dummy);
                    }
                }

                if (is_game(Game::Arthur))
                    set_global(global_map_grid_y_idx, 0);
            }
        }

        if (gli_z6_colorize && 
            (graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeMacBW)) {
            monochrome_black = darkest(gfgcol, gbgcol);
            monochrome_white = brightest(gfgcol, gbgcol);
        } else {
            monochrome_black = 0;
            monochrome_white = 0xffffff;
        }

        if (user_selected_foreground == zcolor_map[13])
            user_selected_foreground = gfgcol;
        if (user_selected_background == zcolor_map[14])
            user_selected_background = gbgcol;

        update_color(SPATTERLIGHT_CURRENT_FOREGROUND_COLOUR, gfgcol);
        update_color(SPATTERLIGHT_CURRENT_BACKGROUND_COLOUR, gbgcol);

        adjust_image_scale();

        if (current_graphics_buf_win) {
            if (!no_size_change)
                win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
            if (user_selected_background != lastbg)
                glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
        }
        if (mainwin->id && !(mainwin->id->peer == lastpeer && user_selected_background == lastbg)) {
            win_setbgnd(mainwin->id->peer, user_selected_background);
        }
        lastbg = user_selected_background;

        if (is_game(Game::Arthur)) {
            arthur_window_adjustments();
        } else if (is_game(Game::Journey)) {
            update_journey_on_resize();
        }

    } else {
        zshow_status();
    }
}
#endif

#ifdef ZTERP_GLK
static bool timer_running;

static void start_timer(uint16_t n)
{
    fprintf(stderr, "start_timer %d\n", n);
    if (!timer_available()) {
        return;
    }

    if (timer_running) {
        die("nested timers unsupported");
    }
    glk_request_timer_events(n * 100);
    timer_running = true;
}

static void stop_timer()
{
    if (!timer_available()) {
        return;
    }

    glk_request_timer_events(0);
    timer_running = false;
}

static void request_char()
{
    if (have_unicode) {
        glk_request_char_event_uni(curwin->id);
    } else {
        glk_request_char_event(curwin->id);
    }
    glk_request_mouse_event(curwin->id);
}

static void request_line(Line &line, glui32 maxlen)
{
    check_terminators(curwin);

    if (have_unicode) {
        glk_request_line_event_uni(curwin->id, line.unicode.data(), maxlen, line.len);
    } else {
        glk_request_line_event(curwin->id, line.latin1.data(), maxlen, line.len);
    }
}

// If an interrupt is called, cancel any pending read events. They will
// be restarted after the interrupt returns. If this was a line input
// event, “line” will be updated with the length of the input that had
// been entered at the time of cancellation.
static void cancel_read_events(const Window *window, Line &line)
{
    event_t ev;

    glk_cancel_char_event(window->id);
    glk_cancel_line_event(window->id, &ev);
    if (mousewin && mousewin->id != nullptr) {
        glk_cancel_mouse_event(mousewin->id);
    } else {
        glk_cancel_mouse_event(window->id);
    }

    // If the pending read was a line input, set the line length to the
    // amount read before cancellation (this will already have been
    // stored in the line struct by Glk).
    if (ev.type == evtype_LineInput) {
        line.len = ev.val1;
    }
}

// This is a wrapper around internal_call() which handles screen-related
// issues: input events are canceled (in case the interrupt routine
// calls input functions) and the window is restored if the interrupt
// changes it. The value from the internal call is returned. This does
// not restart the canceled read events.
static uint16_t handle_interrupt(uint16_t addr, Line *line)
{
    Window *saved = curwin;
    uint16_t ret;

    if (line != nullptr) {
        cancel_read_events(curwin, *line);
    }

    ret = internal_call(addr);

    // It’s possible for an interrupt to switch windows; if it does,
    // simply switch back. This is the easiest way to deal with an
    // undefined bit of the Z-machine.
    if (curwin != saved) {
        set_current_window(saved);
    }

    return ret;
}

// All read events are canceled during an interrupt, but after an
// interrupt returns, it’s possible that the read should be restarted,
// so this function does that.
static void restart_read_events(Line &line, const Input &input, bool enable_mouse)
{
    switch (input.type) {
    case Input::Type::Char:
        request_char();
        break;
    case Input::Type::Line:
        request_line(line, input.maxlen);
        break;
    }

    if (enable_mouse) {
        if (mousewin && mousewin->id) {
            glk_request_mouse_event(mousewin->id);
        } else {
            glk_request_mouse_event(curwin->id);
            glk_request_mouse_event(upperwin->id);
            if (is_game(Game::ZorkZero) && screenmode == MODE_MAP) {
                glk_request_mouse_event(current_graphics_buf_win);
            }
        }
    }
}
#endif

#ifdef ZTERP_GLK
void screen_flush()
{
    event_t ev;

    glk_select_poll(&ev);
    switch (ev.type) {
    case evtype_None:
        break;
    case evtype_Arrange:
        window_change();
        break;
#ifdef GLK_MODULE_SOUND2
    case evtype_SoundNotify: {
        sound_stopped(ev.val2);
        uint16_t sound_routine = sound_get_routine(ev.val2);
        if (sound_routine != 0) {
            handle_interrupt(sound_routine, nullptr);
        }
        break;
    }
#endif
    default:
        // No other events should arrive. Timers are only started in
        // get_input() and are stopped before that function returns.
        // Input events will not happen with glk_select_poll(), and no
        // other event type is expected to be raised.
        break;
    }
}
#else
void screen_flush()
{
}
#endif

template <typename T>
static bool special_zscii(T c)
{
    return c > 129 && c < 154;
}

// This is called when input stream 1 (read from file) is selected. If
// it succefully reads a character/line from the file, it fills the
// struct at “input” with the appropriate information and returns true.
// If it fails to read (likely due to EOF) then it sets the input stream
// back to the keyboard and returns false.
static bool istream_read_from_file(Input &input)
{
    if (input.type == Input::Type::Char) {
        long c;

        // If there are carriage returns in the input, this is almost
        // certainly a command script from a Windows system being run on a
        // non-Windows system, so ignore them.
        do {
            c = istreamio->getc(true);
        } while (c == UNICODE_CARRIAGE_RETURN);

        if (c == -1) {
            input_stream(ISTREAM_KEYBOARD);
            return false;
        }

        // Don’t translate special ZSCII characters (cursor keys, function keys, keypad).
        if (special_zscii(c)) {
            input.key = c;
        } else {
            input.key = unicode_to_zscii_q[c];
        }
    } else {
        std::vector<uint16_t> line;

        try {
            line = istreamio->readline();
        } catch (const IO::EndOfFile &) {
            input_stream(ISTREAM_KEYBOARD);
            return false;
        }

        // As above, ignore carriage returns.
        if (!line.empty() && line.back() == UNICODE_CARRIAGE_RETURN) {
            line.pop_back();
        }

        if (line.size() > input.maxlen) {
            line.resize(input.maxlen);
        }

        input.len = line.size();

#ifdef ZTERP_GLK
        if (curwin->id != nullptr) {
            glk_set_style(style_Input);
            for (const auto &c : line) {
                xglk_put_char(c);
            }
            xglk_put_char(UNICODE_LINEFEED);
            set_current_style();
        }
#else
        for (const auto &c : line) {
            IO::standard_out().putc(c);
        }
        IO::standard_out().putc(UNICODE_LINEFEED);
#endif

        std::copy(line.begin(), line.end(), input.line.begin());
    }

#ifdef ZTERP_GLK
    // It’s possible that output is buffered, meaning that until
    // glk_select() is called, output will not be displayed. When reading
    // from a command-script, flush on each command so that output is
    // visible while the script is being replayed.
    screen_flush();

    saw_input = true;
#endif

    return true;
}

#ifdef GLK_MODULE_LINE_TERMINATORS
// Glk returns terminating characters as keycode_*, but we need them as
// ZSCII. This should only ever be called with values that are matched
// in the switch, because those are the only ones that Glk was told are
// terminating characters. In the event that another keycode comes
// through, though, treat it as Enter.
static uint8_t zscii_from_glk(glui32 key)
{
    switch (key) {
    case 13:             return ZSCII_NEWLINE;
    case keycode_Up:     return ZSCII_UP;
    case keycode_Down:   return ZSCII_DOWN;
    case keycode_Left:   return ZSCII_LEFT;
    case keycode_Right:  return ZSCII_RIGHT;
    case keycode_Func1:  return ZSCII_F1;
    case keycode_Func2:  return ZSCII_F2;
    case keycode_Func3:  return ZSCII_F3;
    case keycode_Func4:  return ZSCII_F4;
    case keycode_Func5:  return ZSCII_F5;
    case keycode_Func6:  return ZSCII_F6;
    case keycode_Func7:  return ZSCII_F7;
    case keycode_Func8:  return ZSCII_F8;
    case keycode_Func9:  return ZSCII_F9;
    case keycode_Func10: return ZSCII_F10;
    case keycode_Func11: return ZSCII_F11;
    case keycode_Func12: return ZSCII_F12;
#ifdef SPATTERLIGHT
    case keycode_Pad0: return ZSCII_KEY0;
    case keycode_Pad1: return ZSCII_KEY1;
    case keycode_Pad2: return ZSCII_KEY2;
    case keycode_Pad3: return ZSCII_KEY3;
    case keycode_Pad4: return ZSCII_KEY4;
    case keycode_Pad5: return ZSCII_KEY5;
    case keycode_Pad6: return ZSCII_KEY6;
    case keycode_Pad7: return ZSCII_KEY7;
    case keycode_Pad8: return ZSCII_KEY8;
    case keycode_Pad9: return ZSCII_KEY9;
#endif
    }

    return ZSCII_NEWLINE;
}
#endif

static void flush_image_buffer(void) {
    if (current_graphics_buf_win == nullptr)
        current_graphics_buf_win = graphics_win_glk;
    if (is_game(Game::Arthur) && screenmode == MODE_NORMAL) {
        glk_window_clear(graphics_win_glk);
        draw_arthur_side_images(graphics_win_glk);
    } else {
        flush_bitmap(current_graphics_buf_win);
    }
}

bool arthur_intro_timer = false;
bool journey_cursor_timer = false;
bool journey_cursor_reverse = true;

// Attempt to read input from the user. The input type can be either a
// single character or a full line. If “timer” is not zero, a timer is
// started that fires off every “timer” tenths of a second (if the value
// is 1, it will timeout 10 times a second, etc.). Each time the timer
// times out the routine at address “routine” is called. If the routine
// returns true, the input is canceled.
//
// The function returns true if input was stored, false if there was a
// cancellation as described above.
static bool get_input(uint16_t timer, uint16_t routine, Input &input)
{
    uint8_t clicktype = 0;

    // If either of these is zero, no timeout should happen.
    if (timer == 0) {
        routine = 0;
    }
    if (routine == 0) {
        timer = 0;
    }

    // Flush all streams when input is requested.
#ifndef ZTERP_GLK
    IO::standard_out().flush();
#endif
    if (scriptio != nullptr) {
        scriptio->flush();
    }
    if (transio != nullptr) {
        transio->flush();
    }
    if (perstransio != nullptr) {
        perstransio->flush();
    }

    // Generally speaking, newline will be the reason the line input
    // stopped, so set it by default. It will be overridden where
    // necessary.
    input.term = ZSCII_NEWLINE;

    if (istream == ISTREAM_FILE && istream_read_from_file(input)) {
#ifdef ZTERP_GLK
        saw_input = true;
#endif
        return true;
    }
#ifdef ZTERP_GLK
    enum class InputStatus { Waiting, Received, Canceled } status = InputStatus::Waiting;
    Line line;
    Window *saved = nullptr;
    // Mouse support is turned on if the game requests it via Flags2
    // and, for @read, if it adds single click to the list of
    // terminating keys
    bool enable_mouse = mouse_available() &&
                        ((input.type == Input::Type::Char) ||
                         (input.type == Input::Type::Line && term_mouse));

    // In V6, input might be requested on an unsupported window. If so,
    // switch to the main window temporarily.
    if (curwin->id == nullptr) {
        saved = curwin;
        curwin = mainwin;
        glk_set_window(curwin->id);
    }

    if (enable_mouse) {
        if (mousewin && mousewin->id)
            glk_request_mouse_event(mousewin->id);
        else {
            glk_request_mouse_event(curwin->id);
            if (upperwin->id)
                glk_request_mouse_event(upperwin->id);
            if (is_game(Game::ZorkZero) && screenmode == MODE_MAP) {
                glk_request_mouse_event(current_graphics_buf_win);
            }
        }
    }

    switch (input.type) {
    case Input::Type::Char:
        request_char();
        break;
    case Input::Type::Line:
        line.len = input.preloaded;
        for (int i = 0; i < input.preloaded; i++) {
            if (have_unicode) {
                line.unicode[i] = input.line[i];
            } else {
                line.latin1 [i] = input.line[i];
            }
        }
        request_line(line, input.maxlen);
            if (input.preloaded > 0) {
                if ( input.line[input.preloaded - 1] == 10 || input.line[input.preloaded - 1] == 13) {
                    fprintf(stderr, "Newline!\n");
                } else {
                    fprintf(stderr, "Last char of preloaded input:\"%c\"\n", input.line[input.preloaded - 1]);
                }
            }
        break;
    }

    if (timer != 0 && !arthur_intro_timer && !journey_cursor_timer) {
        start_timer(timer);
    }

    while (status == InputStatus::Waiting) {
        event_t ev;

        glk_select(&ev);

        switch (ev.type) {
        case evtype_Arrange:
            window_change();
            break;

        case evtype_Timer:
                if (arthur_intro_timer) {
                    arthur_intro_timer = false;
                    draw_centered_title_image(2);
                    draw_centered_title_image(3);
                    flush_bitmap(current_graphics_buf_win);
                    stop_timer();
                    restart_read_events(line, input, enable_mouse);
                    start_timer(timer);
                    break;
                }

                if (journey_cursor_timer) {
                    draw_flashing_journey_cursor();
                    break;
                }


            ZASSERT(timer != 0, "got unexpected evtype_Timer");

            if (is_game(Game::ZorkZero) || is_game(Game::Shogun) || is_game(Game::Arthur)) {
                flush_image_buffer();
            }

            stop_timer();

            if (handle_interrupt(routine, &line) != 0) {
                status = InputStatus::Canceled;
                input.term = 0;
            } else {
                restart_read_events(line, input, enable_mouse);
                start_timer(timer);
            }

            break;

        case evtype_CharInput:
            ZASSERT(input.type == Input::Type::Char, "got unexpected evtype_CharInput");
                if (ev.win != curwin->id) {
                    Window *matching = nullptr;
                    for (auto &window : windows) {
                        if (window.id == ev.win) {
                            matching = &window;
                            break;
                        }
                    }
                    fprintf(stderr, "Expected char input from window %d, peer %d, but got char input from window %d, peer %d\n", curwin->index, curwin->id->peer, matching->index, ev.win->peer);
                }
//            ZASSERT(ev.win == curwin->id, "got evtype_CharInput on unexpected window");

            fprintf(stderr, "BOCFEL6: screen.c: get_input: received key input %d (0x%x) for window %d (peer %d)\n", ev.val1,  ev.val1, curwin->index, curwin->id->peer);
//            sleep(1);

            status = InputStatus::Received;

            // As far as the Standard is concerned, there is no difference in
            // behavior for @read_char between versions 4 and 5, as §10.7.1 claims
            // that all characters defined for input can be returned by @read_char.
            // However, according to Infocom’s EZIP documentation, “keys which do
            // not have a single ASCII value are ignored,” with the exception that
            // up, down, left, and right map to 14, 13, 11, and 7, respectively.
            // From looking at V4 games’ source code, it seems like only Bureaucracy
            // makes use of these codes, and then only partially. Since the down
            // arrow maps to 13, it is indistinguishable from ENTER. Bureaucracy
            // treats 14 like ^, meaning “back up a field”. The other arrows aren’t
            // handled specifically, but what Bureaucracy does with all characters
            // it doesn’t handle specifically is print them out. In the DOS version
            // of Bureaucracy, hitting the left arrow inserts a mars symbol (which
            // is what code page 437 uses to represent value 11). Hitting the right
            // arrow results in a beep since 7 is the bell in ASCII. Up and down
            // work as expected, but left and right appear to have just been ignored
            // and untested. On the Apple II, up and down work, but left and right
            // cause nothing to be printed. However, Bureaucracy does advance its
            // cursor location when the arrows are hit (as it does with every
            // character), which causes interesting behavior: hit the right arrow a
            // couple of times and then hit backspace: the cursor will jump to the
            // right first due to the fact that the cursor position was advanced
            // without anything actually being printed. This can’t be anything but a
            // bug.
            //
            // At any rate, given how haphazard the arrow keys act under V4, they
            // are just completely ignored here. This does no harm for Bureaucracy,
            // since ENTER and ^ can be used. In fact, all input which is not also
            // defined for output (save for BACKSPACE, given that Bureaucracy
            // properly handles it) is ignored. As a result, Bureaucracy will not
            // accidentally print out invalid values (since only valid values will
            // be read). This violates both the Standard (since that allows all
            // values defined for input) and the EZIP documentation (since that
            // mandates ASCII only), but hopefully provides the best experience
            // overall: Unicode values are allowed, but Bureaucracy no longer tries
            // to print out invalid characters.
            //
            // Note that if function keys are supported in V4, Bureaucracy will pass
            // them along to @print_char, even though they’re not valid for output,
            // so ignoring things provides better results than following the
            // Standard.
            if (zversion == 4) {
                switch (ev.val1) {
                case keycode_Delete: input.key = ZSCII_DELETE; break;
                case keycode_Return: input.key = ZSCII_NEWLINE; break;

                default:
                    if (ev.val1 >= 32 && ev.val1 <= 126) {
                        input.key = ev.val1;
                    } else if (ev.val1 < UINT16_MAX) {
                        uint8_t c = unicode_to_zscii[ev.val1];

                        if (c != 0) {
                            input.key = c;
                        }
                    } else {
                        status = InputStatus::Waiting;
                        request_char();
                    }
                }
            } else {
                switch (ev.val1) {
                case keycode_Delete: input.key = ZSCII_DELETE; break;
                case keycode_Return: input.key = ZSCII_NEWLINE; break;
                case keycode_Escape: input.key = ZSCII_ESCAPE; break;
                case keycode_Up:     input.key = ZSCII_UP; break;
                case keycode_Down:   input.key = ZSCII_DOWN; break;
                case keycode_Left:   input.key = ZSCII_LEFT; break;
                case keycode_Right:  input.key = ZSCII_RIGHT; break;
                case keycode_Func1:  input.key = ZSCII_F1; break;
                case keycode_Func2:  input.key = ZSCII_F2; break;
                case keycode_Func3:  input.key = ZSCII_F3; break;
                case keycode_Func4:  input.key = ZSCII_F4; break;
                case keycode_Func5:  input.key = ZSCII_F5; break;
                case keycode_Func6:  input.key = ZSCII_F6; break;
                case keycode_Func7:  input.key = ZSCII_F7; break;
                case keycode_Func8:  input.key = ZSCII_F8; break;
                case keycode_Func9:  input.key = ZSCII_F9; break;
                case keycode_Func10: input.key = ZSCII_F10; break;
                case keycode_Func11: input.key = ZSCII_F11; break;
                case keycode_Func12: input.key = ZSCII_F12; break;
#ifdef SPATTERLIGHT
                case keycode_Pad0: input.key = ZSCII_KEY0; break;
                case keycode_Pad1: input.key = ZSCII_KEY1; break;
                case keycode_Pad2: input.key = ZSCII_KEY2; break;
                case keycode_Pad3: input.key = ZSCII_KEY3; break;
                case keycode_Pad4: input.key = ZSCII_KEY4; break;
                case keycode_Pad5: input.key = ZSCII_KEY5; break;
                case keycode_Pad6: input.key = ZSCII_KEY6; break;
                case keycode_Pad7: input.key = ZSCII_KEY7; break;
                case keycode_Pad8: input.key = ZSCII_KEY8; break;
                case keycode_Pad9: input.key = ZSCII_KEY9; break;
#endif
                default:
                    input.key = ZSCII_QUESTIONMARK;
                    if (ev.val1 <= UINT16_MAX) {
                        uint8_t c = unicode_to_zscii[ev.val1];

                        if (c != 0) {
                            input.key = c;
                        }
                    }
                        transcribe(ZSCII_NEWLINE);
                        transcribe(input.key);
                        transcribe(ZSCII_NEWLINE);

                    break;
                }
            }

            break;

        case evtype_LineInput:
            ZASSERT(input.type == Input::Type::Line, "got unexpected evtype_LineInput");
            ZASSERT(ev.win == curwin->id, "got evtype_LineInput on unexpected window");
            line.len = ev.val1;
#ifdef GLK_MODULE_LINE_TERMINATORS
            if (zversion >= 5) {
                input.term = zscii_from_glk(ev.val2);
            }
#endif
            status = InputStatus::Received;
            break;
        case evtype_MouseInput:
#pragma mark MOUSE INPUT
            fprintf(stderr, "Mouse input event: x: %d y: %d\n", ev.val1, ev.val2);
            {
                int16_t xoffset = 0, yoffset = 0;
                float xmultiplier = 1, ymultiplier = 1;

                if (zversion == 6 && ev.win && ev.win->type == wintype_TextGrid) {
                    xmultiplier = letterwidth;
                    ymultiplier = letterheight;
                    if (is_game(Game::ZorkZero)) {
                        if (screenmode == MODE_NORMAL) {
                            xoffset = letterwidth + ggridmarginx;
                            if (graphics_type == kGraphicsTypeApple2) {
                                curwin->last_click_x = -1;
                                curwin->last_click_y = -1;
                                yoffset = -3 * imagescaley + ggridmarginy;
                                xmultiplier = gcellw;
                                ymultiplier = gcellh;
                                fprintf(stderr, "yoffset: %hd, xmultiplier: %f, ymultiplier: %f\n", yoffset, xmultiplier, ymultiplier);
                                fprintf(stderr, "Upperwin y_size: %d\n", upperwin->y_size);
//                                if (ymultiplier * (float)(ev.val2 + 1) + yoffset > 80)
//                                    yoffset += 80 - (ymultiplier * (float)(ev.val2 + 1) + yoffset);
                            }
                        }
                    }
                    if (screenmode == MODE_HINTS) {
                        if (ev.win == mainwin->id) {
                            yoffset = mainwin->y_origin - letterheight;
                        }
                    } else if (screenmode == MODE_DEFINE) {
                        yoffset = windows[2].y_origin - letterheight + ggridmarginy;
                        xoffset = windows[2].x_origin - letterwidth + ggridmarginx;
                    }
                }

                if (is_game(Game::Arthur) && screenmode == MODE_MAP) {
                    glk_request_mouse_event(graphics_win_glk);
                    xmultiplier = 1 / imagescalex;
                    ymultiplier = 1 / imagescaley;
                }

                zterp_mouse_click(ev.val1 + 1, ev.val2 + 1, xoffset, yoffset, xmultiplier, ymultiplier);
                fprintf(stderr, "imagescale:%f\n", imagescalex);
            }
            status = InputStatus::Received;

                if (curwin->last_click_x == ev.val1 && curwin->last_click_y == ev.val2)
                    clicktype = ZSCII_CLICK_DOUBLE;
                else
                    clicktype = ZSCII_CLICK_SINGLE;
                curwin->last_click_x = ev.val1;
                curwin->last_click_y = ev.val2;

            switch (input.type) {
            case Input::Type::Char:
                input.key = clicktype;
                break;
            case Input::Type::Line:
                glk_cancel_line_event(curwin->id, &ev);
                input.len = ev.val1;
                input.term = clicktype;
                break;
            }

            break;
#ifdef GLK_MODULE_SOUND2
        case evtype_SoundNotify: {
            sound_stopped(ev.val2);
            uint16_t sound_routine = sound_get_routine(ev.val2);
            if (sound_routine != 0) {
                handle_interrupt(sound_routine, &line);
                restart_read_events(line, input, enable_mouse);
            }
            break;
        }
#endif
        }
    }

    stop_timer();

    if (enable_mouse) {
        if (mousewin && mousewin->id) {
            glk_cancel_mouse_event(mousewin->id);
        } else {
            glk_cancel_mouse_event(curwin->id);
        }
    }

    switch (input.type) {
    case Input::Type::Char:
        glk_cancel_char_event(curwin->id);
        break;
    case Input::Type::Line:
            // Copy the Glk line into the internal input structure.
            input.len = line.len;
            for (glui32 i = 0; i < line.len; i++) {
                if (have_unicode) {
                    input.line[i] = line.unicode[i] > UINT16_MAX ? UNICODE_REPLACEMENT : line.unicode[i];
                } else {
                    input.line[i] = static_cast<unsigned char>(line.latin1[i]);
                }
            }

            // When line input echoing is turned off (which is the case on
            // Glk implementations that support it), input won’t be echoed
            // to the screen after it’s been entered. This will echo it
            // where appropriate, for both canceled and completed input.
            if (curwin->has_echo) {
                glk_set_style(style_Input);
                for (glui32 i = 0; i < input.len; i++) {
                    xglk_put_char(input.line[i]);
                }

                if (input.term == ZSCII_NEWLINE) {
                    xglk_put_char(UNICODE_LINEFEED);
                }
                set_current_style();
            }

        // If the current window is the upper window, the position of
        // the cursor needs to be tracked, so after a line has
        // successfully been read, advance the cursor to the initial
        // position of the next line, or if a terminating key was used
        // or input was canceled, to the end of the input.
            if (curwin == upperwin) {
                if (input.term != ZSCII_NEWLINE) {
                    upperwin->x_origin += input.len;
                }

                if (input.term == ZSCII_NEWLINE || upperwin->x_origin >= upperwin->x_size) {
                    upperwin->x_origin = 0;
                    if (upperwin->y_origin < upperwin->y_size) {
                        upperwin->y_origin++;
                    }
                }

                glk_window_move_cursor(upperwin->id, (glui32)upperwin->x_origin - 1, (glui32)upperwin->y_origin - 1);
            }


            if (is_game(Game::Arthur)) {
                // Delete Arthur error window
                v6_delete_glk_win(windows[3].id);

                if (input.term != ZSCII_NEWLINE) {
                    switch(input.term) {
                        case ZSCII_F1:
                            screenmode = MODE_NORMAL;
                            break;
                        case ZSCII_F2:
                            screenmode = MODE_MAP;
                            glk_request_mouse_event(graphics_win_glk);
                            break;
                        case ZSCII_F3:
                            screenmode = MODE_INVENTORY;
                            break;
                        case ZSCII_F4:
                            screenmode = MODE_STATUS;
                            break;
                        case ZSCII_F5:
                            screenmode = MODE_ROOM_DESC;
                            break;
                        case ZSCII_F6:
                            screenmode = MODE_NOGRAPHICS;
                            break;
                        default:
                            break;
                    }
                }
            }
    }

    saw_input = true;

    if (errorwin != nullptr) {
        if (zversion != 6) {
            glk_window_close(errorwin, nullptr);
        } else {
            gli_delete_window(errorwin);
        }
        errorwin = nullptr;
    }

    if (saved != nullptr) {
        curwin = saved;
        glk_set_window(curwin->id);
    }

    return status != InputStatus::Canceled;
#else
    switch (input.type) {
    case Input::Type::Char: {
        std::vector<uint16_t> line;

        try {
            line = IO::standard_in().readline();
        } catch (const IO::EndOfFile &) {
            zquit();
        }

        if (line.empty()) {
            input.key = ZSCII_NEWLINE;
        } else if (line[0] == UNICODE_DELETE) {
            input.key = ZSCII_DELETE;
        } else if (line[0] == UNICODE_ESCAPE) {
            input.key = ZSCII_ESCAPE;
        } else {
            input.key = unicode_to_zscii[line[0]];
            if (input.key == 0) {
                input.key = ZSCII_NEWLINE;
            }
        }
        break;
    }
    case Input::Type::Line:
        input.len = input.preloaded;

        if (input.maxlen > input.preloaded) {
            std::vector<uint16_t> line;

            try {
                line = IO::standard_in().readline();
            } catch (const IO::EndOfFile &) {
                zquit();
            }

            if (line.size() > input.maxlen - input.preloaded) {
                line.resize(input.maxlen - input.preloaded);
            }

            std::copy(line.begin(), line.end(), &input.line[input.preloaded]);
            input.len += line.size();
        }
        break;
    }

    return true;
#endif
}

uint8_t read_char(void) {
    Input input;
    input.type = Input::Type::Char;

    if (!get_input(1, 0, input))
        return 0;
    return input.key;
}

void zread_char()
{
    fprintf(stderr, "zread_char %d %d %d\n", zargs[0], zargs[1], zargs[2]);

    uint16_t timer = 0;
    uint16_t routine = zargs[2];
    Input input;

    input.type = Input::Type::Char;

    if (is_game(Game::ZorkZero) || is_game(Game::Shogun) || is_game(Game::Arthur)) {
        flush_image_buffer();
    }

//    if (options.autosave && !in_interrupt()) {
//#ifdef SPATTERLIGHT
//        spatterlight_do_autosave(SaveOpcode::ReadChar);
//#else
//        do_save(SaveType::Autosave, SaveOpcode::ReadChar);
//#endif
//    }

    if (zversion >= 4 && znargs > 1) {
        timer = zargs[1];
    }

    if (!get_input(timer, routine, input)) {
        store(0);
        return;
    }

#ifdef ZTERP_GLK
    update_delayed();
#endif

    if (streams.test(STREAM_SCRIPT)) {
        // Values 127 to 159 are not valid Unicode, and these just happen to
        // match up to the values needed for special ZSCII keys, so store
        // them as-is.
        if (special_zscii(input.key)) {
            scriptio->putc(input.key);
        } else {
            scriptio->putc(zscii_to_unicode[input.key]);
        }
    }

    store(input.key);
}

void zread_mouse() {
    fprintf(stderr, "zread_mouse\n");
}

void zmouse_window() {
    fprintf(stderr, "zmouse_window %d\n", as_signed(zargs[0]));
    if (as_signed(zargs[0]) == -1) {
        mousewin = nullptr;
    } else {
        if (is_game(Game::Arthur) && zargs[0] == 2)
            mousewin = find_window(7);
        else
            mousewin = find_window(zargs[0]);
    }
}


#ifdef ZTERP_GLK
static void status_putc(uint8_t c)
{
    xglk_put_char_stream(glk_window_get_stream(statuswin.id), zscii_to_unicode[c]);
}
#endif

// §8.2.3.2 says the hours can be assumed to be in the range [0, 23] and
// can be reduced modulo 12 for 12-hour time. Cutthroats, however, sets
// the hours to 111 if the watch is dropped, on the assumption that the
// interpreter will convert 24-hour time to 12-hour time simply by
// subtracting 12, resulting in an hours of 99 (the minutes are set to
// 99 for a final time of 99:99). Perform the calculation as in
// Cutthroats, which results in valid times for [0, 23] as well as
// allowing the 99:99 hack to work.
std::string screen_format_time(long hours, long minutes)
{
    return fstring("Time: %ld:%02ld%s ", hours <= 12 ? (hours + 11) % 12 + 1 : hours - 12, minutes, hours < 12 ? "am" : "pm");
}

void zshow_status()
{
#ifdef ZTERP_GLK
    glui32 width, height;
    std::string rhs;
    long first = as_signed(variable(0x11)), second = as_signed(variable(0x12));
    strid_t stream;

    if (statuswin.id == nullptr) {
        return;
    }

    stream = glk_window_get_stream(statuswin.id);

    glk_window_clear(statuswin.id);

    glk_window_get_size(statuswin.id, &width, &height);

#ifdef GLK_MODULE_GARGLKTEXT
    garglk_set_reversevideo_stream(stream, 1);
#else
    glk_set_style_stream(stream, style_Alert);
#endif
    for (glui32 i = 0; i < width; i++) {
        xglk_put_char_stream(stream, UNICODE_SPACE);
    }

    glk_window_move_cursor(statuswin.id, 1, 0);

    // Variable 0x10 is global variable 1.
    print_object(variable(0x10), status_putc);

    if (status_is_time()) {
        rhs = screen_format_time(first, second);
        if (rhs.size() > width) {
            rhs = fstring("%02ld:%02ld", first, second);
        }
    } else {
        // Planetfall and Stationfall are score games, except the value
        // in the second global variable is the current Galactic
        // Standard Time, not the number of moves. Most of Infocom’s
        // interpreters displayed the score and moves as “score/moves”
        // so it didn’t look flat-out wrong to have a value that wasn’t
        // actually the number of moves. When it’s spelled out as
        // “Moves”, though, then the display is obviously wrong. For
        // these two games, rewrite “Moves” as “Time”. Note that this is
        // how it looks in the Solid Gold version, which is V5, meaning
        // Infocom had full control over the status line, implying it is
        // the most correct display.
        if (is_game(Game::Planetfall) || is_game(Game::Stationfall)) {
            rhs = fstring("Score: %ld  Time: %ld ", first, second);
        } else {
            rhs = fstring("Score: %ld  Moves: %ld ", first, second);
        }

        if (rhs.size() > width) {
            rhs = fstring("%ld/%ld", first, second);
        }
    }

    if (rhs.size() <= width) {
        glk_window_move_cursor(statuswin.id, width - (glui32)rhs.size(), 0);
        glk_put_string_stream(stream, &rhs[0]);
    }
#endif
}

#ifdef ZTERP_GLK
// These track the position of the cursor in the upper window at the
// beginning of a @read call so that the cursor can be replaced in case
// of a meta-command. They are also stored in the Scrn chunk of any
// meta-saves. They are in Z-machine coordinate format, not Glk, which
// means they are 1-based.
static long starting_x, starting_y;
#endif

// Attempt to read and parse a line of input. On success, return true.
// Otherwise, return false to indicate that input should be requested
// again.
static bool read_handler()
{
    uint16_t text = zargs[0], parse = zargs[1];
    ZASSERT(zversion >= 5 || user_byte(text) > 0, "text buffer cannot be zero sized");
    uint8_t maxchars = zversion >= 5 ? user_byte(text) : user_byte(text) - 1;
    std::array<uint8_t, 256> zscii_string;
    Input input;
    input.type = Input::Type::Line;
    input.maxlen = maxchars;
    input.preloaded = 0;
    uint16_t timer = 0;
    uint16_t routine = zargs[3];

    if (is_game(Game::ZorkZero) || is_game(Game::Shogun) || is_game(Game::Arthur)) {
        flush_image_buffer();
    }

    if (options.autosave && !in_interrupt()) {
#ifdef SPATTERLIGHT
//        spatterlight_do_autosave(SaveOpcode::Read);
#else
        do_save(SaveType::Autosave, SaveOpcode::Read);
#endif
    }

#ifdef ZTERP_GLK
    starting_x = upperwin->x_origin + 1;
    starting_y = upperwin->y_origin + 1;
#endif

    if (zversion <= 3) {
        zshow_status();
    }

    if (zversion >= 4 && znargs > 2) {
        timer = zargs[2];
    }

    if (zversion >= 5) {
        int i;

        input.preloaded = user_byte(text + 1);
        ZASSERT(input.preloaded <= maxchars, "too many preloaded characters: %d when max is %d", input.preloaded, maxchars);

        for (i = 0; i < input.preloaded; i++) {
            input.line[i] = zscii_to_unicode[user_byte(text + i + 2)];
        }
        // Under Gargoyle, preloaded input generally works as it’s supposed to.
        // Under Glk, it can fail one of two ways:
        // 1. The preloaded text is printed out once, but is not editable.
        // 2. The preloaded text is printed out twice, the second being editable.
        // I have chosen option #2. For non-Glk, option #1 is done by necessity.
        //
        // The “normal” mode of operation for preloaded text seems to be that a
        // particular string is printed, and then that string is preloaded,
        // allowing the already-printed text to be edited. However, there is no
        // requirement that the preloaded text already be on-screen. For
        // example, what should happen if the text “ZZZ” is printed out, but the
        // text “AAA” is preloaded? Infocom’s interpreters display “ZZZ” (and
        // not “AAA”), and allow the user to edit it; but any characters not
        // edited by the user (that is, not backspaced over) are actually stored
        // as “A” characters. The on-screen “Z” characters are not stored.
        //
        // This isn’t possible under Glk, and as it stands, isn’t possible under
        // Gargoyle either (although it could be extended to support it). For
        // now I am not extending Gargoyle: the usual case (preloaded text
        // matching on-screen text) does work with Gargoyle’s unput extensions,
        // and the unusual/unexpected case (preloaded text not matching) at
        // least is usable: the whole preloaded string is displayed and can be
        // edited. I suspect nobody but Infocom ever uses preloaded text, and
        // Infocom uses it in the normal way, so things work just fine.
#ifdef GARGLK
        input.line[i] = 0;
        if (curwin->id != nullptr) {
            // Convert the Z-machine's 16-bit string to a 32-bit string for Glk.
            std::vector<glui32> line32;
            std::copy(input.line.begin(), input.line.begin() + input.preloaded + 1, std::back_inserter(line32));
            // If the preloaded text would wrap backward in the upper window
            // (to the previous line), limit it to just the current line. For
            // example:
            //
            // |This text bre|
            // |aks          |
            //
            // If the string “breaks” is preloaded, only try to unput the “aks”
            // part. Backing all the way up to “bre” works (as in, is
            // successfully unput) in Gargoyle, but input won’t work: the cursor
            // will be placed at line 1, right after the “e”, and no input will
            // be allowed. At least by keeping the cursor on the second line,
            // proper user input will occur.
            if (curwin == upperwin) {
                long max = input.preloaded > upperwin->x_origin ? upperwin->x_origin : input.preloaded;
                long start = input.preloaded - max;
                glui32 unput = garglk_unput_string_count_uni(&line32[start]);

                // Since the preloaded text might not have been on the screen
                // (or only partially so), reduce the current and starting X
                // coordinates by the number of unput characters, since that is
                // where Gargoyle will logically be starting input.
                curwin->x_origin -= unput;
                starting_x -= unput;
            } else {
                garglk_unput_string_uni(line32.data());
            }
        }
#endif
    }

    if (!get_input(timer, routine, input)) {
        if (zversion >= 5) {
            store(0);
        }
        return true;
    }

#ifdef ZTERP_GLK
    update_delayed();
#endif

    if (curwin == mainwin) {
        history.add_input(input.line.data(), input.len);
    }

    if (options.enable_escape) {
        transcribe(033);
        transcribe('[');
        for (const auto c : *options.escape_string) {
            transcribe(c);
        }
    }

    for (int i = 0; i < input.len; i++) {
        transcribe(input.line[i]);
        if (streams.test(STREAM_SCRIPT)) {
            scriptio->putc(input.line[i]);
        }
    }

    if (options.enable_escape) {
        transcribe(033);
        transcribe('[');
        transcribe('0');
        transcribe('m');
    }

    transcribe(UNICODE_LINEFEED);
    if (streams.test(STREAM_SCRIPT)) {
        scriptio->putc(UNICODE_LINEFEED);
    }

    if (!options.disable_meta_commands) {
        input.line[input.len] = 0;

        if (input.line[0] == '/') {
#ifdef ZTERP_GLK
            // If the game is currently in the upper window, blank out
            // everything the user typed so that a re-request of input has a
            // clean slate to work with. Replace the cursor where it was at the
            // start of input.
            if (curwin == upperwin) {
                set_cursor(starting_y, starting_x, -3);
                for (int i = 0; i < input.len; i++) {
                    put_char_u(UNICODE_SPACE);
                }
                set_cursor(starting_y, starting_x, -3);
            }
#endif

            auto result = handle_meta_command(input.line.data(), input.len);
            switch (result.first) {
            case MetaResult::Rerequest:
                // The game still wants input, so try again. If this is the main
                // window, print a prompt that hopefully meshes well with the
                // game. If it’s the upper window, don’t print anything, because
                // that will almost certainly do more harm than good.
#ifdef ZTERP_GLK
                if (curwin != upperwin)
#endif
                {
                    screen_print("\n>");
                }

                // Any preloaded text is probably going to have been printed by
                // the game first, which allows Gargoyle to unput the preloaded
                // text. But when re-requesting input, the preloaded text will
                // not be on the screen anymore. Print it again so that it can
                // be unput.
                //
                // If this implementation is not Gargoyle, don’t print
                // anything. The original text (if there in fact was
                // any) will still be on the screen, and the preloaded
                // text will be displayed (and editable) by Glk.
#ifdef GARGLK
                for (int i = 0; i < input.preloaded; i++) {
                    put_char(zscii_to_unicode[user_byte(text + i + 2)]);
                }
#endif

                return false;
            case MetaResult::Say: {
                // Convert the UTF-8 result to Unicode using memory-backed I/O.
                IO io(std::vector<uint8_t>(result.second.begin(), result.second.end()), IO::Mode::ReadOnly);
                std::vector<uint16_t> string;

                for (auto c = io.getc(true); c != -1; c = io.getc(true)) {
                    string.push_back(c);
                }

                input.len = string.size();
                std::copy(string.begin(), string.end(), input.line.begin());
            }
            }
        }

        // V1–4 do not have @save_undo, so simulate one each time @read is
        // called.
        //
        // Although V5 introduced @save_undo, not all games make use of it
        // (e.g. Hitchhiker’s Guide). Until @save_undo is called, simulate
        // it each @read, just like in V1–4. If @save_undo is called, all
        // of these interpreter-generated save states are forgotten and the
        // game’s calls to @save_undo take over.
        //
        // Because V1–4 games will never call @save_undo, seen_save_undo
        // will never be true. Thus there is no need to test zversion.
        if (!seen_save_undo) {
            push_save(SaveStackType::Game, SaveType::Meta, SaveOpcode::Read, nullptr);
        }
    }

    for (int i = 0; i < input.len; i++) {
        zscii_string[i] = unicode_to_zscii_q[unicode_tolower(input.line[i])];
    }

    if (zversion >= 5) {
        user_store_byte(text + 1, input.len); // number of characters read

        for (int i = 0; i < input.len; i++) {
            user_store_byte(text + i + 2, zscii_string[i]);
        }

        if (parse != 0) {
            tokenize(text, parse, 0, false);
        }

        store(input.term);
    } else {
        for (int i = 0; i < input.len; i++) {
            user_store_byte(text + i + 1, zscii_string[i]);
        }

        user_store_byte(text + input.len + 1, 0);

        tokenize(text, parse, 0, false);
    }

    return true;
}

void zread()
{
    fprintf(stderr, "zread\n");
    while (!read_handler()) {
    }
}

void zprint_unicode()
{
    if (valid_unicode(zargs[0])) {
        put_char_u(zargs[0]);
    } else {
        put_char_u(UNICODE_REPLACEMENT);
    }
}

void zcheck_unicode()
{
    uint16_t res = 0;

    // valid_unicode() will tell which Unicode characters can be printed;
    // and if the Unicode character is in the Unicode input table, it can
    // also be read. If Unicode is not available, then any character >255
    // is invalid for both reading and writing.
    if (have_unicode || zargs[0] < 256) {
        // §3.8.5.4.5: “Unicode characters U+0000 to U+001F and U+007F to
        // U+009F are control codes, and must not be used.”
        //
        // Even though control characters can be read (e.g. delete and
        // linefeed), when they are looked at through a Unicode lens, they
        // should be considered invalid. I don’t know if this is the right
        // approach, but nobody seems to use @check_unicode, so it’s not
        // especially important. One implication of this is that it’s
        // impossible for this implementation of @check_unicode to return 2,
        // since a character must be valid for output before it’s even
        // checked for input, and all printable characters are also
        // readable.
        //
        // For what it’s worth, interpreters seem to disagree on this pretty
        // drastically:
        //
        // • Zoom 1.1.5 returns 1 for all control characters.
        // • Fizmo 0.7.8 returns 3 for characters 8, 10, 13, and 27, 1 for
        //   all other control characters.
        // • Frotz 2.44 and Nitfol 0.5 return 0 for all control characters.
        // • Filfre 1.1.1 returns 3 for all control characters.
        // • Windows Frotz 1.19 returns 2 for characters 8, 13, and 27, 0
        //   for other control characters in the range 00 to 1f. It returns
        //   a mixture of 2 and 3 for control characters in the range 7F to
        //   9F based on whether the specified glyph is available.
        if (valid_unicode(zargs[0])) {
            res |= 0x01;
            if (unicode_to_zscii[zargs[0]] != 0) {
                res |= 0x02;
            }
        }
    }

#ifdef ZTERP_GLK
    if (glk_gestalt(gestalt_CharOutput, zargs[0]) == gestalt_CharOutput_CannotPrint) {
        res &= ~static_cast<uint16_t>(0x01);
    }
    if (!glk_gestalt(gestalt_CharInput, zargs[0])) {
        res &= ~static_cast<uint16_t>(0x02);
    }
#endif

    store(res);
}

static struct {
    enum Game story_id;
    glui32 pic;
    glui32 pic1;
    glui32 pic2;
} mapper[] = {
//    { Game::ZorkZero,  5, 497, 498},
//    { Game::ZorkZero,  6, 501, 502},
//    { Game::ZorkZero,  7, 499, 500},
//    { Game::ZorkZero,  8, 503, 504},
//    { Game::Arthur,   54, 170, 171},
    { Game::Shogun,   50,  61,  62},
    { Game::Infocom1234,   0,   0,   0}
};

// Should picture_data and get_wind_prop be moved to a V6 source file?
void zpicture_data()
{
    fprintf(stderr, "zpicture_data pic %d table %d\n", zargs[0], zargs[1]);
    if (zargs[0] == 0) {
        user_store_word(zargs[1] + 0, 0);
        user_store_word(zargs[1] + 2, pixversion);
        branch_if(1);
        return;
    }

    glui32 pic = zargs[0];
    uint16_t table = zargs[1];
    int height = 1, width = 1;

    bool avail = get_image_size(pic, &width, &height);

    fprintf(stderr, "zpicture_data: width: %d height %d\n", width, height);

    fprintf(stderr, "z_picture_data: storing %f in picture table[0] (height) and %f in picture table[1] (width). imagescalex:%f, imagescaley:%f, gscreenw: %d, gscreenw / %f: %f\n", ceil((float)height * imagescaley), ceil((float)width * imagescalex), imagescalex, imagescaley, gscreenw, hw_screenwidth, (float)gscreenw / hw_screenwidth);
    if (is_game(Game::Arthur) && (is_arthur_map_image(pic))) {
        user_store_word(table + 0, height);
        user_store_word(table + 2, width);
    } else {
        user_store_word(table + 0, round((float)height * imagescaley));
        user_store_word(table + 2, round((float)width * imagescalex));
    }

    branch_if(avail);
}

#pragma mark zdraw_picture

void zdraw_picture()
{
    glsi32 y = as_signed(zargs[1]);
    glsi32 x = as_signed(zargs[2]);

//    int i;

    Window *win = curwin;

    if (y == 0)        /* use cursor line if y-coordinate is 0 */
        y = win->y_cursor;
    if (x == 0)        /* use cursor column if x-coordinate is 0 */
        x = win->x_cursor;
//EXT:5 5 6 draw_picture picture-number y x
//    Displays the picture with the given number. (y,x) coordinates (of the top left of the picture) are each optional, in that a value of zero for y or x means the cursor y or x coordinate in the current window. It is illegal to call this with an invalid picture number.

    fprintf(stderr, "zdraw_picture: %d %d %d\n", zargs[0], as_signed(zargs[1]), as_signed(zargs[2]));
    fprintf(stderr, "in window %d\n", curwin->index);
    int width, height;
    latest_picture = zargs[0];
//    extract_palette(pic);
    get_image_size(latest_picture, &width, &height);

    if (width == 0 && height == 0)
        return;

    if (is_game(Game::Arthur)) {
        // Skip Arthur "sidebars"
        // We draw them in flush_image_buffer() instead
        if (latest_picture == 170 || latest_picture == 171) {
            return;
        }
        // Intro "slideshow"
        if (latest_picture >= 1 && latest_picture <= 3) {
            if (current_graphics_buf_win == nullptr || current_graphics_buf_win == graphics_win_glk) {
                current_graphics_buf_win = v6_new_glk_window(wintype_Graphics, 0);
                glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
                win_sizewin(current_graphics_buf_win->peer, 0, 0, gscreenw, gscreenh);
            }
            screenmode = MODE_SLIDESHOW;
            glk_request_mouse_event(current_graphics_buf_win);
            ensure_pixmap(current_graphics_buf_win);
            if (latest_picture == 3) {
                arthur_intro_timer = true;
                glk_request_timer_events(500);
                return;
            }
            draw_centered_title_image(latest_picture);
            return;
        }

        else if (current_graphics_buf_win == nullptr) {
            current_graphics_buf_win = graphics_win_glk;
        }


        // Room images
        if (is_arthur_room_image(latest_picture) || is_arthur_stamp_image(latest_picture)) { // Pictures 106-149 are map images
            screenmode = MODE_NORMAL;
            ensure_pixmap(current_graphics_buf_win);
            draw_arthur_room_image(latest_picture);
            return;
        } else if (is_arthur_map_image(latest_picture)) {
            screenmode = MODE_MAP;
            if (latest_picture == 137) {// map background
                glk_request_mouse_event(graphics_win_glk);
            }
            else {
                draw_arthur_map_image(latest_picture, x, y);
                return;
            }
        } else if (latest_picture == 54) { // Border image, drawn in flush_image_buffer()
            screenmode = MODE_NORMAL;
            if (arthur_intro_timer) {
                arthur_intro_timer = 0;
                glk_request_timer_events(0);
            }
//            if (graphics_type == kGraphicsTypeApple2) // Except if Apple 2
//                draw_to_buffer(current_graphics_buf_win, pic, 0, 0);
            return;
        }

        draw_to_buffer(current_graphics_buf_win, latest_picture, x, y);
        return;
    }

    if (graphics_type != kGraphicsTypeApple2 && graphics_type != kGraphicsTypeMacBW && graphics_type != kGraphicsTypeCGA && graphics_type != kGraphicsTypeAmiga && graphics_type != kGraphicsTypeBlorb)
        for (int i = 0; mapper[i].story_id != Game::Infocom1234; i++) {
            if (is_game(mapper[i].story_id)) {
                if (latest_picture == mapper[i].pic) {
                    if (mapper[i].story_id == Game::ZorkZero) {
                        if (latest_picture == 8) {
                            screenmode = MODE_HINTS;
                            windows[7].id = nullptr;
                            glk_request_mouse_event(upperwin->id);
                        } else {
                            screenmode = MODE_NORMAL;
                        }
                    }
                    if (current_graphics_buf_win && current_graphics_buf_win != graphics_win_glk) {
                        v6_delete_glk_win(current_graphics_buf_win);
                    }
                    current_graphics_buf_win = graphics_win_glk;
                    int ypos = height * imagescaley;
                    draw_to_buffer(current_graphics_buf_win, mapper[i].pic1, 0, ypos);
                    int w;
                    get_image_size(mapper[i].pic2, &w, nullptr);
                    draw_to_buffer(current_graphics_buf_win, mapper[i].pic2, gscreenw - (w + 2) * imagescaley, ypos);
                    break;
                }
            }
        }

    if (is_game(Game::Journey)) {
        if (win->id && win->id->type != wintype_Graphics) {
            v6_delete_win(win);
        }
        v6_remap_win_to_graphics(win);
        if (win->id != nullptr) {
            glk_window_set_background_color(win->id, monochrome_black);
            glk_window_clear(win->id);

            float scale;
            adjust_journey_image(latest_picture, &x, &y, width, height, win->x_size, win->y_size, &scale, pixelwidth);
            draw_inline_image(win->id, latest_picture, x, y, scale, false);
        }
        return;
    }

    if (is_game(Game::Shogun)) {
        if (latest_picture == 1) {
            win->id = v6_new_glk_window(wintype_Graphics, 0);
            win->x_size = gscreenw;
            win->y_size = gscreenh;
            v6_sizewin(win);
            win->zpos = max_zpos++;
            float scale = gscreenw / ((float)width * pixelwidth);
            int ypos = gscreenh - height * scale + 1 + 2 * (graphics_type == kGraphicsTypeMacBW);
            glk_window_set_background_color(win->id, 0xffffff);
            glk_window_clear(win->id);
            draw_inline_image(win->id, latest_picture, 0, ypos, scale, false);
            screenmode = MODE_SLIDESHOW;
            glk_request_mouse_event(win->id);
            return;
        }

        if ((latest_picture == 4 || latest_picture == 50) && graphics_type == kGraphicsTypeApple2) {
            v6_delete_win(&windows[7]);
            v6_delete_win(&windows[6]);
            v6_delete_win(win);
            win->id = v6_new_glk_window(wintype_Graphics, 0);
            float scale = gscreenw / ((float)width * pixelwidth);
            win->x_size = width * scale * pixelwidth;
            win->y_size = height * scale;
            a2_graphical_banner_height = win->y_size;
            fprintf(stderr, "Shogun: set a2_graphical_banner_height to %d\n", a2_graphical_banner_height);
            if (latest_picture == 50) {
                fprintf(stderr, "Shogun drawing image 50 at hints screen. Adjust windows again\n");
                upperwin->y_size = 3 * letterheight;
                windows[7].y_origin = upperwin->y_size + 1;
                mainwin->y_origin = win->y_origin + win->y_size;
                mainwin->y_size = gscreenh - mainwin->y_origin;
                v6_sizewin(upperwin);
                v6_sizewin(mainwin);
            }
            v6_sizewin(win);
            win->zpos = max_zpos++;
            draw_inline_image(win->id, latest_picture, 0, 0, scale, false);
//            if (pic == 4) {
//                adjust_shogun_window();
//            }
            graphics_zpos = -1;
            return;
        }

        if (is_shogun_border_image(latest_picture)) {
//            if (screenmode != MODE_NORMAL)
            if (current_graphics_buf_win && current_graphics_buf_win != graphics_win_glk) {
                v6_delete_glk_win(current_graphics_buf_win);
            }
            current_graphics_buf_win = graphics_win_glk;
            win_sizewin(graphics_win_glk->peer, 0, 0, gscreenw, gscreenh);
//            screenmode = MODE_NORMAL;
            clear_image_buffer();
            glk_window_clear(graphics_win_glk);


            if (graphics_type != kGraphicsTypeApple2) {
                if (graphics_type == kGraphicsTypeMacBW || graphics_type == kGraphicsTypeAmiga) {
                    float scale = (float)gscreenw / ((float)width);
                    draw_inline_image(graphics_win_glk, latest_picture, 0, 0, scale, false);
                    if (latest_picture == 3) {
                        if (graphics_type == kGraphicsTypeMacBW) {
                            draw_inline_image(graphics_win_glk, latest_picture, 0, height * scale, scale, true);
                            shogun_graphical_banner_width_left = scale * 44;
                            shogun_graphical_banner_width_right = scale * 45;
                        } else {
                            draw_inline_image(graphics_win_glk, latest_picture, 0, (height - 2) * scale, scale, true);
                            shogun_graphical_banner_width_left = scale * 23.4;
                            shogun_graphical_banner_width_right = scale * 22.6;
                        }
                    }
                    adjust_shogun_window();
                    return;
                } else {
                    if (latest_picture == 59)
                        return;
                    if (latest_picture == 3) {
                        float scaledwidth = (float)width * imagescalex;
                        shogun_graphical_banner_width_left = scaledwidth + imagescalex;
                        shogun_graphical_banner_width_right = scaledwidth + imagescalex;
                        int ypos = height * imagescaley;
                        if (graphics_type == kGraphicsTypeCGA)
                            ypos -= 5 * imagescaley;
                        draw_inline_image(graphics_win_glk, 59, gscreenw - scaledwidth + imagescalex, 0, imagescaley, false);
                        draw_inline_image(graphics_win_glk, 59, 0, ypos - imagescaley * 2, imagescaley * 1.05, true);
                        draw_inline_image(graphics_win_glk, 3, 0, 0, imagescaley, false);
                        draw_inline_image(graphics_win_glk, 3, gscreenw - scaledwidth + imagescalex, ypos, imagescaley, true);
                    } if (latest_picture == 50) {
                        draw_inline_image(graphics_win_glk, 50, 0, 0, imagescaley, false);
                        draw_inline_image(graphics_win_glk, 61, 0, height * imagescaley, imagescaley, false);
                        draw_inline_image(graphics_win_glk, 61, 0, (height + 160) * imagescaley, imagescaley, false);
                        draw_inline_image(graphics_win_glk, 62, gscreenw - 57 * imagescalex, height * imagescaley, imagescaley, false);
                        draw_inline_image(graphics_win_glk, 62, gscreenw - 57 * imagescalex, (height + 161) * imagescaley, imagescaley, false);
                    } else {
                        draw_inline_image(graphics_win_glk, latest_picture, x, y, imagescaley, false);
                    }
                    adjust_shogun_window();
                    return;
                }
            } else {
                float scale = gscreenw / ((float)width * pixelwidth);
                a2_graphical_banner_height = height * scale;
                if (latest_picture == 3) {
                    draw_inline_image(graphics_win_glk, latest_picture, 0, 0, scale, false);
                    draw_inline_image(graphics_win_glk, latest_picture, 0, gscreenh - height * scale, scale, false);
                }

            }
            return;


        } else if (win == &windows[6]) {
            // The original interpreter draws Shogun inline images to window 6,
            // so we redirect them to the main text buffer window here.
            win = mainwin;
        }
    }

    if (is_game(Game::ZorkZero)) {

        if (latest_picture == 1 || latest_picture == 25 || latest_picture == 163 || (latest_picture >= 34 && latest_picture <= 40 && screenmode == MODE_NORMAL)) {
            win->id = v6_new_glk_window(wintype_Graphics, 0);
            win->x_origin = 1;
            win->y_origin = 1;
            win->x_size = gscreenw;
            win->y_size = gscreenh;
            v6_sizewin(win);
            win->zpos = max_zpos;
            if (latest_picture == 1 || graphics_type == kGraphicsTypeApple2) {
                draw_to_buffer(win->id, latest_picture, 0, 0);
                glk_window_set_background_color(win->id, 0);
                glk_window_clear(win->id);
            }

            screenmode = MODE_MAP;
            current_graphics_buf_win = win->id;
            glk_request_mouse_event(win->id);
        }

        if (is_zorkzero_border_image(latest_picture)) {
            if (screenmode != MODE_NORMAL && windows[7].id && windows[7].id->type == wintype_Graphics) {
                v6_delete_win(&windows[7]);
                current_graphics_buf_win = graphics_win_glk;
            }
            screenmode = MODE_NORMAL;
            glk_request_mouse_event(current_graphics_buf_win);
        }

        if (is_zorkzero_game_bg(latest_picture)) {
            screenmode = MODE_Z0GAME;
            upperwin->y_origin += imagescaley * 2;
            v6_sizewin(upperwin);
            mainwin->y_size = imagescaley * 75;
            if (latest_picture == 99)
                mainwin->y_size = imagescaley * 49;
            v6_sizewin(mainwin);
            glk_cancel_line_event(mainwin->id, nullptr);
        }
        if (!win->id || win->id->type != wintype_TextBuffer) {
            if (current_graphics_buf_win == nullptr)
                current_graphics_buf_win = graphics_win_glk;
            draw_to_buffer(current_graphics_buf_win, latest_picture, x, y);
            if (screenmode == MODE_NORMAL && graphics_type != kGraphicsTypeApple2)
                adjust_zork0();
            return;
        }
    }

    if (width <= 130 && height <= 72 &&
        ((win->id == nullptr || win->id->type == wintype_TextGrid || (win->id->type == wintype_TextBuffer && !(is_game(Game::Shogun)) && x > width)))) {
        if (win->id != nullptr) {
            if (win->id->type == wintype_TextGrid)
                fprintf(stderr, "Trying to draw picture in text grid. ");
            else
                fprintf(stderr, "Trying to draw picture in text buffer. ");
        }
        fprintf(stderr, "Remapping to graphics window.\n");
//        if (!is_game(Game::Journey)) {
//            win->x_origin = win->y_origin = 1;
//            win->x_size = gscreenw;
//            win->y_size = gscreenh;
//        }
        win = v6_remap_win_to_graphics(curwin);
    }

    if (!win->id)
        v6_remap_win_to_graphics(win);

    if (!win->id)
        return;

    if (win->id->type == wintype_TextBuffer) {
        if (!is_game(Game::Shogun))
            pending_flowbreak = true;
        float inline_scale = 2.0;
        if (graphics_type == kGraphicsTypeMacBW)
            inline_scale = imagescalex;
        draw_inline_image(win->id, latest_picture , x < width ? imagealign_MarginLeft : imagealign_MarginRight, 0, inline_scale, false);
    }  else {
        draw_inline_image(win->id, latest_picture, x, y, imagescalex, false);
    }
}

void zerase_picture() {
//EXT:7 7 6 erase_picture picture-number y x
//    Like draw_picture, but paints the appropriate region to the background colour for the given window. It is illegal to call this with an invalid picture number.
    fprintf(stderr, "zerase_picture: %d %d %d\n", zargs[0], zargs[1], zargs[2]);
}

void zset_margins() {
//EXT:8 8 6 set_margins left right window
//    Sets the margin widths (in pixels) on the left and right for the given window (which are by default 0). If the cursor is overtaken and now lies outside the margins altogether, move it back to the left margin of the current line (see S8.8.3.2.2.1).
    fprintf(stderr, "zset_margins: %d %d %d\n", zargs[0], zargs[1], zargs[2]);
}

void zget_wind_prop()
{
    fprintf(stderr, "zget_wind_prop: win %d prop %d\n", as_signed(zargs[0]) , zargs[1]);

    uint8_t fg, bg;
    uint16_t val;
    Window *win;

    win = find_window(zargs[0]);

    // These are mostly bald-faced lies.
    switch (zargs[1]) {
    case 0: // y coordinate
        val = win->y_origin;
        break;
    case 1: // x coordinate
        val = win->x_origin;
        break;
    case 2:  // y size
            if (win->id && win->id->type == wintype_TextGrid) {

                if (is_game(Game::Shogun) && screenmode == MODE_CREDITS && win == &windows[7]) {
                    val = gscreenh;
                    break;
                }

//                if (is_game(Game::Journey) && win == &windows[1]) {
//                    unsigned int w, h;
//                    get_screen_size(w, h);
//                    val = h * letterheight;
//                    break;
//                }

                glui32 h;
                glk_window_get_size(win->id, NULL, &h);
                val = h * letterheight;
            } else
            val = win->y_size; // word(0x24) * font_height;
        break;
    case 3:  // x size
            if (win->id && win->id->type == wintype_TextGrid) {
                glui32 w;
                glk_window_get_size(win->id, &w, NULL);
                val = w * letterwidth; //(w + 2) * font_width;
//                if (is_game(Game::Journey) && win == &windows[1]) {
//                    val = gscreenw;
//                    break;
//                }
            } else {
                val = win->x_size;
            }
        break;
    case 4:  // y cursor
        val = win->y_cursor;
        break;
    case 5:  // x cursor
        val = win->x_cursor;
        break;
    case 6: // left margin size
        val = win->left;
        break;
    case 7: // right margin size
        val = win->right;
        break;
    case 8: // newline interrupt routine
        val = 0;
        break;
    case 9: // interrupt countdown
        val = 0;
        break;
    case 10: // text style
        val = win->style.to_ulong();
        break;
    case 11: // colour data
            if (win->fg_color.mode == Color::Mode::ANSI)
                fg = win->fg_color.value;
            else
                fg = screen_convert_colour_to_15_bit(gargoyle_color(win->fg_color));
            if (win->bg_color.mode == Color::Mode::ANSI)
                bg = win->bg_color.value;
            else
                bg = screen_convert_colour_to_15_bit(gargoyle_color(win->bg_color));
            if (!win->style.test(STYLE_REVERSE))
                val = (bg << 8) | fg;
            else
                val = (fg << 8) | bg;
        break;
    case 12: // font number
        val = static_cast<uint16_t>(win->font);
        break;
    case 13: // font size
        val = (letterheight << 8) | letterwidth;
        break;
    case 14: // attributes
        val = win->attribute;
        break;
    case 15: // line count
        val = 0;
        break;
    case 16: // true foreground colour
        val = 0;
        break;
    case 17: // true background colour
        val = 0;
        break;
    default:
        die("unknown window property: %u", static_cast<unsigned int>(zargs[1]));
    }

    fprintf(stderr, "zget_wind_prop: result %d\n", val);

    store(val);
}

void zput_wind_prop()
{
    fprintf(stderr, "zput_wind_prop: win %d prop %d val %d\n", zargs[0], zargs[1], zargs[2]);
}

// This is not correct, because @output_stream does not work as it
// should with a width argument; however, this does print out the
// contents of a table that was sent to stream 3, so it’s at least
// somewhat useful.
//
// Output should be to the currently-selected window, but since V6 is
// only marginally supported, other windows are not active. Send to the
// main window for the time being.
void zprint_form()
{
    fprintf(stderr, "zprint_form:  %d\n", zargs[0]);

//    Window *saved = curwin;
//
//    curwin = mainwin;
#ifdef ZTERP_GLK
//    glk_set_window(mainwin->id);
#endif

    for (uint16_t i = 0; i < user_word(zargs[0]); i++) {
        put_char(user_byte(zargs[0] + 2 + i));
    }

    put_char(ZSCII_NEWLINE);

//    curwin = saved;
//#ifdef ZTERP_GLK
//    glk_set_window(curwin->id);
//#endif
}

void zmake_menu()
{
    branch_if(false);
}

void zbuffer_screen()
{
    store(0);
}

#ifdef GLK_MODULE_GARGLKTEXT
// Glk does not guarantee great control over how various styles are
// going to look, but Gargoyle does. Abusing the Glk “style hints”
// functions allows for quite fine-grained control over style
// appearance. First, clear the (important) attributes for each style,
// and then recreate each in whatever mold is necessary. Re-use some
// that are expected to be correct (emphasized for italic, subheader for
// bold, and so on).
static void set_default_styles()
{
    std::array<int, 7> styles = { style_Subheader, style_Emphasized, style_Alert, style_Preformatted, style_User1, style_User2, style_Note };

    for (const auto &style : styles) {
        glk_stylehint_set(wintype_AllTypes, style, stylehint_Weight, 0);
        glk_stylehint_set(wintype_AllTypes, style, stylehint_Oblique, 0);

        // This sets wintype_TextGrid to be proportional, which of course is
        // wrong; but text grids are required to be fixed, so Gargoyle
        // simply ignores this hint for those windows.
        glk_stylehint_set(wintype_AllTypes, style, stylehint_Proportional, 1);
    }
}
#endif

bool create_mainwin()
{
#ifdef ZTERP_GLK

#ifdef GLK_MODULE_UNICODE
    have_unicode = glk_gestalt(gestalt_Unicode, 0);
#endif

#ifdef GLK_MODULE_GARGLKTEXT
    set_default_styles();

    glk_stylehint_set(wintype_AllTypes, GStyleBold, stylehint_Weight, 1);

    glk_stylehint_set(wintype_AllTypes, GStyleItalic, stylehint_Oblique, 1);

    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalic, stylehint_Weight, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalic, stylehint_Oblique, 1);

    glk_stylehint_set(wintype_AllTypes, GStyleFixed, stylehint_Proportional, 0);

    glk_stylehint_set(wintype_AllTypes, GStyleBoldFixed, stylehint_Weight, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleBoldFixed, stylehint_Proportional, 0);

    glk_stylehint_set(wintype_AllTypes, GStyleItalicFixed, stylehint_Oblique, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleItalicFixed, stylehint_Proportional, 0);

    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalicFixed, stylehint_Weight, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalicFixed, stylehint_Oblique, 1);
    glk_stylehint_set(wintype_AllTypes, GStyleBoldItalicFixed, stylehint_Proportional, 0);
#endif

    mainwin->id = glk_window_open(nullptr, 0, 0, wintype_TextBuffer, 1);
    if (mainwin->id == nullptr) {
        return false;
    }
    max_zpos++;
    mainwin->zpos = max_zpos;
    glk_set_window(mainwin->id);

#ifdef GLK_MODULE_LINE_ECHO
    mainwin->has_echo = glk_gestalt(gestalt_LineInputEcho, 0);
    if (mainwin->has_echo) {
        glk_set_echo_line_event(mainwin->id, 0);
    }
#endif

    return true;
#else
    return true;
#endif
}

bool create_statuswin()
{
#ifdef ZTERP_GLK
    statuswin.id = glk_window_open(mainwin->id, winmethod_Above | winmethod_Fixed, 1, wintype_TextGrid, 0);
    fprintf(stderr, "create_statuswin: created grid window with peer %d\n", statuswin.id->peer);
    return statuswin.id != nullptr;
#else
    return false;
#endif
}

bool create_upperwin()
{
#ifdef ZTERP_GLK
    // The upper window appeared in V3. */
    if (zversion >= 3) {
        upperwin->id = glk_window_open(mainwin->id, winmethod_Above | winmethod_Fixed, 0, wintype_TextGrid, 0);
        upperwin->x_origin = upperwin->y_origin = 1;
        upperwin->y_size = 0;
        max_zpos++;
        upperwin->zpos = max_zpos;

        if (upperwin->id != nullptr) {
            glui32 w, h;

            glk_window_get_size(upperwin->id, &w, &h);
            upperwin->x_size = w;

            if (h != 0 || upperwin->x_size == 0) {
                glk_window_close(upperwin->id, nullptr);
                upperwin->id = nullptr;
            }
        }
    }

    if (upperwin->id != nullptr)
        fprintf(stderr, "create_upperwin: created grid window with peer %d\n", upperwin->id->peer);
    if (upperwin != &windows[1])
        fprintf(stderr, "upperwin != &windows[1]\n");
    return upperwin->id != nullptr;
#else
    return false;
#endif
}

// Write out the screen state for a Scrn chunk.
//
// This implements version 0 as described in stack.cpp.
IFF::TypeID screen_write_scrn(IO &io)
{
    io.write32(0);

    io.write8(curwin - windows.data());
#ifdef ZTERP_GLK
    io.write16(upperwin->y_size);
    io.write16(starting_x);
    io.write16(starting_y);
#else
    io.write16(0);
    io.write16(0);
    io.write16(0);
#endif

    for (int i = 0; i < (zversion == 6 ? 8 : 2); i++) {
        io.write8(windows[i].style.to_ulong());
        io.write8(static_cast<uint8_t>(windows[i].font));
        io.write8(static_cast<uint8_t>(windows[i].fg_color.mode));
        io.write16(windows[i].fg_color.value);
        io.write8(static_cast<uint8_t>(windows[i].bg_color.mode));
        io.write16(windows[i].bg_color.value);
    }

    return IFF::TypeID(&"Scrn");
}

static void try_load_color(Color::Mode mode, uint16_t value, Color &color)
{
    switch (mode) {
    case Color::Mode::ANSI:
        if (value >= 1 && value <= 12) {
            color = Color(Color::Mode::ANSI, value);
        }
        break;
    case Color::Mode::True:
        if (value < 32768) {
            color = Color(Color::Mode::True, value);
        }
        break;
    }
}

// Read and restore the screen state from a Scrn chunk. Since this
// actively touches the screen, it should only be called once the
// restore function has determined that the restore is successful.
void screen_read_scrn(IO &io, uint32_t size)
{
    uint32_t version;
    size_t data_size = zversion == 6 ? 71 : 23;
    uint8_t current_window;
    uint16_t new_upper_window_height;
    struct {
        uint8_t style;
        Window::Font font;
        Color::Mode fgmode;
        uint16_t fgvalue;
        Color::Mode bgmode;
        uint16_t bgvalue;
    } window_data[8];
#ifdef ZTERP_GLK
    uint16_t new_x, new_y;
#endif

    try {
        version = io.read32();
    } catch (const IO::IOError &) {
        throw RestoreError("short read");
    }

    if (version != 0) {
        show_message("Unsupported Scrn version %lu; ignoring chunk", static_cast<unsigned long>(version));
        return;
    }

    if (size != data_size + 4) {
        throw RestoreError(fstring("invalid size: %lu", static_cast<unsigned long>(size)));
    }

    try {
        current_window = io.read8();
        new_upper_window_height = io.read16();
#ifdef ZTERP_GLK
        new_x = io.read16();
        new_y = io.read16();
#else
        io.read32();
#endif

        for (int i = 0; i < (zversion == 6 ? 8 : 2); i++) {
            window_data[i].style = io.read8();
            window_data[i].font = static_cast<Window::Font>(io.read8());
            window_data[i].fgmode = static_cast<Color::Mode>(io.read8());
            window_data[i].fgvalue = io.read16();
            window_data[i].bgmode = static_cast<Color::Mode>(io.read8());
            window_data[i].bgvalue = io.read16();
        }
    } catch (const IO::IOError &) {
        throw RestoreError("short read");
    }

    if (current_window > 7) {
        throw RestoreError(fstring("invalid window: %d", current_window));
    }

    set_current_window(&windows[current_window]);
#ifdef ZTERP_GLK
    delayed_window_shrink = -1;
    saw_input = false;
#endif
    resize_upper_window(new_upper_window_height, false);

#ifdef ZTERP_GLK
    if (new_x > upperwin->x_size) {
        new_x = upperwin->x_size;
    }
    if (new_y > upperwin->y_size) {
        new_y = upperwin->y_size;
    }
    if (new_y != 0 && new_x != 0) {
        set_cursor(new_y, new_x, -3);
    }
#endif

    for (int i = 0; i < (zversion == 6 ? 8 : 2); i++) {
        if (window_data[i].style < 16) {
            windows[i].style = window_data[i].style;
        }

        if (is_valid_font(window_data[i].font)) {
            windows[i].font = window_data[i].font;
        }

        try_load_color(window_data[i].fgmode, window_data[i].fgvalue, windows[i].fg_color);
        try_load_color(window_data[i].bgmode, window_data[i].bgvalue, windows[i].bg_color);
    }

    set_current_style();
}

IFF::TypeID screen_write_bfhs(IO &io)
{
    io.write32(0); // version
    io.write32((uint32_t)history.size());

    for (const auto &entry : history.entries()) {
        switch (entry.type) {
        case History::Entry::Type::Style:
            io.write8(static_cast<uint8_t>(entry.type));
            io.write8(entry.contents.style);
            break;
        case History::Entry::Type::FGColor: case History::Entry::Type::BGColor:
            io.write8(static_cast<uint8_t>(entry.type));
            io.write8(static_cast<uint8_t>(entry.contents.color.mode));
            io.write16(entry.contents.color.value);
            break;
        case History::Entry::Type::InputStart: case History::Entry::Type::InputEnd:
            io.write8(static_cast<uint8_t>(entry.type));
            break;
        case History::Entry::Type::Char:
            io.write8(static_cast<uint8_t>(entry.type));
            io.putc(entry.contents.c);
        }
    };

    return IFF::TypeID(&"Bfhs");
}

class ScopeGuard {
public:
    explicit ScopeGuard(std::function<void()> fn) : m_fn(std::move(fn)) {
    }

    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

    ~ScopeGuard() {
        m_fn();
    }

private:
    std::function<void()> m_fn;
};

void screen_read_bfhs(IO &io, bool autosave)
{
    uint32_t version;
    Window saved = *mainwin;
    auto original_style = mainwin->style;
    uint32_t size;
#ifdef ZTERP_GLK
    // Write directly to the main window’s stream instead of going
    // through screen_print() or similar: history playback should be
    // “transparent”, so to speak. It will have already been added to a
    // transcript during the previous round of play (if the user so
    // desired), and this function itself recreates all history directly
    // from the Bfhs chunk.
    strid_t stream = glk_window_get_stream(mainwin->id);
#endif

    try {
        version = io.read32();
    } catch (const IO::IOError &) {
        show_message("Unable to read history version");
        return;
    }

    if (version != 0) {
        show_message("Unsupported history version: %lu", static_cast<unsigned long>(version));
        return;
    }

    mainwin->fg_color = Color();
    mainwin->bg_color = Color();
    mainwin->style.reset();
    set_window_style(mainwin);

#ifdef ZTERP_GLK
    auto write = [&stream](std::string msg) {
        glk_put_string_stream(stream, &msg[0]);
    };
#else
    auto write = [](const std::string &msg) {
        std::cout << msg;
    };
#endif

    ScopeGuard guard([&autosave, &saved, &write] {
        if (!autosave) {
            write("[End of history playback]\n\n");
        }
        *mainwin = saved;
        int fg = get_global(fg_global_idx);
        int bg = get_global(bg_global_idx);
        if (fg == 1) {
            fg = SPATTERLIGHT_CURRENT_FOREGROUND_COLOUR;
            set_global(fg_global_idx, fg);
        }
        if (bg == 1) {
            bg = SPATTERLIGHT_CURRENT_BACKGROUND_COLOUR;
            set_global(bg_global_idx, bg);
        }
        if (zversion == 6 && !is_game(Game::Journey)) {
            user_selected_foreground = zcolor_map[fg];
            user_selected_background = zcolor_map[bg];
            if (fg < 2)
                user_selected_foreground = gfgcol;
            if (bg < 2)
                user_selected_background = gbgcol;
        }
//        set_window_style(mainwin);
        window_change();
    });

    if (!autosave) {
        write("[Starting history playback]\n");
    }

    try {
        size = io.read32();
    } catch (const IO::IOError &) {
        return;
    }

    if (size == 0 && autosave) {
        warning("empty history record");
        screen_print(">");
        return;
    }

    for (uint32_t i = 0; i < size; i++) {
        uint8_t type;
        long c;

        try {
            type = io.read8();
        } catch (const IO::IOError &) {
            return;
        }

        // Each history entry is added to the history buffer. This is so
        // that new saves, after a restore, continue to include the
        // older save file’s history.
        switch (static_cast<History::Entry::Type>(type)) {
        case History::Entry::Type::Style: {
            uint8_t style;

            try {
                style = io.read8();
            } catch (const IO::IOError &) {
                return;
            }

            mainwin->style = style;
            set_window_style(mainwin);

            history.add_style();

            break;
        }
        case History::Entry::Type::FGColor: case History::Entry::Type::BGColor: {
            uint8_t mode;
            uint16_t value;

            try {
                mode = io.read8();
                value = io.read16();
            } catch (const IO::IOError &) {
                return;
            }

            Color &color = static_cast<History::Entry::Type>(type) == History::Entry::Type::FGColor ? mainwin->fg_color : mainwin->bg_color;

            try_load_color(static_cast<Color::Mode>(mode), value, color);
            set_window_style(mainwin);

            if (static_cast<History::Entry::Type>(type) == History::Entry::Type::FGColor) {
                history.add_fg_color(color);
            } else {
                history.add_bg_color(color);
            }

            break;
        }
        case History::Entry::Type::InputStart:
            original_style = mainwin->style;
            mainwin->style.reset();
            set_window_style(mainwin);
#ifdef ZTERP_GLK
            glk_set_style_stream(stream, style_Input);
#endif
            history.add_input_start();
            break;
        case History::Entry::Type::InputEnd:
            mainwin->style = original_style;
            set_window_style(mainwin);
            history.add_input_end();
            break;
        case History::Entry::Type::Char:
            c = io.getc(false);
            if (c == -1) {
                return;
            }
#ifdef ZTERP_GLK
            xglk_put_char_stream(stream, (uint32_t)c);
#else
            IO::standard_out().putc(c);
#endif
            history.add_char((uint32_t)c);
            break;
        default:
            return;
        }
    }
}

IFF::TypeID screen_write_bfts(IO &io)
{
    if (!options.persistent_transcript || perstransio == nullptr) {
        return IFF::TypeID();
    }

    io.write32(0); // Version

    auto buf = perstransio->get_memory();
    io.write_exact(buf.data(), buf.size());

    return IFF::TypeID(&"Bfts");
}

void screen_read_bfts(IO &io, uint32_t size)
{
    uint32_t version;
    std::vector<uint8_t> buf;

    if (size < 4) {
        show_message("Corrupted Bfts entry (too small)");
    }

    try {
        version = io.read32();
    } catch (const IO::IOError &) {
        show_message("Unable to read persistent transcript size from save file");
        return;
    }

    if (version != 0) {
        show_message("Unsupported Bfts version %lu", static_cast<unsigned long>(version));
        return;
    }

    // The size of the transcript is the size of the whole chunk minus
    // the 32-bit version.
    size -= 4;

    if (size > 0) {
        try {
            buf.resize(size);
        } catch (const std::bad_alloc &) {
            show_message("Unable to allocate memory for persistent transcript");
            return;
        }

        try {
            io.read_exact(buf.data(), size);
        } catch (const IO::IOError &) {
            show_message("Unable to read persistent transcript from save file");
            return;
        }
    }

    perstransio = std::make_unique<IO>(buf, IO::Mode::WriteOnly);

    try {
        perstransio->seek(0, IO::SeekFrom::End);
    } catch (const IO::IOError &) {
        perstransio.reset();
    }
}

static std::unique_ptr<std::vector<uint8_t>> perstrans_backup;

static void perstrans_stash_backup()
{
    perstrans_backup.reset();

    if (options.persistent_transcript && perstransio != nullptr) {
        auto buf = perstransio->get_memory();
        try {
            perstrans_backup = std::make_unique<std::vector<uint8_t>>(buf);
        } catch (const std::bad_alloc &) {
        }
    }
}

// Never fail: this is an optional chunk so isn’t fatal on error.
static bool perstrans_stash_restore()
{
    if (perstrans_backup == nullptr) {
        return true;
    }

    perstransio = std::make_unique<IO>(*perstrans_backup, IO::Mode::WriteOnly);

    try {
        perstransio->seek(0, IO::SeekFrom::End);
    } catch (const IO::IOError &) {
        perstransio.reset();
    }

    perstrans_backup.reset();

    return true;
}

static void perstrans_stash_free()
{
    perstrans_backup.reset();
}

void screen_save_persistent_transcript()
{
    if (!options.persistent_transcript) {
        screen_puts("[Persistent transcripting is turned off]");
        return;
    }

    if (perstransio == nullptr) {
        screen_puts("[Persistent transcripting failed to start]");
        return;
    }

    auto buf = perstransio->get_memory();

    try {
        IO io(nullptr, IO::Mode::WriteOnly, IO::Purpose::Transcript);

        try {
            io.write_exact(buf.data(), buf.size());
            screen_puts("[Saved]");
        } catch (const IO::IOError &) {
            screen_puts("[Error writing transcript to file]");
        }
    } catch (const IO::OpenError &) {
        screen_puts("[Failed to open file]");
        return;
    }
}

void init_screen(bool first_run)
{
    zcolor_map[13] = gfgcol;
    zcolor_map[14] = gbgcol;

    user_selected_foreground = gfgcol;
    user_selected_background = gbgcol;

    if (gli_z6_colorize) {
        monochrome_black = darkest(gfgcol, gbgcol);
        monochrome_white = brightest(gfgcol, gbgcol);
    }

    int i = 0;
    for (auto &window : windows) {
        window.index = i++;
        window.style.reset();
        //        if (is_game(Game::Shogun)) {
        if (!(zcolor_map[SPATTERLIGHT_CURRENT_FOREGROUND_COLOUR] == gfgcol && zcolor_map[SPATTERLIGHT_CURRENT_BACKGROUND_COLOUR] == gbgcol)) {
            window.fg_color = Color();
            window.bg_color = Color();
        } else {
            window.fg_color = Color(Color::Mode::ANSI, SPATTERLIGHT_CURRENT_FOREGROUND_COLOUR);
            window.bg_color = Color(Color::Mode::ANSI, SPATTERLIGHT_CURRENT_BACKGROUND_COLOUR);
        }
//        } else {
//            window.fg_color = window.bg_color = Color();
//        }
        window.font = Window::Font::Normal;
        window.y_cursor = 1;
        window.x_cursor = 1;
        window.x_origin = 1;
        window.y_origin = 1;
        window.left = 0;
        window.right = 0;
        window.x_size = gscreenw;
        fprintf(stderr, "Setting x_size of window %d to gscreenw, %d\n", window.index, gscreenw);
        window.y_size = gscreenh;

#ifdef ZTERP_GLK
        clear_window(&window);
#endif
    }

    if (zversion != 6)
        close_upper_window();

    if (zversion == 6) {
        adjust_image_scale();
        glk_stylehint_clear(wintype_TextBuffer, style_User2, stylehint_Oblique);
        if (!is_game(Game::ZorkZero)) {
            glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_Justification, stylehint_just_Centered);
            glk_stylehint_set(wintype_TextBuffer, style_User2, stylehint_Justification, stylehint_just_Centered);
            glk_stylehint_clear(wintype_TextBuffer, style_User1, stylehint_Proportional);
            glk_stylehint_clear(wintype_TextBuffer, style_User2, stylehint_Proportional);
        }

        if (graphics_type == kGraphicsTypeAmiga) {
            int width;
            get_image_size(1, &width, nullptr);
            fprintf(stderr, "Width of image 1: %d\n", width);
            if ((is_game(Game::Arthur) && width == 436) ||  (is_game(Game::ZorkZero) && width == 480) || (is_game(Game::Journey) && width == 166) || (is_game(Game::Shogun) && width == 479)) {
                graphics_type = kGraphicsTypeMacBW;
                hw_screenwidth = 480;
                for (int i = 0; i < image_count; i++) {
                    raw_images[i].type = kGraphicsTypeMacBW;
                }
            }
        }

        if (first_run)
            last_z6_preferred_graphics = gli_z6_graphics;

        // style_User1
        //    glk_stylehint_set(wintype_AllTypes, GStyleBoldFixed, stylehint_Weight, 1);
        //    glk_stylehint_set(wintype_AllTypes, GStyleBoldFixed, stylehint_Proportional, 0);
        //
        // style_User2
        //    glk_stylehint_set(wintype_AllTypes, GStyleItalicFixed, stylehint_Oblique, 1);
        //    glk_stylehint_set(wintype_AllTypes, GStyleItalicFixed, stylehint_Proportional, 0);


        if (!is_game(Game::Journey)) {
            if (first_run) {
                if (mainwin->id) {
                    glk_window_close(mainwin->id, nullptr);
                    mainwin->id = nullptr;
                }
                if (upperwin->id) {
                    glk_window_close(upperwin->id, nullptr);
                    upperwin->id = nullptr;
                }
            }

            for (int i = 0; i < 8; i++) {
                v6_delete_win(&windows[i]);
            }

            if (graphics_win_glk != nullptr) {
                glk_window_close(graphics_win_glk, nullptr);
            }
            graphics_win_glk = glk_window_open(0, 0, 0, wintype_Graphics, 0);
            current_graphics_buf_win = graphics_win_glk;
            graphics_win_glk->bbox.x0 = 0;
            graphics_win_glk->bbox.y0 = 0;
            graphics_win_glk->bbox.x1 = gscreenw;
            graphics_win_glk->bbox.y1 = gscreenh;
            mainwin->id = v6_new_glk_window(wintype_TextBuffer, 0);
            glk_set_echo_line_event(mainwin->id, 0);
            v6_sizewin(mainwin);
        } else if (first_run) { // If this is Journey
            if (mainwin->id) {
                glk_window_close(mainwin->id, 0);
                mainwin->id = nullptr;
            }
            if (upperwin->id == nullptr)
                upperwin->id = glk_window_open(0, 0, 0, wintype_TextGrid, 0);
            upperwin->x_origin = 1;
            upperwin->y_origin = 1;
            upperwin->x_size = gscreenw;
            upperwin->y_size = gscreenh;
            v6_sizewin(upperwin);
            mainwin->id = v6_new_glk_window(wintype_TextBuffer, 0);
            mainwin->x_origin = 1;
            mainwin->y_origin = 1;
            mainwin->x_size = gscreenw;
            mainwin->y_size = gscreenh;
            v6_sizewin(mainwin);

            fprintf(stderr, "Screen height (pixels)%d letterheight: %d screen height / letterheight: %d\n", gscreenh, letterheight, gscreenh / letterheight);
            //        fprintf(stderr, "Screen width (pixels)%d letterwidth: %d screen width / letterwidth: %d\n", gscreenw, letterwidth, gscreenw / letterwidth);
        }

        if (is_game(Game::Arthur)) {
            screenmode = MODE_SLIDESHOW;
            adjust_arthur_top_margin();
        } else if (is_game(Game::Journey)) {
            win_menuitem(kJMenuTypeDeleteAll, 0, 0, false, nullptr, 15);
            screenmode = MODE_INITIAL_QUESTION;
            v6_delete_win(&windows[3]);
        } else {
            screenmode = MODE_NORMAL;
        }
    }

#ifdef ZTERP_GLK
    if (statuswin.id != nullptr) {
        glk_window_clear(statuswin.id);
    }

    if (errorwin != nullptr) {
        if (zversion != 6) {
            glk_window_close(errorwin, nullptr);
        } else {
            gli_delete_window(errorwin);
        }
        errorwin = nullptr;
    }

    stop_timer();

#else
    have_unicode = true;
#endif

    if (first_run && options.persistent_transcript) {
        try {
            perstransio = std::make_unique<IO>(std::vector<uint8_t>(), IO::Mode::WriteOnly);
        } catch (const IO::OpenError &) {
            warning("Failed to start persistent transcripting");
        }

        stash_register(perstrans_stash_backup, perstrans_stash_restore, perstrans_stash_free);
    }

    // On restart, deselect stream 3 and select stream 1. This allows
    // the command script and transcript to persist across restarts,
    // while resetting memory output and ensuring screen output.
    streams.reset(STREAM_MEMORY);
    streams.set(STREAM_SCREEN);
    stream_tables.clear();

    set_current_window(mainwin);
    glk_set_echo_line_event(mainwin->id, 0);
}

#ifdef SPATTERLIGHT
// This is called during an autosave. It saves the relations
// between Bocfel specific structures and Glk objects, and also
// any active sound commands.
void stash_library_state(library_state_data *dat)
{
//    if (dat) {
//        if ( windows[0].id)
//            dat->wintag0 = windows[0].id->tag;
//        if ( windows[1].id)
//            dat->wintag1 = windows[1].id->tag;
//        if ( windows[2].id)
//            dat->wintag2 = windows[2].id->tag;
//        if ( windows[3].id)
//            dat->wintag3 = windows[3].id->tag;
//        if ( windows[4].id)
//            dat->wintag4 = windows[4].id->tag;
//        if ( windows[5].id)
//            dat->wintag5 = windows[5].id->tag;
//        if ( windows[6].id)
//            dat->wintag6 = windows[6].id->tag;
//        if ( windows[7].id)
//            dat->wintag7 = windows[7].id->tag;
//
//        if (curwin->id)
//            dat->curwintag = curwin->id->tag;
//        if (mainwin->id)
//            dat->mainwintag = mainwin->id->tag;
//        if (statuswin.id)
//            dat->statuswintag = statuswin.id->tag;
//        if (errorwin && errorwin->tag)
//            dat->errorwintag = errorwin->tag;
//        if (upperwin->id)
//            dat->upperwintag = upperwin->id->tag;
//
//        dat->last_random_seed = last_random_seed;
//        dat->random_calls_count = random_calls_count;
//
//        stash_library_sound_state(dat);
//    }
}

// This is called during an autorestore. It recreatets the relations
// between Bocfel specific structures and Glk objects, and any
// active sound commands.
void recover_library_state(library_state_data *dat)
{
    if (dat) {
        windows[0].id = gli_window_for_tag(dat->wintag0);
        windows[1].id = gli_window_for_tag(dat->wintag1);
        windows[2].id = gli_window_for_tag(dat->wintag2);
        windows[3].id = gli_window_for_tag(dat->wintag3);
        windows[4].id = gli_window_for_tag(dat->wintag4);
        windows[5].id = gli_window_for_tag(dat->wintag5);
        windows[6].id = gli_window_for_tag(dat->wintag6);
        windows[7].id = gli_window_for_tag(dat->wintag7);
        statuswin.id = gli_window_for_tag(dat->statuswintag);
        errorwin = gli_window_for_tag(dat->errorwintag);
        for (int i = 0; i < 8; i++)
        {
            if (windows[i].id) {
                if (windows[i].id->tag == dat->mainwintag) {
                    mainwin = &windows[i];
                }
                if (windows[i].id->tag == dat->curwintag) {
                    curwin = &windows[i];
                }
                if (windows[i].id->tag == dat->upperwintag)
                {
                    upperwin = &windows[i];
                    if (mouse_available()) {
                        if (mousewin && mousewin->id) {
                            glk_request_mouse_event(mousewin->id);
                        } else {
                            glk_request_mouse_event(curwin->id);
                            glk_request_mouse_event(upperwin->id);
                            if (is_game(Game::ZorkZero) && screenmode == MODE_MAP) {
                                glk_request_mouse_event(current_graphics_buf_win);
                            }
                        }
                    }
                }
            }
        }

        seed_random((uint32_t)last_random_seed);
        random_calls_count = 0;
        for (int i = 0; i < dat->random_calls_count; i++)
            zterp_rand();

        recover_library_sound_state(dat);
    }
}
#endif
