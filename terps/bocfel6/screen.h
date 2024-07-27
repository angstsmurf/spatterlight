// vim: set ft=cpp:

#ifndef ZTERP_SCREEN_H
#define ZTERP_SCREEN_H

#include <bitset>
#include <string>

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#endif

#include "iff.h"
#include "io.h"
#include "types.h"
#include "util.h"
#include "image.h"

// Represents a Z-machine color.
//
// If mode is ANSI, value is a color in the range [1, 12], representing
// the colors as described in §8.3.1.
//
// If mode is True, value is a 15-bit color as described in §8.3.7 and
// §15.
struct Color {
    enum class Mode { ANSI, True } mode;
    uint16_t value;

    explicit Color() : mode(Mode::ANSI), value(1) {
    }

    Color(Mode mode_, uint16_t value_) : mode(mode_), value(value_) {
    }
};

void init_screen(bool first_run);

bool create_mainwin();
bool create_statuswin();
bool create_upperwin();
void get_screen_size(unsigned int &width, unsigned int &height);
void close_upper_window();

uint32_t screen_convert_color(uint16_t color);

// Text styles.
using Style = std::bitset<4>;

enum StyleBit {
    STYLE_REVERSE = 0,
    STYLE_BOLD    = 1,
    STYLE_ITALIC  = 2,
    STYLE_FIXED   = 3,
};

enum AnsiColour {
    DEFAULT_COLOUR = 1,
    BLACK_COLOUR = 2,
    RED_COLOUR = 3,
    GREEN_COLOUR = 4,
    YELLOW_COLOUR = 5,
    BLUE_COLOUR = 6,
    MAGENTA_COLOUR = 7,
    CYAN_COLOUR = 8,
    WHITE_COLOUR = 9,
    GREY_COLOUR = 10,        /* INTERP_MSDOS only */
    LIGHTGREY_COLOUR = 10,     /* INTERP_AMIGA only */
    MEDIUMGREY_COLOUR = 11,     /* INTERP_AMIGA only */
    DARKGREY_COLOUR = 12,     /* INTERP_AMIGA only */
    SPATTERLIGHT_CURRENT_FOREGROUND = 13,
    SPATTERLIGHT_CURRENT_BACKGROUND = 14,
    TRANSPARENT_COLOUR = 15,    /* ZSpec 1.1 */
};

enum interpreterNumber {
    INTERP_DEFAULT = 0,
    INTERP_DEC_20 = 1,
    INTERP_APPLE_IIE = 2,
    INTERP_MACINTOSH = 3,
    INTERP_AMIGA = 4,
    INTERP_ATARI_ST = 5,
    INTERP_MSDOS = 6,
    INTERP_CBM_128 = 7,
    INTERP_CBM_64 = 8,
    INTERP_APPLE_IIC = 9,
    INTERP_APPLE_IIGS = 10,
    INTERP_TANDY = 11
};

#define GLOBAL(X)   (header.globals + X * 2)
#define get_global(X)    word(GLOBAL(X))
#define set_global(X, Y)    store_word(GLOBAL(X), Y)



zprintflike(1, 2)
void show_message(const char *fmt, ...);
void screen_print(const std::string &s);
zprintflike(1, 2)
void screen_printf(const char *fmt, ...);
void screen_puts(const std::string &s);
void screen_message_prompt(const std::string &message);
void screen_flush();

#ifdef ZTERP_GLK
void term_keys_reset();
void term_keys_add(uint8_t key);
#endif

#ifdef GLK_MODULE_GARGLKTEXT
void update_color(int which, unsigned long color);
#endif

// Output streams.
constexpr uint16_t OSTREAM_SCREEN = 1;
constexpr uint16_t OSTREAM_SCRIPT = 2;
constexpr uint16_t OSTREAM_MEMORY = 3;
constexpr uint16_t OSTREAM_RECORD = 4;

// Input streams.
constexpr int ISTREAM_KEYBOARD = 0;
constexpr int ISTREAM_FILE     = 1;

void screen_set_header_bit(bool set);

bool output_stream(int16_t number, uint16_t table);
bool input_stream(int which);

int print_handler(uint32_t addr, void (*outc)(uint8_t));
void put_char(uint8_t c);

std::string screen_format_time(long hours, long minutes);
void screen_read_scrn(IO &io, uint32_t size);
IFF::TypeID screen_write_scrn(IO &io);
void screen_read_bfhs(IO &io, bool autosave);
IFF::TypeID screen_write_bfhs(IO &io);
void screen_read_bfts(IO &io, uint32_t size);
IFF::TypeID screen_write_bfts(IO &io);
void screen_save_persistent_transcript();

void set_current_style(void);
void update_user_defined_colors(void);



void zoutput_stream();
void zinput_stream();
void zprint();
void znew_line();
void zprint_ret();
void zerase_window();
void zerase_line();
void zset_cursor();
void zget_cursor();
void zset_colour();
void zset_true_colour();
void zmove_window();
void zwindow_size();
void zwindow_style();
void zset_text_style();
void zset_font();
void zdraw_picture();
void zerase_picture();
void zset_margins();
void zprint_table();
void zprint_char();
void zprint_num();
void zprint_addr();
void zprint_paddr();
void zsplit_window();
void zset_window();
void zread_char();
void zread_mouse();
void zmouse_window();
void zshow_status();
void zread();
void zprint_unicode();
void zcheck_unicode();
void zpicture_data();
void zget_wind_prop();
void zput_wind_prop();
void zprint_form();
void zmake_menu();
void zbuffer_screen();

extern GraphicsType graphics_type;
extern bool centeredText;

struct Window {
    Style style;
    //    Color fg_color = Color(), bg_color = Color();
    Color fg_color = Color(Color::Mode::ANSI, 13), bg_color = Color(Color::Mode::ANSI, 14);
    enum class Font { Query, Normal, Picture, Character, Fixed } font = Font::Normal;

#ifdef ZTERP_GLK
    winid_t id = nullptr;
    long x_origin = 1, y_origin = 1;
    bool has_echo = false;

    uint16_t y_size;
    uint16_t x_size;
    uint16_t y_cursor = 1;
    uint16_t x_cursor = 1;
    uint16_t left;
    uint16_t right;
    uint16_t nl_routine;
    uint16_t nl_countdown;
    uint16_t font_size;
    uint16_t attribute;
    uint16_t line_count;
    uint16_t true_fore;
    uint16_t true_back;
    uint16_t index;
//    int zpos;
    uint16_t last_click_x;
    uint16_t last_click_y;
    uint16_t lastline[512];
#endif
};

void v6_sizewin(Window *win);
void v6_delete_win(Window *win);
winid_t v6_new_glk_window(glui32 type);
void set_cursor(uint16_t y, uint16_t x, uint16_t winid);
void set_current_window(Window *window);
void window_change(void);
void transcribe(uint32_t c);
uint8_t read_char(void);
int count_characters_in_zstring(uint16_t str);
int count_characters_in_object(uint16_t obj);

glui32 darkest(glui32 col1, glui32 col2);
glui32 brightest(glui32 col1, glui32 col2);

void v6_restore_hacks(void);
void v6_remap_win_to_buffer(Window *win);
void v6_remap_win_to_grid(Window *win);
//bool is_win_covered(Window *win, int zpos);
void flush_image_buffer(void);
glui32 rgb_colour_from_index(uint16_t index);
void v6_delete_glk_win(winid_t glkwin);
void v6_define_window(Window *win, uint16_t x, uint16_t y, uint16_t width, uint16_t height);

enum V6ScreenMode {
    MODE_NORMAL,
    MODE_SLIDESHOW,
    MODE_MAP,
    MODE_INVENTORY,
    MODE_STATUS,
    MODE_ROOM_DESC,
    MODE_NO_GRAPHICS,
    MODE_Z0_GAME,
    MODE_HINTS,
    MODE_CREDITS,
    MODE_DEFINE,
    MODE_SHOGUN_MENU,
    MODE_SHOGUN_MAZE,
    MODE_INITIAL_QUESTION
};

extern V6ScreenMode screenmode;
extern winid_t current_graphics_buf_win;

extern std::array<Window, 8> windows;
extern uint16_t letterwidth;
extern uint16_t letterheight;
extern winid_t graphics_win_glk;
extern glui32 current_picture;
extern glui32 user_selected_foreground, user_selected_background;

extern int buffer_xpos;

#endif
