//
//  journey.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

extern "C" {
#include "glk.h"
#include "glkimp.h"
}

#include "screen.h"
#include "zterp.h"
#include "memory.h"
#include "objects.h"
#include "process.h"
#include "unicode.h"
#include "dict.h"

#include "draw_image.hpp"
#include "journey.hpp"

extern Window *curwin, *mainwin;

enum StampBits {
  kStampRightBit = 1,
  kStampBottomBit = 2
};

static int journey_image_x, journey_image_y, journey_image_width, journey_image_height;

static float journey_image_scale = 1.0;

glui32 SCREEN_WIDTH_in_chars, SCREEN_HEIGHT_in_chars;

void adjust_journey_image(int picnum, int *x, int *y, int width, int height, int winwidth, int winheight, float *scale, float pixwidth) {

    journey_image_width = width;
    journey_image_height = height;
    journey_image_scale = (float)winwidth / ((float)width * (float)pixwidth) ;

    if (height * journey_image_scale > winheight) {
        journey_image_scale = (float)winheight / height;
        journey_image_x = (winwidth - width * journey_image_scale) / 2;
        journey_image_y = 0;
    } else {
        journey_image_x = 0;
        journey_image_y = (winheight - height * journey_image_scale) / 2;
    }
    
    *scale = journey_image_scale;

    *x = journey_image_x;
    *y = journey_image_y;
}

void draw_journey_stamp_image(winid_t win, int16_t picnum, int16_t where, float pixwidth) {

    int stamp_offset_x = 0, stamp_offset_y = 0, width, height;

    if (where > 0) {
        get_image_size(where, &width, &height);
        stamp_offset_x = width;
        stamp_offset_y = height;
    } else {
        get_image_size(picnum, &width, &height);
        if ((where & kStampRightBit) != 0) {
            stamp_offset_x = journey_image_width - width;
        }

        if ((where & kStampBottomBit) != 0) {
            stamp_offset_y = journey_image_height - height;
        }
    }

    int stamp_x = journey_image_x + stamp_offset_x * journey_image_scale * pixwidth;
    int stamp_y = journey_image_y + stamp_offset_y * journey_image_scale;

    draw_inline_image(win, picnum, stamp_x, stamp_y, journey_image_scale, false);
}


void draw_journey_title_image(void) {
    float scale;
    int x, y, width, height;
    winid_t win = windows[3].id;
    if (win == nullptr || win->type != wintype_Graphics) {
        fprintf(stderr, "draw_journey_title_image: glk window error!\n");
        return;
    }
    glk_window_set_background_color(win, monochrome_black);
    glk_window_clear(win);
    get_image_size(160, &width, &height);
    adjust_journey_image(160, &x, &y, width, height, gscreenw, gscreenh, &scale, pixelwidth);
    draw_inline_image(win, 160, x, y, scale, false);
}




int16_t selected_journey_line = -1;
int16_t selected_journey_column = -1;


#define THICK_V_LINE 57
#define THIN_V_LINE 41
#define H_LINE 39


void FONT3_LINE(int LN, int CHR, int L, int R) {
    glk_set_style(style_BlockQuote);
    glk_window_move_cursor(curwin->id, 0, LN - 1);
    glk_put_char(L);
    for (int i = 1; i < SCREEN_WIDTH_in_chars - 1; i++)
        glk_put_char(CHR);
    glk_put_char(R);
    glk_set_style(style_Normal);

}

int refresh_journey_character_command_area(int LN);

void SETUP_WINDOWS(void) {
    int PW = get_global(0xa9); // PW = BASE-PICTURE-WIDTH
    int PH = get_global(0x27); // PH = BASE-PICTURE-HEIGHT
    int OFF = 6;
    int TEXT_WINDOW_LEFT = get_global(0x38);
    bool GOOD = false;

    bool APPLE2 = (options.int_number == INTERP_APPLE_IIC || options.int_number == INTERP_APPLE_IIE || options.int_number == INTERP_APPLE_IIGS);

    if (!APPLE2 || PH == 0) {
        int width, height;
        GOOD = get_image_size(2, &width, &height);

        PH = round((float)height * imagescaley);
        PW = round((float)width * imagescalex);
        set_global(0xa9, PW);
        set_global(0x27, PH);
    } else {
        GOOD = true;
    }

    if (GOOD) {
        if (APPLE2 || options.int_number == INTERP_MSDOS) {
            OFF = 3;
        }
        if (options.int_number != INTERP_AMIGA) {
            OFF = 5;
        }
        TEXT_WINDOW_LEFT = OFF + (PW + letterwidth) / letterwidth;

    } else {
        TEXT_WINDOW_LEFT = 32; // constant INIT-TEXT-LEFT
    }

    set_global(0x38, TEXT_WINDOW_LEFT);
}

bool qset(uint16_t obj, int16_t bit) { // Test attribute, set it, return test result.
    bool result = internal_test_attr(obj, bit);
    internal_set_attr(obj, bit);
    return result;
}

void adjust_journey_windows(bool restoring);

void update_screen_size(void) {
    glk_window_get_size(windows[1].id, &SCREEN_WIDTH_in_chars, &SCREEN_HEIGHT_in_chars);

    set_global(0x83, SCREEN_WIDTH_in_chars); // GLOBAL SCREEN-WIDTH
    set_global(0x64, SCREEN_HEIGHT_in_chars); // GLOBAL SCREEN-HEIGHT
}

bool BORDER_FLAG, FONT3_FLAG;

void update_internal_globals(void) {

    int BLACK_PICTURE_BORDER;
    int FWC_FLAG;

    switch (options.int_number ) {
        case INTERP_MSDOS:
            BORDER_FLAG = false;
            FONT3_FLAG = false;
            BLACK_PICTURE_BORDER = 0;
            break;
        case INTERP_MACINTOSH:
            BORDER_FLAG = false;
            FONT3_FLAG = true;
            BLACK_PICTURE_BORDER = 1;
            break;
        case INTERP_AMIGA:
            BORDER_FLAG = true;
            FONT3_FLAG = true;
            BLACK_PICTURE_BORDER = 1;
            break;
        case INTERP_APPLE_IIC:
        case INTERP_APPLE_IIE:
        case INTERP_APPLE_IIGS:
            BORDER_FLAG = true;
            FONT3_FLAG = false;
            BLACK_PICTURE_BORDER = 0;
            break;
        default: // Should never happen
            fprintf(stderr, "Unsupported interpreter option!\n");
            exit(1);
    }

    // FWC-FLAG tells whether to switch font when printing in command window, i.e. whether there is a separate proportional font used elsewhere.
    // Always the same as FONT3-FLAG, and used interchangably in the original code
    FWC_FLAG = FONT3_FLAG;

    // Whether there is a border around the screen, true on Amiga only
    set_global(0x9f, BORDER_FLAG ? 1 : 0);

    // Whether to use Font 3. True for Amiga and Mac, false for MS DOS and Apple II
    set_global(0x31, FONT3_FLAG ? 1 : 0);
    set_global(0xb9, FWC_FLAG);
    // Whether to have a border around the image. BLACK_PICTURE_BORDER is only false on IBM PC
    set_global(0x72, BLACK_PICTURE_BORDER);

    update_screen_size();

    //  COMMAND_START_LINE is first line below divider: SCREEN_HEIGHT_in_chars - 4 on non-Amiga, SCREEN_HEIGHT_in_chars - 5 on Amiga

    int TOP_SCREEN_LINE, COMMAND_START_LINE;

    if (!BORDER_FLAG) {
        TOP_SCREEN_LINE = 1;
        COMMAND_START_LINE = SCREEN_HEIGHT_in_chars - 4;
    } else {
        TOP_SCREEN_LINE = 2;
        COMMAND_START_LINE = (SCREEN_HEIGHT_in_chars - 5);
    }

    set_global(0x25, TOP_SCREEN_LINE);
    set_global(0x0e, COMMAND_START_LINE);

    // Width of a column (except Name column) in characters
    int COMMAND_WIDTH = SCREEN_WIDTH_in_chars / 5;
    set_global(0xb8, COMMAND_WIDTH);

    // Width of column in pixels. Used in ERASE-COMMAND
    int COMMAND_WIDTH_PIX = (COMMAND_WIDTH - 1) * letterwidth;
    set_global(0x6d, COMMAND_WIDTH_PIX);

    int NAME_WIDTH = COMMAND_WIDTH + SCREEN_WIDTH_in_chars % 5;
    set_global(0xa2, NAME_WIDTH);

    int NAME_WIDTH_PIX = (NAME_WIDTH - 1) * letterwidth;
    set_global(0x82, NAME_WIDTH_PIX);

    bool APPLE2 = (options.int_number == INTERP_APPLE_IIC || options.int_number == INTERP_APPLE_IIE || options.int_number == INTERP_APPLE_IIGS);

    int PARTY_COMMAND_COLUMN;

    if (APPLE2) {
        PARTY_COMMAND_COLUMN = 1;
    } else {
        PARTY_COMMAND_COLUMN = 2;
    }
    set_global(0xb4, PARTY_COMMAND_COLUMN);


    // NAME_COLUMN: Position of second column in characters (1-based)
    int NAME_COLUMN = PARTY_COMMAND_COLUMN + COMMAND_WIDTH;
    set_global(0xa3, NAME_COLUMN);

    int CHR_COMMAND_COLUMN = NAME_COLUMN + NAME_WIDTH;
    set_global(0x28, CHR_COMMAND_COLUMN);

    int COMMAND_OBJECT_COLUMN = CHR_COMMAND_COLUMN + COMMAND_WIDTH;
    set_global(0xb0, COMMAND_OBJECT_COLUMN);
//    int RIGHT_COLUMN_WIDTH = (SCREEN_WIDTH_in_chars - BORDER_FLAG) * letterwidth - RIGHT_COLUMN_LEFT_EDGE;

//    set_global(0x0, RIGHT_COLUMN_WIDTH);
//    set_global(0x15, RIGHT_COLUMN_LEFT_EDGE);

//    int LONG_ARROW_WIDTH = 2.0 + 3.0 * gcellw;
//    int SHORT_ARROW_WIDTH = 2.0 + 2.0 * gcellw;
//    int NO_ARROW_WIDTH = 2.0 + 1.0 * gcellw;
//
//    set_global(0x7a, LONG_ARROW_WIDTH);
//    set_global(0x76, SHORT_ARROW_WIDTH);
//    set_global(0x60, NO_ARROW_WIDTH);
}

int party_pcm(int chr) {
    uint16_t party_table = get_global(0x63); // global 0x63 is PARTY table
    uint16_t MAX = user_word(party_table);
    for (int cnt = 1; cnt <= MAX; cnt++) {
        if (word(party_table + cnt * 2) == chr)
            return cnt;
    }
    return 0;
}

bool bad_character(uint8_t c, bool elvish) {
    if (c >= 'A' && c <= 'Z')
        return false;
    if (c >= 'a' && c <= 'z')
        return false;
    if (elvish && (c == ' ' || c == '\'' || c == '-' || c == '.' || c == ',' || c == '?'))
        return false;
    return true;
}

enum inputMode {
    INPUT_PARTY,
    INPUT_OBJECT,
    INPUT_SPECIAL,
    INPUT_ELVISH,
    INPUT_NAME,
};

inputMode current_input = INPUT_PARTY;

void underscore_or_square() {
    if (FONT3_FLAG) {
        glk_put_char('_');
    } else {
        glk_put_char(UNICODE_SPACE);
    }
}

uint16_t input_length = 0;
uint16_t max_length = 0;
uint16_t from_command_start_line = 0;
uint16_t input_column = 0;
uint16_t input_line = 0;
uint16_t input_table = 0;

void move_journey_cursor(int column, int line) {
    set_cursor(line * letterwidth, column * letterwidth, 1);

    if (column <= 1)
        column = 0;
    else
        column--;
    if (line <= 1)
        line = 0;
    else
        line--;
    glk_window_move_cursor(windows[1].id, column, line);
}

void debug_print_str(uint8_t c);

void debug_PRINT_STRING(uint16_t str) {
    print_handler(unpack_string(str), debug_print_str);
}

int GET_COMMAND(int cmd);


static char *string_buf_ptr = nullptr;
static int string_buf_pos = 0;
static int string_maxlen = 0;

void print_to_string_buffer(uint8_t c) {

    if (string_buf_pos < string_maxlen)
        string_buf_ptr[string_buf_pos++] = c;
}


int print_zstr_to_cstr(uint16_t addr, char *str) {
    int length = count_characters_in_zstring(addr);
    if (length < 2)
        return 0;
    string_buf_ptr = str;
    string_buf_pos = 0;
    string_maxlen = 15;
    print_handler(unpack_string(addr), print_to_string_buffer);
    str[length] = 0;
    return length;
}


int print_tag_route_to_str(char *str) {
    int name_length = get_global(0x1b);
    int name_table = get_global(0xba) + 4;
    for (int i = 0; i < name_length; i++) {
        str[i] = zscii_to_unicode[user_byte(name_table++)];
    }
    const char *routestring = " route";
    for (int i = 0; i < 6; i++) {
        str[name_length + i] = routestring[i];
    }
    str[name_length + 6] = 0;
    return name_length + 6;
}

void create_journey_party_menu(void) {

    fprintf(stderr, "create_journey_party_menu");
    int object;

    char str[15];
    int stringlength = 0;

    int line = 0;

    int column = 0;
    int table = get_global(0x06); // PARTY-COMMANDS (G06)

    int table_count = user_word(table);

    for (int i = 1; i <= table_count; i++) {
        object = user_word(table + 2 * i);
        if (object == 0x3ea7 // TAG-ROUTE-COMMAND
            && get_global(0x1b) != 0) { // TAG-NAME-LENGTH (G1b)
            stringlength = print_tag_route_to_str(str);
        } else {
            stringlength = print_zstr_to_cstr(user_word(object), str);
        }
        if (stringlength > 1)
            win_menuitem(kJMenuTypeParty, column, line, i == table_count, str, stringlength);
        line++;
    }
}

struct JourneyWords {
    uint16_t pcm;
    uint16_t pcf;
    uint16_t str;
};

static JourneyWords printed_journey_words[4];
static int number_of_printed_journey_words = 0;

struct JourneyMenu {
    char name[15];
    struct JourneyMenu *submenu;
    int submenu_entries;
    int length;
    int line;
    int column;
};

void create_submenu(JourneyMenu *m, int object, int objectindex) {
    uint16_t command_table = word(get_global(0x32) + objectindex * 2);
    m->submenu_entries = 0;
    if (!internal_test_attr(object, 0x17)) { // flag 0x17 is SHADOW flag
        for (int i = 0; i <= 2; i++) {
            uint16_t command = user_word(command_table + i * 2);
            uint16_t str = word(command);
            if (str != 0 && count_characters_in_zstring(str) > 1) {
                if (m->submenu == nullptr) {
                    m->submenu = (struct JourneyMenu *)malloc(sizeof(struct JourneyMenu) * 3);
                }
                struct JourneyMenu *submenu = &(m->submenu[m->submenu_entries]);
                submenu->length = print_zstr_to_cstr(str, submenu->name);
                submenu->line = m->line;
                m->submenu_entries++;
                submenu->column = m->column + m->submenu_entries;
                fprintf(stderr, "verb submenu item %d: \"%s\"\n", m->submenu_entries, submenu->name);
            }
        }
    }
}


void create_journey_menu(JourneyMenuType type, bool prsi) {

    struct JourneyMenu menu[10];

    fprintf(stderr, "create_journey_menu\n");
    int table, table_count;
    if (type == kJMenuTypeObjects) {
        table = get_global(0x14) + (prsi ? 10 : 0);
        table_count = user_word(table);
    } else {
        table = get_global(0x63);
        table_count = 5;
    }

    struct JourneyMenu *m;

    bool subgroup = (get_global(0x80) == 1);
    int menu_counter = 0;
    int submenu_counter = 0;

    for (int i = 1; i <= table_count; i++) {
        uint16_t object = word(table + 2 * i);
        if (object != 0 && !(subgroup && !internal_test_attr(object, 0x2a))) {

            m = &menu[menu_counter];

            int TAG_OBJECT = 0x01a3;
            int TAG = 0x78;
            int TAG_NAME_LENGTH = get_global(0x1b);
            int NAME_TBL = get_global(0xba) + 2;

            if ((object == TAG_OBJECT || object == TAG) && TAG_NAME_LENGTH != 0) {
                for (uint16_t j = 0; j < TAG_NAME_LENGTH; j++) {
                    m->name[j] = user_byte(NAME_TBL++);
                }
                m->name[TAG_NAME_LENGTH] = 0;
                m->length = TAG_NAME_LENGTH;
            } else {
                uint16_t str = internal_get_prop(object, 0x3a);
                m->length = print_zstr_to_cstr(str, m->name);
            }

            m->submenu = nullptr;

            if (m->length > 1) {
                m->line = i - 1;
                if (type == kJMenuTypeMembers) {
                    m->column = 1;
                    create_submenu(m, object, i);
                } else {
                    m->column = 3 + ((menu_counter > 4 || prsi) ? 1 : 0);
                }
                menu_counter++;
            }
        }
    }

    if (type == kJMenuTypeObjects) {
        for (int i = 0; i < number_of_printed_journey_words; i++) {
            JourneyWords *jword = &(printed_journey_words[i]);
            char string[15];
            int len = print_zstr_to_cstr(jword->str, string);
            if (len > 1) {
                fprintf(stderr, "Sending glue menu command %s line %d length %d\n", string, jword->pcm, len);
                win_menuitem(kJMenuTypeGlue, jword->pcf, jword->pcm - 1, 0, string, len);
            }
        }
    }

    for (int i = 0; i < menu_counter; i++) {
        m = &menu[i];
        win_menuitem(type, m->column, m->line, (i == menu_counter - 1), m->name, m->length);

        if (m->submenu != nullptr) {
            for (int j = 0; j < m->submenu_entries; j++) {
                struct JourneyMenu *submenu = &(m->submenu[j]);
                win_menuitem(kJMenuTypeVerbs, submenu->column, submenu->line, (j == submenu_counter - 1), submenu->name, submenu->length);
            }
            free(m->submenu);
            m->submenu = nullptr;
        }
    }
}

void print_input(inputMode mode) {
    switch (mode) {
        case INPUT_PARTY:
            fprintf(stderr, "PARTY");
            break;
        case INPUT_SPECIAL:
            fprintf(stderr, "SPELLS");
            break;
        case INPUT_OBJECT:
            fprintf(stderr, "OBJECT");
            break;
        case INPUT_ELVISH:
            fprintf(stderr, "ELVISH");
            break;
        case INPUT_NAME:
            fprintf(stderr, "NAME");
            break;
        default:
            fprintf(stderr, "(unknown mode)");
            break;
    }
}

void change_input(inputMode mode) {
    if (current_input == mode) {
        fprintf(stderr, "change_input to ");
        print_input(mode);
        fprintf(stderr, ": No change\n");
        return;
    }

    fprintf(stderr, "change_input: changed from ");
    print_input(current_input);
    fprintf(stderr, " to ");
    print_input(mode);
    fprintf(stderr, "\n");
    current_input = mode;
}

extern bool journey_cursor_timer;
extern bool journey_cursor_reverse;

#pragma mark Input

uint16_t journey_read_keyboard_line(int x, int y, uint16_t table, int max, bool elvish, uint8_t *kbd) {

    input_column = x;
    input_line = y;
    input_length = 0;
    max_length = max;
    input_table = table;
    from_command_start_line = y - get_global(0x0e); // COMMAND-START-LINE

    set_current_window(&windows[1]);
    move_journey_cursor(input_column, input_line);

    if (!FONT3_FLAG) {
        garglk_set_reversevideo(1);
    }

    for (int i = 0; i <= max_length; i++) {
        underscore_or_square();
    }

    move_journey_cursor(input_column, input_line);

    int start = input_column;

    win_menuitem(kJMenuTypeTextEntry, x, from_command_start_line, elvish, nullptr, 15);

    uint8_t character;

    // Starting cursor timer.
    journey_cursor_timer = true;
    journey_cursor_reverse = false;
    glk_request_timer_events(10);

    while ((character = read_char()) != ZSCII_NEWLINE) {
        if (FONT3_FLAG) {
            garglk_set_reversevideo(0);
        }
        journey_cursor_reverse = false;
        glk_request_timer_events(10);
        if (character == 0x7f || character == ZSCII_DELETE || character == ZSCII_LEFT) { // DELETE-KEY ,BACK-SPACE ,LEFT-ARROW
            if (input_length == 0) {
                win_beep(1);
                continue;
            } else {
                if (input_column < SCREEN_WIDTH_in_chars - 1) {
                    move_journey_cursor(input_column, input_line);
                    underscore_or_square();
                }
                input_column--;
                input_length--;
                move_journey_cursor(input_column, input_line);
                underscore_or_square();
                move_journey_cursor(input_column, input_line);
            }
        } else {
            
            if (input_length == max_length || bad_character(character, elvish)) {
                win_beep(1);
                continue;
            }
            
            if (!elvish) {
                if (input_length == 0) {
                    if (character >= 'a' && character <= 'z') {
                        character -= 32;
                    }
                    *kbd = character;
                } else if (character >= 'A' && character <= 'Z') {
                    character += 32;
                }
            }

            if (input_column < SCREEN_WIDTH_in_chars) {
                move_journey_cursor(input_column, input_line);
                put_char(character);
            }
            user_store_byte(table + input_length, character);
            input_column++;
            input_length++;
        }
    }

//  Input is finished. Switching off cursor timer and any reverse video.
    journey_cursor_timer = false;
    journey_cursor_reverse = false;
    glk_request_timer_events(0);

    garglk_set_reversevideo(0);

    if (elvish) {
        move_journey_cursor(start, input_line);
        for (int i = 0; i < max_length; i++) {
            put_char(UNICODE_SPACE);
        }
    }

    set_current_window(&windows[0]);
    change_input(INPUT_PARTY);
    return input_length;
}


int GET_COMMAND(int cmd) {
    int COMMAND_WIDTH = get_global(0xb8);
    if (COMMAND_WIDTH < 13) {
        int STR = user_word(cmd + 10);
        if (STR)
            return STR;
    }
    return user_word(cmd);
}

void PRINT_COMMAND(int cmd) {
    print_handler(unpack_string(GET_COMMAND(cmd)), nullptr);
}

void debug_PRINT_COMMAND(uint16_t cmd) {
    print_handler(unpack_string(GET_COMMAND(cmd)), debug_print_str);
}

int GET_DESC(int obj) {
    int STR;
    if (SCREEN_WIDTH_in_chars < 0x32) {
        STR = internal_get_prop(obj, 0x2b);
        if (STR)
            return STR;
    }
    if (SCREEN_WIDTH_in_chars < 0x47) {
        STR = internal_get_prop(obj, 0x29);
        if (STR)
            return STR;
    }
    return (internal_get_prop(obj, 0x3a));
}


int PRINT_DESC(int obj, bool cmd) {
    int NAME_TBL = get_global(0xba) + 2;
    int TAG_NAME_LENGTH = get_global(0x1b);
    int TAG_OBJECT = 0x01a3;
    int TAG = 0x78;

    if ((obj == TAG_OBJECT || obj == TAG) && TAG_NAME_LENGTH != 0) {
        for (uint16_t j = 0; j < TAG_NAME_LENGTH; j++) {
            put_char(user_byte(NAME_TBL++));
        }
        return TAG_NAME_LENGTH;
    }

    uint16_t str;
    if (cmd) {
        str = internal_get_prop(obj, 0x3a);
    } else {
        str = GET_DESC(obj);
    }
    print_handler(unpack_string(str), nullptr);
    return count_characters_in_zstring(str);
}

void erase_journey_command_chars(int LN, int COL, int num_spaces) {

    if (options.int_number == INTERP_MSDOS) {
        move_journey_cursor(COL - 1, LN);
    } else {
        move_journey_cursor(COL, LN);
    }

    bool in_leftmost_column = (COL > SCREEN_WIDTH_in_chars * 0.7);

    if (in_leftmost_column) {
        if (!FONT3_FLAG) {
            num_spaces++;
        } else if (options.int_number == INTERP_AMIGA) {
            num_spaces--;
        }
    }

    for (int i = 0; i < num_spaces; i++)
        glk_put_char_stream(windows[1].id->str, UNICODE_SPACE);

    if (!in_leftmost_column) {
        if (!FONT3_FLAG) {
            garglk_set_reversevideo(1);
            glk_put_char_stream(windows[1].id->str, UNICODE_SPACE);
            garglk_set_reversevideo(0);
        } else if (COL > SCREEN_WIDTH_in_chars * 0.5) {
            glk_set_style(style_BlockQuote);
            glk_put_char_stream(windows[1].id->str, THIN_V_LINE);
            glk_set_style(style_Normal);
        }
    }


    move_journey_cursor(COL, LN);
}


void erase_journey_command_pixels(int pix) {
    int NAME_WIDTH_PIX = get_global(0x82);

    int COMMAND_WIDTH = get_global(0xb8);
    int NAME_WIDTH = get_global(0xa2);

    int num_spaces = COMMAND_WIDTH - 1;
    if (pix == NAME_WIDTH_PIX) {
        num_spaces = NAME_WIDTH - 1;
    }

    if (options.int_number == INTERP_MSDOS) {
        set_cursor(curwin->y_cursor, curwin->x_cursor - letterwidth, (uint16_t)-3);
    }

    if (curwin->x_cursor > gscreenw * 0.7) {
        if (!FONT3_FLAG) {
            num_spaces++;
        } else if (options.int_number == INTERP_AMIGA) {
            num_spaces--;
        }
    }

    for (int i = 0; i < num_spaces; i++)
        glk_put_char(UNICODE_SPACE);

    if (pix == NAME_WIDTH_PIX && options.int_number == INTERP_MSDOS) {
        set_cursor(curwin->y_cursor, curwin->x_cursor + letterwidth, (uint16_t)-3);
    } else {
        set_cursor(curwin->y_cursor, curwin->x_cursor, (uint16_t)-3);
    }
}

void print_journey_character_commands(bool CLEAR) {
    fprintf(stderr, "print_journey_character_commands: CLEAR: %s\n", CLEAR ? "true" : "false");

    // Prints the character names and arrows, and their commands in the three rightmost columns.
    // If CLEAR is true, the three columns to the right of the character names will be cleared.

    if (CLEAR)
        number_of_printed_journey_words = 0;

    int LN = get_global(0x0e); // COMMAND-START-LINE
    int PTBL, CHR, POS;
    int UPDATE_FLAG = get_global(0x79);
    if (UPDATE_FLAG && !CLEAR) {
        internal_call(pack_routine(0x4bf8)); // FILL_CHARACTER_TBL();
    }

    if (!CLEAR)
        create_journey_menu(kJMenuTypeMembers, false);

    PTBL = get_global(0x63);  // global 0x63 is PARTY table: <GLOBAL PARTY <TABLE 5 BERGON PRAXIX ESHER TAG 0>>

    set_current_window(&windows[1]);

    int COMMAND_WIDTH = get_global(0xb8);
    int NAME_WIDTH = get_global(0xa2);
    int NAME_RIGHT = get_global(0x28) - 2; // global 0x28 is CHR-COMMAND-COLUMN

    // Print up to 5 character names and arrows
    for (int i = 1; i <= 5; i++) {
        CHR = word(PTBL + 2 * i); // <GET .PTBL 1>
        POS = get_global(0xa3); // NAME-COLUMN

        erase_journey_command_chars(LN, POS, NAME_WIDTH - 1);

        // global 0x80 is SUBGROUP-MODE
        // attribute 0x2a is SUBGROUP flag
        if (CHR != 0 && !(get_global(0x80) && !internal_test_attr(CHR, 0x2a))) {
            int namelength = PRINT_DESC(CHR, false);

            uint16_t LONG_ARROW_WIDTH = 3;
            uint16_t SHORT_ARROW_WIDTH = 2;
            uint16_t NO_ARROW_WIDTH = 1;

            if (SCREEN_WIDTH_in_chars < 55) { // (<L? ,SCREEN-WIDTH ,8-WIDTH

                if (NAME_WIDTH - namelength - 2 < SHORT_ARROW_WIDTH) {
                    move_journey_cursor(NAME_RIGHT - NO_ARROW_WIDTH, LN);
                    glk_put_string(const_cast<char*>(">"));
                } else {
                    move_journey_cursor(NAME_RIGHT - SHORT_ARROW_WIDTH, LN);
                    glk_put_string(const_cast<char*>("->"));
                }

            } else {
                move_journey_cursor(NAME_RIGHT - LONG_ARROW_WIDTH, LN);
                glk_put_string(const_cast<char*>("-->"));
            }
        }

        if (current_input == INPUT_PARTY) {
            POS = get_global(0x28); //CHR-COMMAND-COLUMN;

            // global 0x32 is CHARACTER-INPUT-TBL
            // CHARACTER-INPUT-TBL contains pointers to tables of up to three verbs
            uint16_t BTBL = word(get_global(0x32) + i * 2);

            bool SUBGROUP_MODE = (get_global(0x80) == 1); // global 0x80 is SUBGROUP-MODE
            bool SUBGROUP = internal_test_attr(CHR, 0x2a); // flag 0x2a is SUBGROUP flag
            bool SHADOW = internal_test_attr(CHR, 0x17); // flag 0x17 is SHADOW flag
            fprintf(stderr, "SUBGROUP_MODE: %s SUBGROUP flag: %s SHADOW flag: %s\n", SUBGROUP_MODE ? "true" : "false", SUBGROUP ? "true" : "false",SHADOW ? "true" : "false" );

            bool should_print_command = (!CLEAR && CHR != 0 && !(SUBGROUP_MODE && !SUBGROUP) && !SHADOW);

            // Print up to three verbs for each character to the right, or just erase the fields
            for (int j = 0; j <= 2; j++) {
                erase_journey_command_chars(LN, POS, COMMAND_WIDTH - 1);

                if (should_print_command) {
                    PRINT_COMMAND(word(BTBL + j * 2));
                }

                POS += COMMAND_WIDTH;
            }
        }

        LN++;
    }


    if (get_global(0x40) == 1) { // SMART-DEFAULT-FLAG
        set_global(0x40, 0);
        internal_call(pack_routine(0x64bc)); //    SMART_DEFAULT();
    }

    set_current_window(&windows[0]);
}

bool journey_read_elvish(int actor) {
    //  actor is set to 0x78 (Tag) by default

    int COL = get_global(0xb0); // COMMAND-OBJECT-COLUMN
    print_journey_character_commands(true); // <CLEAR-FIELDS>

    user_store_byte(get_global(0x75), 0x14); // <PUTB ,E-LEXV 0 20>
    user_store_byte(get_global(0x34), 0x32); // <PUTB ,E-INBUF 0 50>

    int LN = get_global(0x0e) + party_pcm(actor) - 1; // COMMAND-START-LINE

    move_journey_cursor(get_global(0x28), LN); // CHR-COMMAND-COLUMN
    glk_put_string_stream(windows[1].id->str, const_cast<char*>("says..."));

    int MAX = SCREEN_WIDTH_in_chars - get_global(0xb0) - 2;

    uint16_t TBL = get_global(0x55); // <SET TBL ,E-TEMP>

    uint16_t offset = journey_read_keyboard_line(COL, LN, TBL, MAX, true, nullptr);

    refresh_journey_character_command_area(get_global(0x0e) - 1); // <REFRESH-CHARACTER-COMMAND-AREA <- ,COMMAND-START-LINE 1>>

    change_input(INPUT_PARTY);

    set_global(0x79, 1); // <SETG UPDATE-FLAG T>

    if (offset == 0)
        return false;

    internal_call_with_arg(pack_routine(0x7b5c), offset);  // <MASSAGE-ELVISH .OFF>
    set_global(0x03, offset); // <SETG E-TEMP-LEN .OFF>

    tokenize(get_global(0x34),get_global(0x75), 0, false);
    if (user_byte(get_global(0x75) + 1) == 0) // <ZERO? <GETB ,E-LEXV 1>>
        return false;
    if (actor == 0x4f || actor == 0x0196) // PRAXIX ,BERGON
        return true;
    internal_call(pack_routine(0x1951c)); // <PARSE-ELVISH>
    return true;
}

void journey_change_name() {
    // <TURN-ON-CURSOR>  <CURSET -2>>

    int MAX;
    if (SCREEN_WIDTH_in_chars < 50) // 8-WIDTH
        MAX = 5;
    else
        MAX = 8;
    
    int COL = get_global(0xa3); // NAME-COLUMN
    int LN = get_global(0x0e) + party_pcm(0x78) - 1; // <SET LN <- <+ ,COMMAND-START-LINE <PARTY-PCM ,TAG>> 1>
    uint16_t TBL = get_global(0xba) + 2; // NAME-TBL

    uint8_t key;
    uint16_t offset = journey_read_keyboard_line(COL, LN, TBL, MAX, false, &key);

    if (offset == 0) {
        set_global(0x79, 1); // <SETG UPDATE-FLAG T>
    } else if (internal_call_with_arg(pack_routine(0x78a0), offset) == 1) { // ILLEGAL-NAME
        glk_put_string(const_cast<char*>("[The name you have chosen is reserved. Please try again.]"));
    } else {
        // Do the change
        internal_call(pack_routine(0x676c)); // <REMOVE-TRAVEL-COMMAND>

        encode_text(TBL, offset, get_global(0x96)); //  <ZWSTR ,NAME-TBL .OFF 2 ,TAG-NAME>

        // Set which single letter key to use for selecting protagonist
        internal_put_prop(0x01a3, 0x39, key); // <PUTP ,TAG-OBJECT ,P?KBD .KBD>

        set_global(0x1b, offset); // <SETG TAG-NAME-LENGTH .OFF>
        set_global(0x79, 1); // <SETG UPDATE-FLAG T>
    }
}

void init_journey_screen(void) {
    Window *lastwin = curwin;
    set_current_window(&windows[1]);
    glk_window_clear(windows[1].id);

    update_internal_globals();

    glk_set_style(style_Normal);
    garglk_set_reversevideo(0);

    int TOP_SCREEN_LINE = get_global(0x25);
    int COMMAND_START_LINE = get_global(0x0e);

    SETUP_WINDOWS();
    int TEXT_WINDOW_LEFT = get_global(0x38);


    // Draw a line at the top with a centered "JOURNEY"
    if (BORDER_FLAG) {
        FONT3_LINE(1, H_LINE, 47, 48);
        int x = SCREEN_WIDTH_in_chars / 2 - 2;
        move_journey_cursor(x, 0);
        glk_put_string(const_cast<char*>("JOURNEY"));
    }

    // Draw vertical divider between graphics window
    // and buffer text output window

    // Starts at TOP-SCREEN-LINE which is 2 on Amiga (because of border)
    // otherwise 1
    int LN;
    for (LN = TOP_SCREEN_LINE; LN != COMMAND_START_LINE - 1; LN++) {
        if (!BORDER_FLAG) {
            move_journey_cursor(TEXT_WINDOW_LEFT - 1, LN);
            if (FONT3_FLAG) {
                glk_set_style(style_BlockQuote);
                glk_put_char(THIN_V_LINE);
                glk_set_style(style_Normal);
            } else {
                garglk_set_reversevideo(1);
                glk_put_char(UNICODE_SPACE);
                garglk_set_reversevideo(0);
            }
        } else {
            glk_set_style(style_BlockQuote);
            move_journey_cursor(0, LN);
            glk_put_char(THIN_V_LINE);
            move_journey_cursor(TEXT_WINDOW_LEFT - 1, LN);
            glk_put_char(THIN_V_LINE);
            move_journey_cursor(SCREEN_WIDTH_in_chars, LN);
            glk_put_char(40);
            glk_set_style(style_Normal);
        }
    };

    // Draw horizontal line above "command area"
    if (FONT3_FLAG) {
        if (BORDER_FLAG) {
            FONT3_LINE(LN, H_LINE, THIN_V_LINE, 40);
        } else {
            FONT3_LINE(LN, H_LINE, H_LINE, H_LINE);
        }
    } else {
        move_journey_cursor(0, LN);
        garglk_set_reversevideo(1);
        for (int i = 0; i < SCREEN_WIDTH_in_chars; i++)
            glk_put_char(UNICODE_SPACE);
        garglk_set_reversevideo(0);
    }

    // Draw bottom border line
    if (BORDER_FLAG) {
        FONT3_LINE(SCREEN_HEIGHT_in_chars, 38, 46, 49);
    }

    if (!FONT3_FLAG) {
        garglk_set_reversevideo(1);
    }

    int NAME_COLUMN = get_global(0xa3);
    int NAME_WIDTH = get_global(0xa2);

    int WIDTH = 9; //  WIDTH = TEXT_WIDTH("The Party");

    // Print "The Party" centered over the name column
    int x = NAME_COLUMN + (NAME_WIDTH - WIDTH) / 2 - 1;
    move_journey_cursor(x, LN);
    glk_put_string(const_cast<char*>("The Party"));

    // Print "Individual Commands" centered in the empty space to the right of "The Party" text
    WIDTH = 19; // WIDTH = TEXT_WIDTH("Individual Commands");

    int CHR_COMMAND_COLUMN = get_global(0x28);
    move_journey_cursor(CHR_COMMAND_COLUMN + (SCREEN_WIDTH_in_chars - CHR_COMMAND_COLUMN - WIDTH) / 2 + (FONT3_FLAG ? 1 : 0),  LN);

    glk_put_string(const_cast<char*>("Individual Commands"));

    if (!FONT3_FLAG) {
        garglk_set_reversevideo(0);
    }
    set_current_window(lastwin);
}

void TAG_ROUTE_PRINT(void) {
//    int width_12 = 71;
    int TAG_NAME_LENGTH = get_global(0x1b);
    int NAME_TBL = get_global(0xba) + 4;

    for (uint16_t j = 0; j < TAG_NAME_LENGTH; j++) {
        put_char(user_byte(NAME_TBL++));
    }
    if (SCREEN_WIDTH_in_chars < 71 || TAG_NAME_LENGTH > 6) {
        glk_put_string(const_cast<char*>(" Rt"));
    } else {
        glk_put_string(const_cast<char*>(" Route"));
    }
}

void PRINT_CHARACTER_COMMANDS(void) {
    if (current_input == INPUT_OBJECT || current_input == INPUT_SPECIAL) {
        change_input(INPUT_PARTY);
        number_of_printed_journey_words = 0;
    }
    print_journey_character_commands(variable(1) == 1 ? true : false);
}


void READ_ELVISH(void) {
    change_input(INPUT_ELVISH);
    store_variable(1, (journey_read_elvish(variable(1)) ? 1 : 0));
}

void CHANGE_NAME(void) {
    change_input(INPUT_NAME);
    journey_change_name();
}

void ERASE_COMMAND(void) {
    erase_journey_command_pixels(variable(1));
}

void print_journey_columns(bool PARTY, bool PRSI) {
    fprintf(stderr, "print_journey_columns: PARTY: %s PRSI: %s\n", PARTY ? "true" : "false", PRSI ? "true" : "false");
    int column, table, object;
    int line = get_global(0x0e); // COMMAND-START-LINE (G0e)

    int COMMAND_WIDTH = get_global(0xb8);
    int O_TABLE = get_global(0x14); // O-TABLE (object table)
    set_current_window(&windows[1]);

    if (PARTY) {
        column = get_global(0xb4); // PARTY-COMMAND-COLUMN Global b4
        table = get_global(0x06); // PARTY-COMMANDS (G06)
    } else  {
        column = get_global(0xb0) + (PRSI ? COMMAND_WIDTH : 0); // COMMAND-OBJECT-COLUMN (Gb0) + COMMAND-WIDTH (Gb8)
        table = O_TABLE + (PRSI ? 10 : 0);;
        create_journey_menu(kJMenuTypeObjects, PRSI);
    }

    int table_count = user_word(table);

    for (int i = 1; i <= table_count; i++) {
        object = user_word(table + 2 * i);
        erase_journey_command_chars(line, column, COMMAND_WIDTH - 1);
        if (PARTY) {
            if (object == 0x3ea7 // TAG-ROUTE-COMMAND
                && get_global(0x1b) // TAG-NAME-LENGTH (G1b)
                != 0) {
                TAG_ROUTE_PRINT();
            } else {
                PRINT_COMMAND(object);
            }
        } else {
            PRINT_DESC(object, 1); // <PRINT-DESC .OBJ T>
        }
        line++;
        if (i % 5 == 0) {
            // Move to the next column
            column += COMMAND_WIDTH; // COMMAND-WIDTH (Gb8)
            line = get_global(0x0e); // COMMAND-START-LINE (G0e)
        }
    }
//    if (SCREEN_WIDTH_in_chars < 60) {
        refresh_journey_character_command_area(get_global(0x0e) - 1);
//    }
}

void PRINT_COLUMNS(void) {
    fprintf(stderr, "PRINT-COLUMNS: Local variable 0:%d Local variable 1:%d \n", variable(1), variable(2));
    if (variable(1) == 1) {
        change_input(INPUT_PARTY);
        create_journey_party_menu();
    } else if (variable(2) == 1) {
        change_input(INPUT_SPECIAL);
    } else {
        change_input(INPUT_OBJECT);
    }
    print_journey_columns(variable(1), variable(2));
}

int refresh_journey_character_command_area(int LN) {
    fprintf(stderr, "called refresh_journey_character_command_area(LN %d)\n", LN);
    int POS;

    update_internal_globals();

    int COMMAND_START_LINE = get_global(0x0e);

    // Width of a column (except Name column) in characters
    int COMMAND_WIDTH = get_global(0xb8);
    // Width of a column (except Name column) in pixels
    int COMMAND_WIDTH_PIX = get_global(0x6d);

    int NAME_WIDTH = get_global(0xa2);

    int PARTY_COMMAND_COLUMN = get_global(0xb4);

    Window *lastwin = curwin;
    set_current_window(&windows[1]);

    while (++LN <= COMMAND_START_LINE + 4) {
        POS = 1;
        move_journey_cursor(POS, LN);
        while (POS <= SCREEN_WIDTH_in_chars) {
            if (FONT3_FLAG) {
                if (POS != 1 && POS < SCREEN_WIDTH_in_chars - 5) {
                    glk_set_style(style_BlockQuote);
                    move_journey_cursor(POS, LN);
                    if (POS == COMMAND_WIDTH || POS == COMMAND_WIDTH + 1 || POS == COMMAND_WIDTH + NAME_WIDTH + 1 || POS == COMMAND_WIDTH + NAME_WIDTH) {
                        glk_put_char(THICK_V_LINE);
                    } else {
                        glk_put_char(THIN_V_LINE);
                    }
                    glk_set_style(style_Normal);
                } else if (POS == 1 && BORDER_FLAG) {
                    glk_set_style(style_BlockQuote);
                    move_journey_cursor(POS, LN);
                    glk_put_char(THIN_V_LINE);
                    glk_set_style(style_Normal);
                }
            } else if (POS != 1 && POS < SCREEN_WIDTH_in_chars - 5) {
                move_journey_cursor(POS - 1, LN);
                garglk_set_reversevideo(1);
                glk_put_char(UNICODE_SPACE);
                garglk_set_reversevideo(0);
            }

            if (POS == COMMAND_WIDTH || POS == COMMAND_WIDTH + 1) {
                POS += NAME_WIDTH;
            } else {
                if (COMMAND_WIDTH_PIX == 0) {
                    move_journey_cursor(PARTY_COMMAND_COLUMN, LN);
                }
                POS += COMMAND_WIDTH;
            }
        }

        if (BORDER_FLAG) {
            glk_set_style(style_BlockQuote);
            move_journey_cursor(SCREEN_WIDTH_in_chars, LN);
            glk_put_char(40); // Draw right border char (40)
            glk_set_style(style_Normal);
        }
    }

    set_current_window(lastwin);
    glk_set_style(style_Normal);

    return LN;
}

void REFRESH_CHARACTER_COMMAND_AREA(void) {
    fprintf(stderr, "REFRESH-CHARACTER-COMMAND-AREA: Local variable 0:%d \n", variable(1));
    int LN = variable(1);
    refresh_journey_character_command_area(LN);
}

void print_globals(void) {
    fprintf(stderr, "Let's only support r83 for now\n");
    fprintf(stderr, "MOUSETBL is global 0xbd (0x%x)\n", get_global(0xbd));

    fprintf(stderr, "SAVED-PCM is 0x%x (%d)\n", get_global(0x08), (int16_t)get_global(0x08));
    fprintf(stderr, "SAVED-PCF is 0x%x (%d)\n", get_global(0x98), (int16_t)get_global(0x98));
    fprintf(stderr, "BORDER-FLAG is global 0x9f (%x)\n", get_global(0x9f));
    fprintf(stderr, "FONT3-FLAG is global 0x31 (%x)\n", get_global(0x31));
    fprintf(stderr, "FWC-FLAG is global 0xb9 (%x)\n", get_global(0xb9));
    fprintf(stderr, "BLACK-PICTURE-BORDER is global 0x72 (%x)\n", get_global(0x72));
    fprintf(stderr, "CHRH is global 0x92 (0x%x) (%d)\n", get_global(0x92), get_global(0x92));
    fprintf(stderr, "letterwidth is %hu\n", letterwidth);
    fprintf(stderr, "gcellw is %f\n", gcellw);
    fprintf(stderr, "CHRV is global 0x89 (0x%x) (%d)\n", get_global(0x89), get_global(0x89));
    fprintf(stderr, "letterheight is %hu\n", letterheight);
    fprintf(stderr, "gcellh is %f\n", gcellh);
    fprintf(stderr, "SCREEN-WIDTH is global 0x83 (0x%x) (%d)\n", get_global(0x83), get_global(0x83));
    fprintf(stderr, "gscreenw is %d\n", gscreenw);
    fprintf(stderr, "gscreenw / letterwidth is %d\n", gscreenw / letterwidth);
    fprintf(stderr, "gscreenw / gcellw is %f\n", gscreenw / gcellw);

    fprintf(stderr, "SCREEN-HEIGHT is global 0x64 (0x%x) (%d)\n", get_global(0x64), get_global(0x64));
    fprintf(stderr, "gscreenh is %d\n", gscreenh);
    fprintf(stderr, "gscreenh / letterheight is %d\n", gscreenh / letterheight);
    fprintf(stderr, "gscreenh / gcellh is %f\n", gscreenh / gcellh);

    fprintf(stderr, "TOP-SCREEN-LINE is global 0x25 (0x%x) (%d)\n", get_global(0x25), get_global(0x25));
    fprintf(stderr, "COMMAND-START-LINE is global 0x0e (0x%x) (%d)\n", get_global(0x0e), get_global(0x0e));
    fprintf(stderr, "COMMAND-WIDTH is global 0xb8 (0x%x) (%d)\n", get_global(0xb8), get_global(0xb8));
    fprintf(stderr, "COMMAND-WIDTH-PIX is global 0x6d (0x%x) (%d)\n", get_global(0x6d), get_global(0x6d));
    fprintf(stderr, "NAME-WIDTH is global 0xa2 (0x%x) (%d)\n", get_global(0xa2), get_global(0xa2));
    fprintf(stderr, "NAME-WIDTH-PIX is global 0x82 (0x%x) (%d)\n", get_global(0x82), get_global(0x82));
    fprintf(stderr, "NAME-RIGHT is global 0x2d (0x%x) (%d)\n", get_global(0x2d), get_global(0x2d));

    fprintf(stderr, "PARTY-COMMAND-COLUMN is global 0xb4 (0x%x) (%d)\n", get_global(0xb4), get_global(0xb4));
    fprintf(stderr, "NAME-COLUMN is global 0xa3 (0x%x) (%d)\n", get_global(0xa3), get_global(0xa3));
    fprintf(stderr, "CHR-COMMAND-COLUMN is global 0x28 (0x%x) (%d)\n", get_global(0x28), get_global(0x28));
    fprintf(stderr, "COMMAND-OBJECT-COLUMN is global 0xb0 (0x%x) (%d)\n", get_global(0xb0), get_global(0xb0));
}

void reprint_partial_input(int x, int y, int length_so_far, int max_length, int16_t table_address) {
    move_journey_cursor(x + 1, y + 1);
    glk_set_style(style_Normal);
    if (!FONT3_FLAG) {
        garglk_set_reversevideo(1);
    }

    int max_screen = SCREEN_WIDTH_in_chars - 1 - (options.int_number == INTERP_AMIGA ? 1 : 0);
    if (max_length + x >= max_screen)
        max_length = max_screen - x;
    int i;
    for (i = 0; i < max_length && i < length_so_far; i++) {
        char c = zscii_to_unicode[byte(table_address + i)];
        if (c == 0) {
            break;
        }
        glk_put_char(c);
    }

    for (int j = i; j <= max_length; j++) {
        underscore_or_square();
    }

    garglk_set_reversevideo(0);
}

void resize_journey_windows_after_restore() {
    win_menuitem(kJMenuTypeDeleteAll, 0, 0, false, nullptr, 15);
    internal_set_attr(68, 0x2e);
    screenmode = MODE_NORMAL;
    change_input(INPUT_PARTY);
    adjust_journey_windows(true);
}

void resize_journey_graphics_and_buffer_windows(void) {

    int text_window_left = get_global(global_text_window_left_idx);

    // Resize windows.
    // Window 0: Text buffer. Receives most keypresses.
    // Window 1: Fullscreen grid window
    // Window 3: Graphics window

    if (BORDER_FLAG) {
        windows[3].x_origin = ggridmarginx + gcellw + 5;
        windows[3].y_origin = ggridmarginy + gcellh + 1;
    } else {
        if (FONT3_FLAG) {
            windows[3].x_origin = ggridmarginx + 1;
        } else {
            windows[3].x_origin = ggridmarginx + 1;
        }
        windows[3].y_origin = ggridmarginy + 1;
    }

    if (SCREEN_HEIGHT_in_chars < 5)
        update_screen_size();

    windows[3].x_size = (float)(text_window_left - 2) * gcellw - windows[3].x_origin + ggridmarginx + (FONT3_FLAG ? 0 : 2);

    windows[0].x_origin = windows[3].x_origin + windows[3].x_size + gcellw + (FONT3_FLAG ? 3 : 0);
    windows[0].x_size = (SCREEN_WIDTH_in_chars - text_window_left + 1) * gcellw;
    windows[0].y_origin = windows[3].y_origin;

    int command_start_line = SCREEN_HEIGHT_in_chars - 4 - (BORDER_FLAG ? 1 : 0);

    windows[0].y_size = (command_start_line - 2) * gcellh - FONT3_FLAG;
    if (BORDER_FLAG) {
        windows[0].x_size -= gcellw;
        windows[0].y_size -= gcellh;
    }
    windows[3].y_size = windows[0].y_size;

    if (windows[3].id == nullptr) {
        windows[3].id = v6_new_glk_window(wintype_Graphics, 0);
        if (windows[3].id != nullptr)
            glk_window_set_background_color(windows[3].id, monochrome_black);
    }
    if (!windows[0].id) {
        windows[0].id = v6_new_glk_window(wintype_TextBuffer, 0);
    }

    v6_sizewin(&windows[0]);
    v6_sizewin(&windows[3]);
}

#pragma mark adjust_journey_windows

void adjust_journey_windows(bool restoring) {
    // Window 0: Text buffer
    // Window 1: Fullscreen grid window
    // Window 3: Graphics window

    // First (leftmost) column (x:1 or x:2) PARTY-COMMAND-COLUMN: party commands such as Proceed
    // Second column: NAME-COLUMN: names of present party members and arrows
    // Third column: CHR-COMMAND-COLUMN: first column of individual commands
    // Fourth column: COMMAND-OBJECT-COLUMN: second column of individual commands,
    // or first list of objects
    // Fifth (rightmost) column has no corresponding global,
    // but is just referred to as COMMAND-OBJECT-COLUMN + COMMAND-WIDTH.
    // It will contain a second list of objects (such as targets in CAST GLOW ON)
    // or a third column of individual commands


    fprintf(stderr, "adjust_journey_windows: inital global values:\n");
    print_globals();

    if (windows[1].id && windows[1].id->type != wintype_TextGrid)
        v6_delete_win(&windows[1]);

    if (windows[1].id == nullptr) {
        windows[1].id = v6_new_glk_window(wintype_TextGrid, 0);
    }

    windows[1].x_size = gscreenw;
    windows[1].y_size = gscreenh;
    v6_sizewin(&windows[1]);

    windows[1].style.reset(STYLE_REVERSE);

    int16_t mousetbl = get_global(0xbd);
    fprintf(stderr, "MOUSETBL 0 is 0x%x (%d)\n", word(mousetbl), (int16_t)word(mousetbl));
    fprintf(stderr, "selected_journey_line is %d\n", selected_journey_line);
    fprintf(stderr, "MOUSETBL 1 is 0x%x (%d)\n", word(mousetbl + 2), (int16_t)word(mousetbl + 2));
    fprintf(stderr, "selected_journey_column is %d\n", selected_journey_column);

    // Draw borders
    init_journey_screen();

    // Redraw vertical lines in command area (the bottom five rows)
    refresh_journey_character_command_area(SCREEN_HEIGHT_in_chars - 5 - BORDER_FLAG);

    if (screenmode == MODE_NORMAL) {

        set_global(0x40, 0); // SMART-DEFAULT-FLAG

        // Print party (the leftmost) column (call print columns with PARTY flag set
        print_journey_columns(true, false);

        if (current_input != INPUT_PARTY) {
            // If the player is currently entering text, redraw typed text at new position
            if (current_input == INPUT_NAME ) {
                reprint_partial_input(get_global(0xb8) + 1, get_global(0x0e) + from_command_start_line - 1, input_length, max_length, input_table);
            } else if (current_input == INPUT_ELVISH ) {
                move_journey_cursor(get_global(0x28), get_global(0x0e) + from_command_start_line);
                glk_put_string_stream(windows[1].id->str, const_cast<char*>("says..."));
                reprint_partial_input(get_global(0xb0) - 1, get_global(0x0e) + from_command_start_line - 1, input_length, max_length - 1, input_table);
            } else {
                print_journey_columns(false, current_input == INPUT_SPECIAL);
            }
        }

        if (current_input != INPUT_NAME) {
            // Print character names and arrows, and corresponding verbs in the three rightmost columns
            print_journey_character_commands(false);
        }

        // reset bit 2 in LOWCORE FLAGS, no screen redraw needed
        store_word(8, (word(8) & 0xfffb));

        if (!restoring && screenmode != MODE_CREDITS) {
            fprintf(stderr, "selected_journey_line: %d mousetbl: %d SAVED-PCM: %d\n", selected_journey_line, word(mousetbl), get_global(0x08));
            fprintf(stderr, "selected_journey_column: %d mousetbl 1: %d SAVED-PCF: %d\n", selected_journey_column, as_signed(word(mousetbl + 2)), get_global(0x98));
            
            if (selected_journey_column <= 0) { // call BOLD-PARTY-CURSOR
                internal_call_with_2_args(pack_routine(0x6228), selected_journey_line, 0);
            } else if (current_input == INPUT_PARTY) { // call BOLD-CURSOR
                internal_call_with_2_args(pack_routine(0x529c), selected_journey_line, selected_journey_column);

            } else { // call BOLD-OBJECT-CURSOR
                int numwords = number_of_printed_journey_words;
                for (int i = 0; i < numwords; i++) {
                    JourneyWords *word = &printed_journey_words[i];                    internal_call_with_3_args(pack_routine(0x529c), word->pcm, word->pcf, word->str); // BOLD-CURSOR(PCM, PCF, STR)
                }
                internal_call_with_2_args(pack_routine(0x61a4), selected_journey_line, selected_journey_column); // BOLD-OBJECT-CURSOR(PCM, PCF)
            }
        }
    }

    fprintf(stderr, "After REFRESH-SCREEN: MOUSETBL 0 is 0x%x, MOUSETBL 1 is 0x%x\n", word(mousetbl), word(mousetbl + 2));

    resize_journey_graphics_and_buffer_windows();


    // Redraw image(s)
    // (unless we have no graphics)
    if (windows[3].id != nullptr) {
        glk_window_clear(windows[3].id);
        internal_call(pack_routine(0x49fc)); // ROUTINE GRAPHIC

        uint16_t HERE = get_global(0x0f);
        fprintf(stderr, "HERE: 0x%x (%d)\n", HERE, HERE);
        if (HERE == 216) // Control Room
            internal_call(pack_routine(0x307a4)); //  ROUTINE COMPLETE-DIAL-GRAPHICS
    }
    fprintf(stderr, "adjust_journey_windows: final global values:\n");
    print_globals();

    if (current_input == INPUT_NAME || current_input == INPUT_ELVISH) {

        if (current_input == INPUT_NAME) {
            input_column = get_global(0xb8) + 2 + input_length;
        } else {
            input_column = get_global(0xb0) + input_length;
        }

        input_line = get_global(0x0e) + from_command_start_line;

        set_current_window(&windows[1]);

        if (!FONT3_FLAG) {
            garglk_set_reversevideo(1);
        }
        draw_flashing_journey_cursor();
    }
}

void draw_flashing_journey_cursor(void) {
    if (input_column > SCREEN_WIDTH_in_chars - 1)
        return;
    move_journey_cursor(input_column, input_line);

    if (journey_cursor_reverse || FONT3_FLAG)
        garglk_set_reversevideo(1);
    else
        garglk_set_reversevideo(0);

    underscore_or_square();

    if (FONT3_FLAG)
        garglk_set_reversevideo(0);
    else
        garglk_set_reversevideo(1);
    
    move_journey_cursor(input_column, input_line);
    journey_cursor_reverse = !journey_cursor_reverse;
    glk_request_timer_events(300 + (journey_cursor_reverse ? 600 : 0));
}


void BOLD_CURSOR(void) {
    fprintf(stderr, "BOLD-CURSOR(");

    fprintf(stderr, "PCM:%d, PCF:%d",variable(1), as_signed(variable(2)));

    fprintf(stderr, ", STR:%d, \"", variable(3));
    debug_PRINT_STRING(variable(3));

    fprintf(stderr, "\")\n");

    if (number_of_printed_journey_words < 4) {
        JourneyWords *word = &printed_journey_words[number_of_printed_journey_words++];
        word->pcm = variable(1);
        word->pcf = variable(2);
        word->str = variable(3);
    }
}

void BOLD_PARTY_CURSOR(void) {
    fprintf(stderr, "BOLD-PARTY-CURSOR(");
//    if (znargs > 1) {
        fprintf(stderr, "PCM:%d, PCF:%d",variable(1), as_signed(variable(2)));
//    }
    fprintf(stderr, ")\n");
}


void after_INTRO(void) {
    fprintf(stderr, "after_INTRO\n");
    screenmode = MODE_NORMAL;
//    adjust_journey_windows();
}

void GRAPHIC_STAMP(void) {
    int16_t picnum, where = 0;
    if (znargs > 0 && windows[3].id != nullptr) {
        picnum = variable(1);
        if (znargs > 1)
            where = variable(2);
        draw_journey_stamp_image(windows[3].id, picnum, where, pixelwidth);
    }
}


void REFRESH_SCREEN(void) {
    fprintf(stderr, "REFRESH-SCREEN\n");
    store_variable(1, 0);
}

void INIT_SCREEN(void) {
    fprintf(stderr, "INIT-SCREEN\n");

    // We check if START-LOC has the SEEN flag set
    // This will be false at the start of the game
    // and on restart.
    if (!qset(68, 0x2e) && screenmode == MODE_INITIAL_QUESTION) {
        // Show title image and wait for key press

        // We delete the graphics window to make it recreate on top
        v6_delete_win(&windows[3]);
        glk_window_clear(windows[0].id);
        screenmode = MODE_SLIDESHOW;
        // We do a fake resize event to draw the title image
        // (This will recreate the graphics window)
        window_change();
        win_setbgnd(windows[0].id->peer, monochrome_black);
        glk_request_mouse_event(windows[3].id);
        glk_request_char_event(curwin->id);
        read_char();
        screenmode = MODE_CREDITS;
        win_setbgnd(windows[0].id->peer, user_selected_background);
        adjust_journey_windows(false);
    } else {
        init_journey_screen();
    }
}

void DIVIDER(void) {
    fprintf(stderr, "DIVIDER\n");
    glk_set_style(style_User2);
    glk_put_string(const_cast<char*>("\n\n***\n\n"));
    transcribe(UNICODE_LINEFEED);
    transcribe(UNICODE_LINEFEED);
    transcribe(UNICODE_LINEFEED);
    transcribe('*');
    transcribe('*');
    transcribe('*');
    transcribe(UNICODE_LINEFEED);
    transcribe(UNICODE_LINEFEED);
    transcribe(UNICODE_LINEFEED);
    glk_set_style(style_Normal);
}

void WCENTER(void) {
    fprintf(stderr, "WCENTER\n");
    int16_t stringnum = variable(1);
    if (stringnum == 0x06ff)
        glk_set_style(style_User1);
    else
        glk_set_style(style_User2);
    print_handler(unpack_string(stringnum), nullptr);
    glk_set_style(style_Normal);
}

void update_journey_on_resize(void) {
    // Window 0: Text buffer
    // Window 1: Fullscreen grid window
    // Window 3: Graphics window

    set_global(0x7f, options.int_number); // GLOBAL INTERPRETER

    set_global(0x92, gcellw); // GLOBAL CHRH
    set_global(0x89, gcellh); // GLOBAL CHRV

    if (screenmode == MODE_INITIAL_QUESTION) {
        windows[0].x_size = gscreenw;
        windows[0].y_size = gscreenh;
        v6_sizewin(&windows[0]);
    } else if (screenmode == MODE_SLIDESHOW) {
        if (!windows[3].id) {
            windows[3].id = v6_new_glk_window(wintype_Graphics, 0);
        }
        windows[3].x_size = gscreenw;
        windows[3].y_size = gscreenh;

        v6_sizewin(&windows[3]);

        draw_journey_title_image();
    } else {
        adjust_journey_windows(false);
    }
}

//void INTRO(void) {
//    fprintf(stderr, "INTRO!\n");

//    glk_stylehint_set(wintype_TextBuffer, style_Subheader, stylehint_Justification, stylehint_just_Centered);
//    glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_Justification, stylehint_just_Centered);
//    remap_win_to_buffer(&windows[0]);

//    glk_window_clear(windows[0].id);
//}


//void after_TITLE_PAGE(void) {
//    fprintf(stderr, "after TITLE_PAGE!\n");
////    centeredText = false;
////    glk_stylehint_set(wintype_TextBuffer, style_Subheader, stylehint_Justification, stylehint_just_LeftFlush);
////    glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_Justification, stylehint_just_LeftFlush);
////    v6_delete_win(&windows[0]);
////    remap_win_to_buffer(&windows[0]);
////    v6_sizewin(&windows[0]);
//}
