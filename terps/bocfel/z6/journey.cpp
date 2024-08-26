//
//  journey.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-18.
//

#include "screen.h"
#include "zterp.h"
#include "memory.h"
#include "objects.h"
#include "process.h"
#include "unicode.h"
#include "dict.h"
#include "options.h"

#include "draw_image.hpp"
#include "v6_specific.h"

#include "journey.hpp"

extern Window *curwin;

JourneyGlobals jg;
JourneyRoutines jr;
JourneyObjects jo;
JourneyAttributes ja;

enum StampBits {
  kStampRightBit = 1,
  kStampBottomBit = 2
};

#define JOURNEY_BG_GRID windows[1]
#define JOURNEY_GRAPHICS_WIN windows[3]

Window *journey_text_buffer;

static int journey_image_x, journey_image_y, journey_image_width, journey_image_height;

static float journey_image_scale = 1.0;

static glui32 SCREEN_WIDTH_in_chars, SCREEN_HEIGHT_in_chars;

static bool BORDER_FLAG, FONT3_FLAG;

JourneyWords printed_journey_words[4];
int number_of_printed_journey_words = 0;

struct JourneyMenu {
    char name[15];
    struct JourneyMenu *submenu;
    int submenu_entries;
    int length;
    int line;
    int column;
};

int16_t selected_journey_line = -1;
int16_t selected_journey_column = -1;

#define THICK_V_LINE 57
#define THIN_V_LINE 41
#define H_LINE 39

#define MAX_SUBMENU_ITEMS 6


inputMode journey_current_input = INPUT_PARTY;

uint16_t journey_input_length = 0;
static uint16_t max_length = 0;
static uint16_t from_command_start_line = 0;
static uint16_t input_column = 0;
static uint16_t input_line = 0;
static uint16_t input_table = 0;


void journey_adjust_image(int picnum, uint16_t *x, uint16_t *y, int width, int height, int winwidth, int winheight, float *scale, float pixwidth) {

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

static void draw_journey_stamp_image(winid_t win, int16_t picnum, int16_t where, float pixwidth) {

    fprintf(stderr, "draw_journey_stamp_image: picnum %d where:%d\n", picnum, where);
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

static void draw_journey_title_image(void) {
    float scale;
    uint16_t x, y;
    int width, height;
    winid_t win = JOURNEY_GRAPHICS_WIN.id;
    if (win == nullptr || win->type != wintype_Graphics) {
        fprintf(stderr, "draw_journey_title_image: glk window error!\n");
        return;
    }
    glk_window_set_background_color(win, monochrome_black);
    glk_window_clear(win);
    win_setbgnd(-1, monochrome_black);

    get_image_size(160, &width, &height);
    journey_adjust_image(160, &x, &y, width, height, gscreenw, gscreenh, &scale, pixelwidth);
    draw_inline_image(win, 160, x, y, scale, false);
}

static void journey_font3_line(int LN, int CHR, int L, int R) {
    glk_set_style(style_BlockQuote);
    glk_window_move_cursor(curwin->id, 0, LN - 1);
    glk_put_char(L);
    for (int i = 1; i < SCREEN_WIDTH_in_chars - 1; i++)
        glk_put_char(CHR);
    glk_put_char(R);
    glk_set_style(style_Normal);
}

static int journey_refresh_character_command_area(int16_t LN);

static void journey_setup_windows(void) {
    int PW;
    int OFF = 6;
    int TEXT_WINDOW_LEFT;
    bool GOOD = false;

    bool APPLE2 = (options.int_number == INTERP_APPLE_IIC || options.int_number == INTERP_APPLE_IIE || options.int_number == INTERP_APPLE_IIGS);

    int width, height;
    GOOD = get_image_size(2, &width, &height);
    if (!GOOD) {
        fprintf(stderr, "journey_setup_windows: Could not get size of image 2!\n");
        TEXT_WINDOW_LEFT = 32;
    } else {
        PW = round((float)width * imagescalex);

        if (APPLE2 || options.int_number == INTERP_MSDOS) {
            OFF = 3;
        }
        if (options.int_number != INTERP_AMIGA) {
            OFF = 5;
        }
        TEXT_WINDOW_LEFT = OFF + (PW + gcellw) / gcellw;
    }

    set_global(jg.TEXT_WINDOW_LEFT, TEXT_WINDOW_LEFT);
}

static bool qset(uint16_t obj, int16_t bit) { // Test attribute, set it, return test result.
    if (obj == 0 || bit == 0)
        return false;
    bool result = internal_test_attr(obj, bit);
    internal_set_attr(obj, bit);
    return result;
}

static void journey_adjust_windows(bool restoring);

static void update_screen_size(void) {
    glk_window_get_size(JOURNEY_BG_GRID.id, &SCREEN_WIDTH_in_chars, &SCREEN_HEIGHT_in_chars);
    if (SCREEN_WIDTH_in_chars == 0 || SCREEN_HEIGHT_in_chars == 0) {
        fprintf(stderr, "Error!\n");
        SCREEN_WIDTH_in_chars = (gscreenw - ggridmarginx * 2) / gcellw;
        SCREEN_HEIGHT_in_chars = (gscreenh - ggridmarginy * 2) / gcellh;
    }

    set_global(jg.SCREEN_WIDTH, SCREEN_WIDTH_in_chars);
    set_global(jg.SCREEN_HEIGHT, SCREEN_HEIGHT_in_chars);

    journey_sync_upperwin_size(SCREEN_WIDTH_in_chars, SCREEN_HEIGHT_in_chars);
}


static void update_internal_globals(void) {

    int BLACK_PICTURE_BORDER;
    int FWC_FLAG;

    switch (options.int_number) {
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
            BORDER_FLAG = false;
            FONT3_FLAG = false;
            BLACK_PICTURE_BORDER = 0;
            break;
        default: // Should never happen
            fprintf(stderr, "Unsupported interpreter option (%lu)!\n", options.int_number);
            exit(1);
    }

    // FWC-FLAG tells whether to switch font when printing in command window, i.e. whether there is a separate proportional font used elsewhere.
    // Always the same as FONT3-FLAG, and used interchangably in the original code
    FWC_FLAG = FONT3_FLAG;

    if (jg.BORDER_FLAG != 0) {
        // Whether there is a border around the screen, true on Amiga only
        set_global(jg.BORDER_FLAG, BORDER_FLAG ? 1 : 0);

        // Whether to use Font 3. True for Amiga and Mac, false for MS DOS and Apple II
        set_global(jg.FONT3_FLAG, FONT3_FLAG ? 1 : 0);
        set_global(jg.FWC_FLAG, FWC_FLAG);
        // Whether to have a border around the image. BLACK_PICTURE_BORDER is only false on IBM PC
        set_global(jg.BLACK_PICTURE_BORDER, BLACK_PICTURE_BORDER);
    }

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

    set_global(jg.TOP_SCREEN_LINE, TOP_SCREEN_LINE);
    set_global(jg.COMMAND_START_LINE, COMMAND_START_LINE);

    // Width of a column (except Name column) in characters
    int COMMAND_WIDTH = SCREEN_WIDTH_in_chars / 5;
    set_global(jg.COMMAND_WIDTH, COMMAND_WIDTH);

    // Width of column in pixels. Used in ERASE-COMMAND
    int COMMAND_WIDTH_PIX = COMMAND_WIDTH - 1;


    int NAME_WIDTH = COMMAND_WIDTH + SCREEN_WIDTH_in_chars % 5;
    set_global(jg.NAME_WIDTH, NAME_WIDTH);

    int NAME_WIDTH_PIX = NAME_WIDTH - 1;

    if (jg.COMMAND_WIDTH_PIX != 0 && jg.COMMAND_WIDTH != jg.COMMAND_WIDTH_PIX) {
        set_global(jg.COMMAND_WIDTH_PIX, COMMAND_WIDTH_PIX);
        set_global(jg.NAME_WIDTH_PIX, NAME_WIDTH_PIX);
    }

    bool APPLE2 = (options.int_number == INTERP_APPLE_IIC || options.int_number == INTERP_APPLE_IIE || options.int_number == INTERP_APPLE_IIGS);

    int PARTY_COMMAND_COLUMN;

    if (APPLE2) {
        PARTY_COMMAND_COLUMN = 1;
    } else {
        PARTY_COMMAND_COLUMN = 2;
    }
    set_global(jg.PARTY_COMMAND_COLUMN, PARTY_COMMAND_COLUMN);


    // NAME_COLUMN: Position of second column in characters (1-based)
    int NAME_COLUMN = PARTY_COMMAND_COLUMN + COMMAND_WIDTH;
    set_global(jg.NAME_COLUMN, NAME_COLUMN);

    int CHR_COMMAND_COLUMN = NAME_COLUMN + NAME_WIDTH;
    set_global(jg.CHR_COMMAND_COLUMN, CHR_COMMAND_COLUMN);

    int COMMAND_OBJECT_COLUMN = CHR_COMMAND_COLUMN + COMMAND_WIDTH;
    set_global(jg.COMMAND_OBJECT_COLUMN, COMMAND_OBJECT_COLUMN);
}

static int party_pcm(int chr) {
    uint16_t party_table = get_global(jg.PARTY); // global 0x63 is PARTY table
    uint16_t MAX = user_word(party_table);
    for (int cnt = 1; cnt <= MAX; cnt++) {
        if (word(party_table + cnt * 2) == chr)
            return cnt;
    }
    return 0;
}

static bool bad_character(uint8_t c, bool elvish) {
    if (c >= 'A' && c <= 'Z')
        return false;
    if (c >= 'a' && c <= 'z')
        return false;
    if (elvish && (c == ' ' || c == '\'' || c == '-' || c == '.' || c == ',' || c == '?'))
        return false;
    return true;
}


static void underscore_or_square() {
    if (FONT3_FLAG) {
        glk_put_char('_');
    } else {
        glk_put_char(UNICODE_SPACE);
    }
}

static void move_v6_cursor(int column, int line) {
    fprintf(stderr, "move_v6_cursor x:%d y:%d\n", column, line);
    if (column < 1)
        column = 0;
    else
        column--;
    if (line < 1)
        line = 0;
    else
        line--;

    JOURNEY_BG_GRID.x = column;
    JOURNEY_BG_GRID.y = line;

    if (column > SCREEN_WIDTH_in_chars || line > SCREEN_HEIGHT_in_chars) {
        fprintf(stderr, "Error! move_v6_cursor() moving cursor out of bounds\n");
        if (column > SCREEN_WIDTH_in_chars)
            column = SCREEN_WIDTH_in_chars - 1;
        if (line > SCREEN_HEIGHT_in_chars)
            line = SCREEN_HEIGHT_in_chars - 1;
    }

    glk_window_move_cursor(JOURNEY_BG_GRID.id, column, line);
}

//void debug_print_str(uint8_t c);

//static void debug_PRINT_STRING(uint16_t str) {
//    print_handler(unpack_string(str), debug_print_str);
//}

static int GET_COMMAND(int cmd);

static char *string_buf_ptr = nullptr;
static int string_buf_pos = 0;
static int string_maxlen = 0;

static void print_to_string_buffer(uint8_t c) {

    if (string_buf_pos < string_maxlen)
        string_buf_ptr[string_buf_pos++] = c;
}

int print_long_zstr_to_cstr(uint16_t addr, char *str, int maxlen) {
    int length = count_characters_in_zstring(addr);
    if (length < 2)
        return 0;
    string_buf_ptr = str;
    string_buf_pos = 0;
    string_maxlen = maxlen;
    print_handler(unpack_string(addr), print_to_string_buffer);
    str[length] = 0;
    return length;
}


int print_zstr_to_cstr(uint16_t addr, char *str) {
    return print_long_zstr_to_cstr(addr, str, 15);
}


static int print_tag_route_to_str(char *str) {
    int name_length = get_global(jg.TAG_NAME_LENGTH);
    int name_table = get_global(jg.NAME_TBL) + 4;
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

static void create_journey_party_menu(void) {

    fprintf(stderr, "create_journey_party_menu");
    int object;

    char str[15];
    int stringlength = 0;

    int line = 0;

    int column = 0;
    int table = get_global(jg.PARTY_COMMANDS);

    int table_count = user_word(table);

    for (int i = 1; i <= table_count; i++) {
        object = user_word(table + 2 * i);
        if (object == jo.TAG_ROUTE_COMMAND // TAG-ROUTE-COMMAND
            && get_global(jg.TAG_NAME_LENGTH) != 0) {
            stringlength = print_tag_route_to_str(str);
        } else {
            stringlength = print_zstr_to_cstr(user_word(object), str);
        }
        if (stringlength > 1)
            win_menuitem(kJMenuTypeParty, column, line, i == table_count, str, stringlength);
        line++;
    }
}

static void create_submenu(JourneyMenu *m, int object, int objectindex) {
    uint16_t command_table = word(get_global(jg.CHARACTER_INPUT_TBL) + objectindex * 2);
    if (!internal_test_attr(object, ja.SHADOW)) {
        m->submenu_entries = 0;
    }
    for (int i = 0; i <= 2; i++) {
        uint16_t command = user_word(command_table + i * 2);
        uint16_t str = word(command);
        if (str != 0 && count_characters_in_zstring(str) > 1) {
            if (m->submenu == nullptr) {
                m->submenu = (struct JourneyMenu *)malloc(sizeof(struct JourneyMenu) * MAX_SUBMENU_ITEMS);
            }
            struct JourneyMenu *submenu = &(m->submenu[m->submenu_entries]);
            submenu->length = print_zstr_to_cstr(str, submenu->name);
            submenu->line = m->line;
            m->submenu_entries++;
            if (m->submenu_entries > MAX_SUBMENU_ITEMS) {
                fprintf(stderr, "Error! Too manu submenu items\n");
                return;
            }
            submenu->column = m->column + i;
//            fprintf(stderr, "verb submenu item %d: \"%s\"\n", m->submenu_entries, submenu->name);
        }
    }
}

static void journey_create_menu(JourneyMenuType type, bool prsi) {

    struct JourneyMenu menu[10];

//    fprintf(stderr, "create_journey_menu\n");
    int table, table_count;
    if (type == kJMenuTypeObjects) {
        table = get_global(jg.O_TABLE) + (prsi ? 10 : 0);
        table_count = user_word(table);
    } else {
        table = get_global(jg.PARTY);
        table_count = 5;
    }

    struct JourneyMenu *m;

    bool subgroup = (get_global(jg.SUBGROUP_MODE) == 1);
    int menu_counter = 0;

    for (int i = 1; i <= table_count; i++) {
        uint16_t object = word(table + 2 * i);
        if (object != 0 && (!(subgroup && !internal_test_attr(object, ja.SUBGROUP)) || internal_test_attr(object, ja.SHADOW))) {

            m = &menu[menu_counter];

            int TAG_OBJECT = jo.TAG_OBJECT;
            int TAG = jo.TAG;
            int TAG_NAME_LENGTH = get_global(jg.TAG_NAME_LENGTH);
            int NAME_TBL = get_global(jg.NAME_TBL) + 2;

            if ((object == TAG_OBJECT || object == TAG) && TAG_NAME_LENGTH != 0) {
                for (uint16_t j = 0; j < TAG_NAME_LENGTH; j++) {
                    m->name[j] = user_byte(NAME_TBL++);
                }
                m->name[TAG_NAME_LENGTH] = 0;
                m->length = TAG_NAME_LENGTH;
            } else {
                uint16_t str = internal_get_prop(object, ja.SDESC);
                m->length = print_zstr_to_cstr(str, m->name);
            }

            m->submenu = nullptr;

            if (m->length > 1) {
                m->line = i - 1;
                if (type == kJMenuTypeMembers) {
                    m->column = 2;
                    // Hack to add "shadow" menus to the previous submenu
                    // instead of creating a new one.
                    // This assumes that the shadow menu is always the last one.
                    if (internal_test_attr(object, ja.SHADOW)) {
                        menu_counter--;
                        menu[menu_counter].line++;
                    }
                    create_submenu(&menu[menu_counter], object, i);
                } else {
                    m->column = 3 + ((menu_counter > 4 || prsi) ? 1 : 0);
                }
                menu_counter++;
                if (menu_counter >= 10)
                    return;
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
//                fprintf(stderr, "Sending submenu command %s line %d column %d stop %d length %d\n", submenu->name, submenu->line, submenu->column, (j == m->submenu_entries - 1), submenu->length);
                win_menuitem(kJMenuTypeVerbs, submenu->column, submenu->line, (j == m->submenu_entries - 1), submenu->name, submenu->length);
            }
            free(m->submenu);
            m->submenu = nullptr;
        }
    }
}

#pragma mark Input

static void journey_draw_cursor(void) {
    if (input_column > SCREEN_WIDTH_in_chars - 1)
        return;
    move_v6_cursor(input_column, input_line);

    if (FONT3_FLAG)
        garglk_set_reversevideo(1);
    else
        garglk_set_reversevideo(0);

    underscore_or_square();

    if (FONT3_FLAG)
        garglk_set_reversevideo(0);
    else
        garglk_set_reversevideo(1);


    move_v6_cursor(input_column, input_line);
}

static uint16_t journey_read_keyboard_line(int x, int y, uint16_t table, int max, bool elvish, uint8_t *kbd) {

    input_column = x + journey_input_length;
    input_line = y;
    max_length = max;
    input_table = table;
    from_command_start_line = y - get_global(jg.COMMAND_START_LINE);

    set_current_window(&JOURNEY_BG_GRID);
    move_v6_cursor(x, input_line);

    if (!FONT3_FLAG) {
        garglk_set_reversevideo(1);
    }

    for (int i = 0; i <= max_length; i++) {
        underscore_or_square();
    }

    move_v6_cursor(input_column, input_line);

    int start = input_column;

    win_menuitem(kJMenuTypeTextEntry, x, from_command_start_line, elvish, nullptr, 15);

    uint8_t character;

    journey_draw_cursor();

    while ((character = internal_read_char()) != ZSCII_NEWLINE) {
        if (FONT3_FLAG) {
            garglk_set_reversevideo(0);
        }
        if (character == 0x7f || character == ZSCII_BACKSPACE || character == ZSCII_LEFT) { // DELETE-KEY ,BACK-SPACE ,LEFT-ARROW
            if (journey_input_length == 0) {
                win_beep(1);
                continue;
            } else {
                if (input_column < SCREEN_WIDTH_in_chars - 1) {
                    move_v6_cursor(input_column, input_line);
                    underscore_or_square();
                } else {
                    move_v6_cursor(SCREEN_WIDTH_in_chars - 1, input_line);
                    underscore_or_square();
                }
                input_column--;
                journey_input_length--;
                journey_draw_cursor();
            }
        } else {
            
            if (journey_input_length == max_length || bad_character(character, elvish)) {
                win_beep(1);
                continue;
            }
            
            if (!elvish) {
                if (journey_input_length == 0) {
                    if (character >= 'a' && character <= 'z') {
                        character -= 32;
                    }
                    *kbd = character;
                } else if (character >= 'A' && character <= 'Z') {
                    character += 32;
                }
            }

            if (input_column < SCREEN_WIDTH_in_chars) {
                move_v6_cursor(input_column, input_line);
                put_char(character);
            }
            user_store_byte(table + journey_input_length, character);
            input_column++;
            journey_input_length++;
            journey_draw_cursor();
        }
    }

    if (elvish) {
        move_v6_cursor(start, input_line);
        for (int i = 0; i < max_length; i++) {
            put_char(UNICODE_SPACE);
        }
    }

    set_current_window(journey_text_buffer);
    journey_current_input = INPUT_PARTY;
    return journey_input_length;
}


static int GET_COMMAND(int cmd) {
    int COMMAND_WIDTH = get_global(jg.COMMAND_WIDTH);
    if (header.release > 50) {
        if (COMMAND_WIDTH < 13) {
            int STR = user_word(cmd + 10);
            if (STR)
                return STR;
        }
    }
    return user_word(cmd);
}

static void PRINT_COMMAND(int cmd) {
    print_handler(unpack_string(GET_COMMAND(cmd)), nullptr);
}

static int GET_DESC(int obj) {
    int STR;
    if (SCREEN_WIDTH_in_chars < 0x32) {
        STR = internal_get_prop(obj, ja.DESC8);
        if (STR)
            return STR;
    }
    if (SCREEN_WIDTH_in_chars < 0x47) {
        STR = internal_get_prop(obj, ja.DESC12);
        if (STR)
            return STR;
    }
    return (internal_get_prop(obj, ja.SDESC));
}


static int PRINT_DESC(int obj, bool cmd) {
    int NAME_TBL = get_global(jg.NAME_TBL) + 2;
    int TAG_NAME_LENGTH = get_global(jg.TAG_NAME_LENGTH);
    int TAG_OBJECT = jo.TAG_OBJECT;
    int TAG = jo.TAG;

    if ((obj == TAG_OBJECT || obj == TAG) && TAG_NAME_LENGTH != 0) {
        for (uint16_t j = 0; j < TAG_NAME_LENGTH; j++) {
            put_char(user_byte(NAME_TBL++));
        }
        return TAG_NAME_LENGTH;
    }

    uint16_t str;
    if (cmd) {
        str = internal_get_prop(obj, ja.SDESC);
    } else {
        str = GET_DESC(obj);
    }
    print_handler(unpack_string(str), nullptr);
    return count_characters_in_zstring(str);
}

static void journey_erase_command_chars(int LN, int COL, int num_spaces) {

    fprintf(stderr, "journey_erase_command_chars ln:%d col:%d spaces:%d\n", LN, COL, num_spaces);
    if (options.int_number == INTERP_MSDOS) {
        move_v6_cursor(COL - 1, LN);
    } else {
        move_v6_cursor(COL, LN);
    }

    bool in_rightmost_column = (COL > SCREEN_WIDTH_in_chars * 0.7);

    if (in_rightmost_column) {
        if (!FONT3_FLAG) {
            num_spaces++;
        } else if (options.int_number == INTERP_AMIGA) {
            num_spaces--;
        }
    }

    for (int i = 0; i < num_spaces; i++)
        glk_put_char_stream(JOURNEY_BG_GRID.id->str, UNICODE_SPACE);

    if (!in_rightmost_column) {
        if (!FONT3_FLAG) {
            garglk_set_reversevideo(1);
            glk_put_char_stream(JOURNEY_BG_GRID.id->str, UNICODE_SPACE);
            garglk_set_reversevideo(0);
        } else if (COL > SCREEN_WIDTH_in_chars * 0.5) {
            glk_set_style(style_BlockQuote);
            glk_put_char_stream(JOURNEY_BG_GRID.id->str, THIN_V_LINE);
            glk_set_style(style_Normal);
        }
    }


    move_v6_cursor(COL, LN);
}


static void journey_erase_command_pixels(int pix) {
    fprintf(stderr, "journey_erase_command_pixels:%d\n", pix);

    glk_window_move_cursor(curwin->id, curwin->x, curwin->y);

    int NAME_WIDTH_PIX = get_global(jg.NAME_WIDTH_PIX);

    int COMMAND_WIDTH = get_global(jg.COMMAND_WIDTH);
    int NAME_WIDTH = get_global(jg.NAME_WIDTH);

    int num_spaces = COMMAND_WIDTH - 1;
    if (pix == NAME_WIDTH_PIX) {
        num_spaces = NAME_WIDTH - 1;
    }

    if (options.int_number == INTERP_MSDOS) {
        num_spaces--;
    }

    if (curwin->x > SCREEN_WIDTH_in_chars * 0.7) {
        if (!FONT3_FLAG) {
            num_spaces++;
        } else if (options.int_number == INTERP_AMIGA) {
            num_spaces--;
        }
    }

    for (int i = 0; i < num_spaces; i++)
        glk_put_char(UNICODE_SPACE);

    glk_window_move_cursor(curwin->id, curwin->x, curwin->y);
}

static void journey_print_character_commands(bool CLEAR) {
    fprintf(stderr, "print_journey_character_commands: CLEAR: %s\n", CLEAR ? "true" : "false");

    // Prints the character names and arrows, and their commands in the three rightmost columns.
    // If CLEAR is true, the three columns to the right of the character names will be cleared.

    if (CLEAR)
        number_of_printed_journey_words = 0;

    int LN = get_global(jg.COMMAND_START_LINE); // COMMAND-START-LINE
    int PTBL, CHR, POS;
    int UPDATE_FLAG = get_global(jg.UPDATE_FLAG);
    if (UPDATE_FLAG && !CLEAR) {
        internal_call(pack_routine(jr.FILL_CHARACTER_TBL));
    }

    if (!CLEAR)
        journey_create_menu(kJMenuTypeMembers, false);

    PTBL = get_global(jg.PARTY);

    set_current_window(&JOURNEY_BG_GRID);

    int COMMAND_WIDTH = get_global(jg.COMMAND_WIDTH);
    int NAME_WIDTH = get_global(jg.NAME_WIDTH);
    int NAME_RIGHT = get_global(jg.CHR_COMMAND_COLUMN) - 2;

    // Print up to 5 character names and arrows
    for (int i = 1; i <= 5; i++) {
        CHR = word(PTBL + 2 * i); // <GET .PTBL 1>
        POS = get_global(jg.NAME_COLUMN); // NAME-COLUMN

        journey_erase_command_chars(LN, POS, NAME_WIDTH - 1);

        // global 0x80 is SUBGROUP-MODE
        // attribute 0x2a is SUBGROUP flag
        if (CHR != 0 && !(get_global(jg.SUBGROUP_MODE) && !internal_test_attr(CHR, ja.SUBGROUP))) {
            int namelength = PRINT_DESC(CHR, false);

            uint16_t LONG_ARROW_WIDTH = 3;
            uint16_t SHORT_ARROW_WIDTH = 2;
            uint16_t NO_ARROW_WIDTH = 1;

            if (SCREEN_WIDTH_in_chars < 55) { // (<L? ,SCREEN-WIDTH ,8-WIDTH

                if (NAME_WIDTH - namelength - 2 < SHORT_ARROW_WIDTH) {
                    move_v6_cursor(NAME_RIGHT - NO_ARROW_WIDTH, LN);
                    glk_put_string(const_cast<char*>(">"));
                } else {
                    move_v6_cursor(NAME_RIGHT - SHORT_ARROW_WIDTH, LN);
                    glk_put_string(const_cast<char*>("->"));
                }

            } else {
                move_v6_cursor(NAME_RIGHT - LONG_ARROW_WIDTH, LN);
                glk_put_string(const_cast<char*>("-->"));
            }
        }

        if (journey_current_input == INPUT_PARTY) {
            POS = get_global(jg.CHR_COMMAND_COLUMN);

            // CHARACTER-INPUT-TBL contains pointers to tables of up to three verbs
            uint16_t BTBL = word(get_global(jg.CHARACTER_INPUT_TBL) + i * 2);

            bool SUBGROUP_MODE = (get_global(jg.SUBGROUP_MODE) == 1);
            bool SUBGROUP = internal_test_attr(CHR, ja.SUBGROUP); // attribute 0x2a is SUBGROUP flag
            // if the SHADOW_BIT of a character is set, the character name is hidden
            // and its commands "belong" to the visible character above.
            // This is used to give Praxis more than three commands in the mines.
            bool SHADOW = internal_test_attr(CHR, ja.SHADOW); // attribute 0x17 is SHADOW flag

            bool should_print_command = true;

            if (CLEAR || CHR == 0) {
                should_print_command = false;
            // The SHADOW bit trumps subgroup mode: if it is set,
            // commands will be printed even though the character
            // does not belong to the subgroup.
            } else if ((SUBGROUP_MODE && SUBGROUP) || SHADOW) {
                should_print_command = true;
            }

            // Print up to three verbs for each character to the right, or just erase the fields
            for (int j = 0; j <= 2; j++) {
                journey_erase_command_chars(LN, POS, COMMAND_WIDTH - 1);

                if (should_print_command) {
                    PRINT_COMMAND(word(BTBL + j * 2));
                }

                POS += COMMAND_WIDTH;
            }
        }

        LN++;
    }


    if (get_global(jg.SMART_DEFAULT_FLAG) == 1) { // SMART-DEFAULT-FLAG
        set_global(jg.SMART_DEFAULT_FLAG, 0);
        internal_call(pack_routine(jr.SMART_DEFAULT)); //    SMART_DEFAULT();
    }

    set_current_window(&windows[ja.buffer_window_index]);
}

bool journey_read_elvish(int actor) {
    //  actor is set to 0x78 (Tag) by default

    int COL = get_global(jg.COMMAND_OBJECT_COLUMN); // COMMAND-OBJECT-COLUMN
    journey_print_character_commands(true); // <CLEAR-FIELDS>

    user_store_byte(get_global(jg.E_LEXV), 0x14); // <PUTB ,E-LEXV 0 20>
    user_store_byte(get_global(jg.E_INBUF), 0x32); // <PUTB ,E-INBUF 0 50>

    int LN = get_global(jg.COMMAND_START_LINE) + party_pcm(actor) - 1; // COMMAND-START-LINE

    move_v6_cursor(get_global(jg.CHR_COMMAND_COLUMN), LN);
    glk_put_string_stream(JOURNEY_BG_GRID.id->str, const_cast<char*>("says..."));

    int MAX = SCREEN_WIDTH_in_chars - get_global(jg.COMMAND_OBJECT_COLUMN) - 2;

    uint16_t TBL = get_global(jg.E_TEMP); // <SET TBL ,E-TEMP>

    uint16_t offset = journey_read_keyboard_line(COL, LN, TBL, MAX, true, nullptr);

    journey_input_length = 0;

    journey_refresh_character_command_area(get_global(jg.COMMAND_START_LINE) - 1); // <REFRESH-CHARACTER-COMMAND-AREA <- ,COMMAND-START-LINE 1>>

    set_global(jg.UPDATE_FLAG, 1); // <SETG UPDATE-FLAG T>

    if (offset == 0)
        return false;

    internal_call_with_arg(pack_routine(jr.MASSAGE_ELVISH), offset);  // <MASSAGE-ELVISH .OFF>
    set_global(jg.E_TEMP_LEN, offset); // <SETG E-TEMP-LEN .OFF>

    tokenize(get_global(jg.E_INBUF),get_global(jg.E_LEXV), 0, false);
    if (user_byte(get_global(jg.E_LEXV) + 1) == 0) // <ZERO? <GETB ,E-LEXV 1>>
        return false;
    if (actor == jo.PRAXIX || actor == jo.BERGON) // PRAXIX ,BERGON
        return true;
    internal_call(pack_routine(jr.PARSE_ELVISH)); // <PARSE-ELVISH>
    return true;
}

void journey_change_name() {
    int MAX;
    if (SCREEN_WIDTH_in_chars < 50) // 8-WIDTH
        MAX = 5;
    else
        MAX = 8;
    
    int COL = get_global(jg.NAME_COLUMN); // NAME-COLUMN
    int LN = get_global(jg.COMMAND_START_LINE) + party_pcm(jo.TAG) - 1; // <SET LN <- <+ ,COMMAND-START-LINE <PARTY-PCM ,TAG>> 1>
    uint16_t TBL = get_global(jg.NAME_TBL) + 2; // NAME-TBL

    uint8_t key = 0;
    uint16_t offset = journey_read_keyboard_line(COL, LN, TBL, MAX, false, &key);

    journey_input_length = 0;

    if (offset == 0) {
        set_global(jg.UPDATE_FLAG, 1); // <SETG UPDATE-FLAG T>
    } else if (internal_call_with_arg(pack_routine(jr.ILLEGAL_NAME), offset) == 1) { // ILLEGAL-NAME
        glk_put_string(const_cast<char*>("[The name you have chosen is reserved. Please try again.]"));
    } else {
        // Do the change
        internal_call(pack_routine(jr.REMOVE_TRAVEL_COMMAND));

        public_encode_text(TBL, offset, get_global(jg.TAG_NAME)); //  <ZWSTR ,NAME-TBL .OFF 2 ,TAG-NAME>

        // Set which single letter key to use for selecting protagonist
        internal_put_prop(jo.TAG_OBJECT, ja.KBD, key); // <PUTP ,TAG-OBJECT ,P?KBD .KBD>

        set_global(jg.TAG_NAME_LENGTH, offset); // <SETG TAG-NAME-LENGTH .OFF>
        set_global(jg.UPDATE_FLAG, 1); // <SETG UPDATE-FLAG T>
    }
}

void journey_init_screen(void) {
    Window *lastwin = curwin;
    set_current_window(&JOURNEY_BG_GRID);
    glk_window_clear(JOURNEY_BG_GRID.id);

    update_internal_globals();

    glk_set_style(style_Normal);
    garglk_set_reversevideo(0);

    int TOP_SCREEN_LINE = get_global(jg.TOP_SCREEN_LINE);
    int COMMAND_START_LINE = get_global(jg.COMMAND_START_LINE);

    journey_setup_windows();
    int TEXT_WINDOW_LEFT = get_global(jg.TEXT_WINDOW_LEFT);


    // Draw a line at the top with a centered "JOURNEY"
    if (BORDER_FLAG) {
        journey_font3_line(1, H_LINE, 47, 48);
        int x = SCREEN_WIDTH_in_chars / 2 - 2;
        move_v6_cursor(x, 1);
        glk_put_string(const_cast<char*>("JOURNEY"));
    }

    // Draw vertical divider between graphics window
    // and buffer text output window

    // Starts at TOP-SCREEN-LINE which is 2 on Amiga (because of border)
    // otherwise 1
    int LN;
    for (LN = TOP_SCREEN_LINE; LN != COMMAND_START_LINE - 1; LN++) {
        if (!BORDER_FLAG) {
            move_v6_cursor(TEXT_WINDOW_LEFT - 1, LN);
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
            move_v6_cursor(0, LN);
            glk_put_char(THIN_V_LINE);
            move_v6_cursor(TEXT_WINDOW_LEFT - 1, LN);
            glk_put_char(THIN_V_LINE);
            move_v6_cursor(SCREEN_WIDTH_in_chars, LN);
            glk_put_char(40);
            glk_set_style(style_Normal);
        }
    };

    // Draw horizontal line above "command area"
    if (FONT3_FLAG) {
        if (BORDER_FLAG) {
            journey_font3_line(LN, H_LINE, THIN_V_LINE, 40);
        } else {
            journey_font3_line(LN, H_LINE, H_LINE, H_LINE);
        }
    } else {
        move_v6_cursor(0, LN);
        garglk_set_reversevideo(1);
        for (int i = 0; i < SCREEN_WIDTH_in_chars; i++)
            glk_put_char(UNICODE_SPACE);
        garglk_set_reversevideo(0);
    }

    // Draw bottom border line
    if (BORDER_FLAG) {
        journey_font3_line(SCREEN_HEIGHT_in_chars, 38, 46, 49);
    }

    if (!FONT3_FLAG) {
        garglk_set_reversevideo(1);
    }

    int NAME_COLUMN = get_global(jg.NAME_COLUMN);
    int NAME_WIDTH = get_global(jg.NAME_WIDTH);

    int WIDTH = 9; //  WIDTH = TEXT_WIDTH("The Party");

    // Print "The Party" centered over the name column
    int x = NAME_COLUMN + (NAME_WIDTH - WIDTH) / 2 - 1;
    move_v6_cursor(x, LN);
    glk_put_string(const_cast<char*>("The Party"));

    // Print "Individual Commands" centered in the empty space to the right of "The Party" text
    WIDTH = 19; // WIDTH = TEXT_WIDTH("Individual Commands");

    int CHR_COMMAND_COLUMN = get_global(jg.CHR_COMMAND_COLUMN);
    move_v6_cursor(CHR_COMMAND_COLUMN + (SCREEN_WIDTH_in_chars - CHR_COMMAND_COLUMN - WIDTH) / 2 + (FONT3_FLAG ? 1 : 0),  LN);

    glk_put_string(const_cast<char*>("Individual Commands"));

    if (!FONT3_FLAG) {
        garglk_set_reversevideo(0);
    }
    set_current_window(lastwin);
}

void TAG_ROUTE_PRINT(void) {
    int TAG_NAME_LENGTH = get_global(jg.TAG_NAME_LENGTH);
    int NAME_TBL = get_global(jg.NAME_TBL) + 4;

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
    if (journey_current_input == INPUT_OBJECT || journey_current_input == INPUT_SPECIAL) {
        journey_current_input = INPUT_PARTY;
        number_of_printed_journey_words = 0;
    }
    journey_print_character_commands(variable(1) == 1 ? true : false);
}


void READ_ELVISH(void) {
    journey_current_input = INPUT_ELVISH;
    store_variable(1, (journey_read_elvish(variable(1)) ? 1 : 0));
}

void CHANGE_NAME(void) {
    journey_current_input = INPUT_NAME;
    journey_change_name();
}

void ERASE_COMMAND(void) {
    journey_erase_command_pixels(variable(1));
}

static void journey_print_columns(bool PARTY, bool PRSI) {
    int column, table, object;
    int line = get_global(jg.COMMAND_START_LINE);

    int COMMAND_WIDTH = get_global(jg.COMMAND_WIDTH);
    int O_TABLE = get_global(jg.O_TABLE); // O-TABLE (object table)
    set_current_window(&JOURNEY_BG_GRID);

    if (PARTY) {
        column = get_global(jg.PARTY_COMMAND_COLUMN);
        table = get_global(jg.PARTY_COMMANDS);
    } else  {
        column = get_global(jg.COMMAND_OBJECT_COLUMN) + (PRSI ? COMMAND_WIDTH : 0);
        table = O_TABLE + (PRSI ? 10 : 0);;
        journey_create_menu(kJMenuTypeObjects, PRSI);
    }

    int table_count = user_word(table);

    for (int i = 1; i <= table_count; i++) {
        object = user_word(table + 2 * i);
        journey_erase_command_chars(line, column, COMMAND_WIDTH - 1);
        if (PARTY) {
            if (object == jo.TAG_ROUTE_COMMAND // TAG-ROUTE-COMMAND
                && get_global(jg.TAG_NAME_LENGTH) // TAG-NAME-LENGTH (G1b)
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
            line = get_global(jg.COMMAND_START_LINE); // COMMAND-START-LINE (G0e)
        }
    }
    journey_refresh_character_command_area(get_global(jg.COMMAND_START_LINE) - 1);
}

void PRINT_COLUMNS(void) {
    fprintf(stderr, "PRINT-COLUMNS: Local variable 0:%d Local variable 1:%d \n", variable(1), variable(2));
    if (variable(1) == 1) {
        journey_current_input = INPUT_PARTY;
        create_journey_party_menu();
    } else if (variable(2) == 1) {
        journey_current_input = INPUT_SPECIAL;
    } else {
        journey_current_input = INPUT_OBJECT;
    }
    journey_print_columns(variable(1), variable(2));
}

static int journey_refresh_character_command_area(int16_t LN) {
    fprintf(stderr, "called refresh_journey_character_command_area(LN %d)\n", LN);
    int16_t POS;

    update_internal_globals();

    int COMMAND_START_LINE = get_global(jg.COMMAND_START_LINE);

    // Width of a column (except Name column) in characters
    int COMMAND_WIDTH = get_global(jg.COMMAND_WIDTH);
    // Width of a column (except Name column) in pixels
    int COMMAND_WIDTH_PIX = get_global(jg.COMMAND_WIDTH_PIX);

    int NAME_WIDTH = get_global(jg.NAME_WIDTH);

    int PARTY_COMMAND_COLUMN = get_global(jg.PARTY_COMMAND_COLUMN);

    Window *lastwin = curwin;
    set_current_window(&JOURNEY_BG_GRID);

    while (++LN <= COMMAND_START_LINE + 4) {
        POS = 1;
        move_v6_cursor(POS, LN);
        while (POS <= SCREEN_WIDTH_in_chars) {
            if (FONT3_FLAG) {
                if (POS != 1 && POS < SCREEN_WIDTH_in_chars - 5) {
                    glk_set_style(style_BlockQuote);
                    move_v6_cursor(POS, LN);
                    if (POS == COMMAND_WIDTH || POS == COMMAND_WIDTH + 1 || POS == COMMAND_WIDTH + NAME_WIDTH + 1 || POS == COMMAND_WIDTH + NAME_WIDTH) {
                        glk_put_char(THICK_V_LINE);
                    } else {
                        glk_put_char(THIN_V_LINE);
                    }
                    glk_set_style(style_Normal);
                } else if (POS == 1 && BORDER_FLAG) {
                    glk_set_style(style_BlockQuote);
                    move_v6_cursor(POS, LN);
                    glk_put_char(THIN_V_LINE);
                    glk_set_style(style_Normal);
                }
            } else if (POS != 1 && POS < SCREEN_WIDTH_in_chars - 5) {
                move_v6_cursor(POS - 1, LN);
                garglk_set_reversevideo(1);
                glk_put_char(UNICODE_SPACE);
                garglk_set_reversevideo(0);
            }

            if (POS == COMMAND_WIDTH || POS == COMMAND_WIDTH + 1) {
                POS += NAME_WIDTH;
            } else {
                if (COMMAND_WIDTH_PIX == 0) {
                    move_v6_cursor(PARTY_COMMAND_COLUMN, LN);
                }
                POS += COMMAND_WIDTH;
            }
        }

        if (BORDER_FLAG) {
            glk_set_style(style_BlockQuote);
            move_v6_cursor(SCREEN_WIDTH_in_chars, LN);
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
    journey_refresh_character_command_area(LN);
}

//static void print_globals(void) {
//    fprintf(stderr, "Let's only support r83 for now\n");
//    fprintf(stderr, "MOUSETBL is global 0xbd (0x%x)\n", get_global(0xbd));
//    fprintf(stderr, "MOUSE-INFO-TBL is global 0x16 (0x%x)\n", get_global(0x16));
//
//    fprintf(stderr, "SAVED-PCM is 0x%x (%d)\n", get_global(0x08), (int16_t)get_global(0x08));
//    fprintf(stderr, "SAVED-PCF is 0x%x (%d)\n", get_global(0x98), (int16_t)get_global(0x98));
//    fprintf(stderr, "BORDER-FLAG is global 0x9f (%x)\n", get_global(0x9f));
//    fprintf(stderr, "FONT3-FLAG is global 0x31 (%x)\n", get_global(0x31));
//    fprintf(stderr, "FWC-FLAG is global 0xb9 (%x)\n", get_global(0xb9));
//    fprintf(stderr, "BLACK-PICTURE-BORDER is global 0x72 (%x)\n", get_global(0x72));
//    fprintf(stderr, "CHRH is global 0x92 (0x%x) (%d)\n", get_global(0x92), get_global(0x92));
//    fprintf(stderr, "gcellw is %f\n", gcellw);
//    fprintf(stderr, "CHRV is global 0x89 (0x%x) (%d)\n", get_global(0x89), get_global(0x89));
//    fprintf(stderr, "gcellh is %f\n", gcellh);
//    fprintf(stderr, "SCREEN-WIDTH is global 0x83 (0x%x) (%d)\n", get_global(0x83), get_global(0x83));
//    fprintf(stderr, "gscreenw is %d\n", gscreenw);
//    fprintf(stderr, "gscreenw / gcellw is %f\n", gscreenw / gcellw);
//
//    fprintf(stderr, "SCREEN-HEIGHT is global 0x64 (0x%x) (%d)\n", get_global(0x64), get_global(0x64));
//    fprintf(stderr, "gscreenh is %d\n", gscreenh);
////    fprintf(stderr, "gscreenh / letterheight is %d\n", gscreenh / letterheight);
//    fprintf(stderr, "gscreenh / gcellh is %f\n", gscreenh / gcellh);
//
//    fprintf(stderr, "TOP-SCREEN-LINE is global 0x25 (0x%x) (%d)\n", get_global(0x25), get_global(0x25));
//    fprintf(stderr, "COMMAND-START-LINE is global 0x0e (0x%x) (%d)\n", get_global(0x0e), get_global(0x0e));
//    fprintf(stderr, "COMMAND-WIDTH is global 0xb8 (0x%x) (%d)\n", get_global(0xb8), get_global(0xb8));
//    fprintf(stderr, "COMMAND-WIDTH-PIX is global 0x6d (0x%x) (%d)\n", get_global(0x6d), get_global(0x6d));
//    fprintf(stderr, "NAME-WIDTH is global 0xa2 (0x%x) (%d)\n", get_global(0xa2), get_global(0xa2));
//    fprintf(stderr, "NAME-WIDTH-PIX is global 0x82 (0x%x) (%d)\n", get_global(0x82), get_global(0x82));
//    fprintf(stderr, "NAME-RIGHT is global 0x2d (0x%x) (%d)\n", get_global(0x2d), get_global(0x2d));
//
//    fprintf(stderr, "PARTY-COMMAND-COLUMN is global 0xb4 (0x%x) (%d)\n", get_global(0xb4), get_global(0xb4));
//    fprintf(stderr, "NAME-COLUMN is global 0xa3 (0x%x) (%d)\n", get_global(0xa3), get_global(0xa3));
//    fprintf(stderr, "CHR-COMMAND-COLUMN is global 0x28 (0x%x) (%d)\n", get_global(0x28), get_global(0x28));
//    fprintf(stderr, "COMMAND-OBJECT-COLUMN is global 0xb0 (0x%x) (%d)\n", get_global(0xb0), get_global(0xb0));
//}

void stash_journey_state(library_state_data *dat) {
    if (!dat)
        return;

    dat->selected_journey_line = selected_journey_line;
    dat->selected_journey_column = selected_journey_column;
    dat->current_input_mode = journey_current_input;
    dat->current_input_length = journey_input_length;
    dat->number_of_journey_words = number_of_printed_journey_words;
    for (int i = 0; i < number_of_printed_journey_words; i++) {
        dat->journey_words[i].str = printed_journey_words[i].str;
        dat->journey_words[i].pcf = printed_journey_words[i].pcf;
        dat->journey_words[i].pcm = printed_journey_words[i].pcm;
    }
}

void recover_journey_state(library_state_data *dat) {
    if (!dat)
        return;
    selected_journey_line = dat->selected_journey_line;
    selected_journey_column = dat->selected_journey_column;
    journey_current_input = dat->current_input_mode;
    journey_input_length = dat->current_input_length;
    number_of_printed_journey_words = dat->number_of_journey_words;
    for (int i = 0; i < number_of_printed_journey_words; i++) {
        printed_journey_words[i].str = dat->journey_words[i].str;
        printed_journey_words[i].pcf = dat->journey_words[i].pcf;
        printed_journey_words[i].pcm = dat->journey_words[i].pcm;
    }
}

static void journey_reprint_partial_input(int x, int y, int length_so_far, int max_length, int16_t table_address) {
    move_v6_cursor(x, y);
    glk_set_style(style_Normal);
    if (!FONT3_FLAG) {
        garglk_set_reversevideo(1);
    }

    int max_screen = SCREEN_WIDTH_in_chars - 1 - (options.int_number == INTERP_AMIGA ? 1 : 0);
    if (max_length + x > max_screen)
        max_length = max_screen - x + 1;
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

void journey_update_after_restore() {
    win_menuitem(kJMenuTypeDeleteAll, 0, 0, false, nullptr, 15);
    if (jo.START_LOC != 0 && ja.SEEN != 0) {
        internal_set_attr(jo.START_LOC, ja.SEEN);
    }
    screenmode = MODE_NORMAL;
    journey_current_input = INPUT_PARTY;
    JOURNEY_BG_GRID.x = 0;
    JOURNEY_BG_GRID.y = 0;
    set_global(jg.CHRH, 1); // GLOBAL CHRH
    set_global(jg.CHRV, 1); // GLOBAL CHRV
    journey_setup_windows();
    journey_adjust_windows(true);
}

void journey_update_after_autorestore() {
    fprintf(stderr, "journey_update_after_autorestore\n");
    // Do not redraw
    store_word(0x10, word(0x10) & ~FLAGS2_STATUS);
}

bool journey_autorestore_internal_read_char_hacks(void) {
    update_screen_size();
    if (screenmode == MODE_SLIDESHOW) {
        if (jo.START_LOC != 0 && ja.SEEN != 0) {
            internal_clear_attr(jo.START_LOC, ja.SEEN);
        }
        screenmode = MODE_INITIAL_QUESTION;
        return true;
    }

    if (screenmode == MODE_NORMAL &&
        (journey_current_input == INPUT_NAME ||
         journey_current_input == INPUT_ELVISH)) {
        return true;
    }
    return false;
}


static void journey_resize_graphics_and_buffer_windows(void) {

    int text_window_left = get_global(jg.TEXT_WINDOW_LEFT);

    if (BORDER_FLAG) {
        JOURNEY_GRAPHICS_WIN.x_origin = ggridmarginx + gcellw + 5;
        JOURNEY_GRAPHICS_WIN.y_origin = ggridmarginy + gcellh + 1;
    } else {
        JOURNEY_GRAPHICS_WIN.x_origin = ggridmarginx + 1;
        JOURNEY_GRAPHICS_WIN.y_origin = ggridmarginy + 1;
    }

    if (SCREEN_HEIGHT_in_chars < 5)
        update_screen_size();

    JOURNEY_GRAPHICS_WIN.x_size = (float)(text_window_left - 2) * gcellw - JOURNEY_GRAPHICS_WIN.x_origin + ggridmarginx + (FONT3_FLAG ? 0 : 2);

    if (journey_text_buffer == nullptr) {
        journey_text_buffer = &windows[ja.buffer_window_index];
    }

    int command_start_line = SCREEN_HEIGHT_in_chars - 4 - (BORDER_FLAG ? 1 : 0);

    uint16_t x_size = (SCREEN_WIDTH_in_chars - text_window_left + 1) * gcellw;
    uint16_t y_size = (command_start_line - 2) * gcellh - FONT3_FLAG;

    if (BORDER_FLAG) {
        x_size -= gcellw;
        y_size -= gcellh;
    }

    v6_define_window(journey_text_buffer, JOURNEY_GRAPHICS_WIN.x_origin + JOURNEY_GRAPHICS_WIN.x_size + gcellw + (FONT3_FLAG ? 3 : 0), JOURNEY_GRAPHICS_WIN.y_origin, x_size, y_size);

    JOURNEY_GRAPHICS_WIN.y_size = y_size;

    glk_window_set_background_color(JOURNEY_GRAPHICS_WIN.id, monochrome_black);

    v6_sizewin(&JOURNEY_GRAPHICS_WIN);
}

#pragma mark adjust_journey_windows

static void journey_adjust_windows(bool restoring) {
    fprintf(stderr, "journey_adjust_windows(%s)\n", restoring ? "true" : "false");

    // Window 0: Text buffer (in later versions) Receives most keypresses.
    // Window 1: Fullscreen grid window
    //(Window 2: Used in place of window 0 in earlier versions)
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

    v6_define_window(&JOURNEY_BG_GRID, 1, 1, gscreenw, gscreenh);

    JOURNEY_BG_GRID.style.reset(STYLE_REVERSE);

    // Draw borders, depending on interpreter setting
    journey_init_screen();

    // Redraw vertical lines in command area (the bottom five rows)
    journey_refresh_character_command_area(SCREEN_HEIGHT_in_chars - 5 - BORDER_FLAG);

    if (screenmode == MODE_NORMAL) {
        set_global(jg.SMART_DEFAULT_FLAG, 0); // SMART-DEFAULT-FLAG

        // Print party (the leftmost) column (call print columns with PARTY flag set
        journey_print_columns(true, false);

        if (journey_current_input != INPUT_PARTY) {
            // If the player is currently entering text, redraw typed text at new position
            if (journey_current_input == INPUT_NAME ) {
                journey_reprint_partial_input(get_global(jg.NAME_COLUMN), get_global(jg.COMMAND_START_LINE) + from_command_start_line, journey_input_length, max_length, input_table);
            } else if (journey_current_input == INPUT_ELVISH ) {
                move_v6_cursor(get_global(jg.CHR_COMMAND_COLUMN), get_global(jg.COMMAND_START_LINE) + from_command_start_line);
                glk_put_string_stream(JOURNEY_BG_GRID.id->str, const_cast<char*>("says..."));
                journey_reprint_partial_input(get_global(jg.COMMAND_OBJECT_COLUMN), get_global(jg.COMMAND_START_LINE) + from_command_start_line, journey_input_length, max_length - 1, input_table);
            } else {
                journey_print_columns(false, journey_current_input == INPUT_SPECIAL);
            }
        }

        if (journey_current_input != INPUT_NAME) {
            // Print character names and arrows, and corresponding verbs in the three rightmost columns
            journey_print_character_commands(false);
        }

        // reset bit 2 in LOWCORE FLAGS, no screen redraw needed
        store_word(0x10, word(0x10) & ~FLAGS2_STATUS);

        if (!restoring && screenmode != MODE_CREDITS) {
            int16_t saved_line = selected_journey_line;
            int16_t saved_column = selected_journey_column;

            if (selected_journey_column <= 0) { // call BOLD-PARTY-CURSOR
                internal_call_with_2_args(pack_routine(jr.BOLD_PARTY_CURSOR), selected_journey_line, 0);
            } else if (journey_current_input == INPUT_PARTY) { // call BOLD-CURSOR
                internal_call_with_2_args(pack_routine(jr.BOLD_CURSOR), selected_journey_line, selected_journey_column);

            } else if (journey_current_input != INPUT_ELVISH) { // call BOLD-OBJECT-CURSOR
                int numwords = number_of_printed_journey_words;

                for (int i = 0; i < numwords; i++) {
                    JourneyWords *word = &printed_journey_words[i];
                    uint16_t args[3] = {word->pcm, word->pcf, word->str};
                    internal_call_with_args(pack_routine(jr.BOLD_CURSOR), 3, args);
                }
                internal_call_with_2_args(pack_routine(jr.BOLD_OBJECT_CURSOR), saved_line, saved_column); // BOLD-OBJECT-CURSOR(PCM, PCF)
            }

            selected_journey_line = saved_line;
            selected_journey_column = saved_column;
        }
    }
    journey_resize_graphics_and_buffer_windows();

    // Redraw image(s)
    // (unless we have no graphics)
    if (JOURNEY_GRAPHICS_WIN.id != nullptr) {
        glk_window_clear(JOURNEY_GRAPHICS_WIN.id);
        internal_call(pack_routine(jr.GRAPHIC));

        uint16_t HERE = get_global(jg.HERE);
        if (HERE == 216) // Control Room
            internal_call(pack_routine(jr.COMPLETE_DIAL_GRAPHICS));
    }

    if (journey_current_input == INPUT_NAME || journey_current_input == INPUT_ELVISH) {

        if (journey_current_input == INPUT_NAME) {
            input_column = get_global(jg.NAME_COLUMN) + journey_input_length;
        } else {
            input_column = get_global(jg.COMMAND_OBJECT_COLUMN) + journey_input_length;
        }

        input_line = get_global(jg.COMMAND_START_LINE) + from_command_start_line;

        set_current_window(&JOURNEY_BG_GRID);

        journey_draw_cursor();
    }
}

void redraw_vertical_lines_if_needed(void) {
    if (options.int_number != INTERP_MSDOS ||
        journey_current_input == INPUT_ELVISH ||
        (jg.NAME_WIDTH_PIX != 0 && jg.COMMAND_WIDTH_PIX != 0))
        return;
    journey_refresh_character_command_area(SCREEN_HEIGHT_in_chars - 5 - BORDER_FLAG);
}

void BOLD_CURSOR(void) {
    fprintf(stderr, "BOLD-CURSOR(");
    selected_journey_line = variable(1);
    selected_journey_column = variable(2);

    if (number_of_printed_journey_words < 4) {
        JourneyWords *word = &printed_journey_words[number_of_printed_journey_words++];
        word->pcm = variable(1);
        word->pcf = variable(2);
        word->str = variable(3);
    }
    redraw_vertical_lines_if_needed();
}

void BOLD_PARTY_CURSOR(void) {
    selected_journey_line = variable(1);
    selected_journey_column = variable(2);
    redraw_vertical_lines_if_needed();
}

void BOLD_OBJECT_CURSOR(void) {
    fprintf(stderr, "BOLD-OBJECT-CURSOR(");
    selected_journey_line = variable(1);
    selected_journey_column = variable(2);
    redraw_vertical_lines_if_needed();
}

void after_INTRO(void) {
    fprintf(stderr, "after_INTRO\n");
    screenmode = MODE_NORMAL;
}

void GRAPHIC_STAMP(void) {
    int16_t picnum, where = 0;
    if (znargs > 0 && JOURNEY_GRAPHICS_WIN.id != nullptr) {
        picnum = variable(1);
        if (znargs > 1)
            where = variable(2);
        draw_journey_stamp_image(JOURNEY_GRAPHICS_WIN.id, picnum, where, pixelwidth);
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
    if (!qset(jo.START_LOC, ja.SEEN) && screenmode == MODE_INITIAL_QUESTION) {
        // Show title image and wait for key press
        if (journey_text_buffer == NULL)
            journey_text_buffer = &windows[ja.buffer_window_index];
        glk_window_clear(journey_text_buffer->id);
        screenmode = MODE_SLIDESHOW;
        // We do a fake resize event to draw the title image
        journey_update_on_resize();
        glk_request_mouse_event(JOURNEY_GRAPHICS_WIN.id);
        glk_request_char_event(curwin->id);
        internal_read_char();
        screenmode = MODE_CREDITS;
        journey_adjust_windows(false);
        win_setbgnd(-1, user_selected_background);
    } else if (screenmode != MODE_CREDITS){
        journey_init_screen();
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
    char str[1024];
    print_zstr_to_cstr(stringnum, str);
    str[7] = 0;
    if (strcmp(str, "JOURNEY") == 0)
        glk_set_style(style_User1);
    else
        glk_set_style(style_User2);
    print_handler(unpack_string(stringnum), nullptr);
    glk_set_style(style_Normal);
}

void COMPLETE_DIAL_GRAPHICS(void) {}

void journey_update_on_resize(void) {
    fprintf(stderr, "journey_update_on_resize\n");
    // Window 0: Text buffer (in later versions)
    // Window 1: Fullscreen grid window
    //(Window 2: Used in place of window 0 in earlier versions)
    // Window 3: Graphics window

    if (jg.INTERPRETER != 0)
        set_global(jg.INTERPRETER, options.int_number); // GLOBAL INTERPRETER

    set_global(jg.CHRH, 1); // GLOBAL CHRH
    set_global(jg.CHRV, 1); // GLOBAL CHRV

    if (screenmode == MODE_INITIAL_QUESTION) {
        if (journey_text_buffer == nullptr)
            journey_text_buffer = &windows[ja.buffer_window_index];
        journey_text_buffer->x_size = gscreenw;
        journey_text_buffer->y_size = gscreenh;
        v6_sizewin(journey_text_buffer);
        JOURNEY_GRAPHICS_WIN.x_size = 0;
        JOURNEY_GRAPHICS_WIN.y_size = 0;
        v6_sizewin(&JOURNEY_GRAPHICS_WIN);
    } else if (screenmode == MODE_SLIDESHOW) {
        JOURNEY_GRAPHICS_WIN.x_size = gscreenw;
        JOURNEY_GRAPHICS_WIN.y_size = gscreenh;

        v6_sizewin(&JOURNEY_GRAPHICS_WIN);

        draw_journey_title_image();
    } else {
        journey_adjust_windows(false);
        glui32 result;
        glk_style_measure(JOURNEY_BG_GRID.id, style_Normal, stylehint_BackColor, &result);
        win_setbgnd(-1, result);
    }
}

void journey_pre_save_hacks(void) {
    // Try to make the save file Frotz compatible.
    // Still haven't worked out how to do this.
    set_global(jg.CHRH, 0x08);
    set_global(jg.CHRV, 0x10);
    if (jg.NAME_WIDTH_PIX != 0)
        set_global(jg.NAME_WIDTH_PIX, 0x70);
    if (jg.COMMAND_WIDTH_PIX != 0)
        set_global(jg.COMMAND_WIDTH_PIX, 0x70);
    set_global(jg.SCREEN_HEIGHT, 0x19);
    set_global(jg.SCREEN_WIDTH, 0x50);
}

void journey_post_save_hacks(void) {
    if (jg.INTERPRETER != 0)
        set_global(jg.INTERPRETER, options.int_number);
    set_global(jg.CHRH, 1);
    set_global(jg.CHRV, 1);
    update_internal_globals();
}
