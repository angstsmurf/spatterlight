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
#include "v6_shared.hpp"

#include "journey.hpp"

extern Window *curwin;

enum StampBits {
  kStampRightBit = 1,
  kStampBottomBit = 2
};

#define JOURNEY_BG_GRID windows[1]
#define JOURNEY_GRAPHICS_WIN windows[3]

#define THICK_V_LINE 57
#define THIN_V_LINE 41
#define H_LINE 39

#define MAX_SUBMENU_ITEMS 6


JourneyGlobals jg;
JourneyRoutines jr;
JourneyObjects jo;
JourneyAttributes ja;

static Window *journey_text_buffer;

static JourneyWords printed_journey_words[4];
static int number_of_printed_journey_words = 0;

static inputMode journey_current_input = INPUT_PARTY;
static uint16_t journey_input_length = 0;

static int16_t selected_journey_line = -1;
static int16_t selected_journey_column = -1;
static int journey_image_x, journey_image_y, journey_image_width, journey_image_height;
static float journey_image_scale = 1.0;
static glui32 screen_width_in_chars, screen_height_in_chars;
static bool global_border_flag, global_font_3_flag;
static uint16_t max_length = 0;
static uint16_t from_command_start_line = 0;
static uint16_t input_column = 0;
static uint16_t input_line = 0;
static uint16_t input_table = 0;
static int serial_as_int = 0;

struct JourneyMenu {
    char name[STRING_BUFFER_SIZE];
    struct JourneyMenu *submenu;
    int submenu_entries;
    int length;
    int line;
    int column;
};


void journey_adjust_image(int picnum, uint16_t *x, uint16_t *y, int width, int height, int winwidth, int winheight, float *scale, float pixwidth) {

    journey_image_width = width;
    journey_image_height = height;
    journey_image_scale = (float)winwidth / ((float)width * pixwidth) ;

    if (height * journey_image_scale > winheight) {
        journey_image_scale = (float)winheight / height;
        journey_image_x = (winwidth - width * journey_image_scale * pixwidth) / 2;
        journey_image_y = 0;
    } else {
        journey_image_x = 0;
        journey_image_y = (winheight - height * journey_image_scale) / 2;
    }
    
    *scale = journey_image_scale;

    *x = journey_image_x;
    *y = journey_image_y;
}

static void journey_draw_stamp_image(winid_t win, int16_t picnum, int16_t where, float pixwidth) {
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

static void journey_draw_title_image(void) {
    float scale;
    uint16_t x, y;
    int width, height;
    winid_t win = JOURNEY_GRAPHICS_WIN.id;
    if (win == nullptr || win->type != wintype_Graphics) {
        fprintf(stderr, "draw_journey_title_image: glk window error!\n");
        return;
    }

    // Resize and recolour all three windows, as they might be
    // visible behind each other when resizing.
    // Window size also determines which window background color
    // is copied to the border.
    win_setbgnd(JOURNEY_BG_GRID.id->peer, monochrome_black);
    JOURNEY_BG_GRID.x_size = gscreenw;
    JOURNEY_BG_GRID.y_size = gscreenh;
    v6_sizewin(&JOURNEY_BG_GRID);

    win_setbgnd(journey_text_buffer->id->peer, monochrome_black);
    journey_text_buffer->x_size = gscreenw;
    journey_text_buffer->y_size = gscreenh;
    v6_sizewin(journey_text_buffer);

    glk_window_set_background_color(win, monochrome_black);
    JOURNEY_GRAPHICS_WIN.x_size = gscreenw;
    JOURNEY_GRAPHICS_WIN.y_size = gscreenh;
    v6_sizewin(&JOURNEY_GRAPHICS_WIN);
    glk_window_clear(win);

    win_setbgnd(-1, monochrome_black);

    get_image_size(160, &width, &height);
    journey_adjust_image(160, &x, &y, width, height, gscreenw, gscreenh, &scale, pixelwidth);
    draw_inline_image(win, 160, x, y, scale, false);
}

int journey_draw_picture(int pic, winid_t journey_window) {
    if (pic == 44) {
        pic = 116;
    }

    int width, height;
    get_image_size(pic, &width, &height);

    Window *win = curwin;
    if (win->id && win->id->type != wintype_Graphics) {
        fprintf(stderr, "Trying to draw image to non-graphics win (%d)", curwin->index);

        if (journey_window == nullptr) {
            journey_window = glk_window_open(JOURNEY_BG_GRID.id, winmethod_Left | winmethod_Fixed, gscreenw / 3, wintype_Graphics, 0);
        }
        if (JOURNEY_GRAPHICS_WIN.id != journey_window) {
            fprintf(stderr, "windows[3].id != journey_window!\n");
            if (JOURNEY_GRAPHICS_WIN.id != nullptr) {
                gli_delete_window(windows[3].id);
            }
            JOURNEY_GRAPHICS_WIN.id = journey_window;
        }
        win = &JOURNEY_GRAPHICS_WIN;
    }
    if (win->id != nullptr) {
        glk_window_clear(win->id);

        float scale;
        uint16_t x, y;
        journey_adjust_image(pic, &x, &y, width, height, win->x_size, win->y_size, &scale, pixelwidth);
        draw_inline_image(win->id, pic, x, y, scale, false);
    }
    return pic;
}

static void journey_font3_line(int line, int character, int left, int right) {
    glk_set_style(style_BlockQuote);
    glk_window_move_cursor(curwin->id, 0, line - 1);
    glk_put_char(left);
    for (int i = 1; i < screen_width_in_chars - 1; i++)
        glk_put_char(character);
    glk_put_char(right);
    glk_set_style(style_Normal);
}

static int journey_refresh_character_command_area(int16_t line);

static void journey_setup_windows(void) {
    int offset = 6;
    int text_window_left_edge;

    int width;
    bool result = get_image_size(2, &width, nullptr);
    if (!result) {
        fprintf(stderr, "journey_setup_windows: Could not get size of image 2!\n");
        text_window_left_edge = 32;
    } else {
        int picture_width = round((float)width * imagescalex);

        if (options.int_number == INTERP_APPLE_IIC ||
            options.int_number == INTERP_APPLE_IIE ||
            options.int_number == INTERP_APPLE_IIGS ||
            options.int_number == INTERP_MSDOS) {
            offset = 3;
        }
        if (options.int_number != INTERP_AMIGA) {
            offset = 5;
        }
        text_window_left_edge = offset + (picture_width + gcellw) / gcellw;
    }

    set_global(jg.TEXT_WINDOW_LEFT, text_window_left_edge);
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
    glk_window_get_size(JOURNEY_BG_GRID.id, &screen_width_in_chars, &screen_height_in_chars);
    if (screen_width_in_chars == 0 || screen_height_in_chars == 0) {
        fprintf(stderr, "Journey update_screen_size: screen size 0!\n");
        screen_width_in_chars = (gscreenw - ggridmarginx * 2) / gcellw;
        screen_height_in_chars = (gscreenh - ggridmarginy * 2) / gcellh;
    }

    set_global(jg.SCREEN_WIDTH, screen_width_in_chars);
    set_global(jg.SCREEN_HEIGHT, screen_height_in_chars);

    v6_sync_upperwin_size(screen_width_in_chars, screen_height_in_chars);
}


static void update_internal_globals(void) {

    int black_picture_border;

    switch (options.int_number) {
        case INTERP_MSDOS:
            global_border_flag = false;
            global_font_3_flag = false;
            black_picture_border = 0;
            break;
        case INTERP_MACINTOSH:
            global_border_flag = false;
            global_font_3_flag = true;
            black_picture_border = 1;
            break;
        case INTERP_AMIGA:
            global_border_flag = true;
            global_font_3_flag = true;
            black_picture_border = 1;
            break;
        case INTERP_APPLE_IIC:
        case INTERP_APPLE_IIE:
        case INTERP_APPLE_IIGS:
            global_border_flag = false;
            global_font_3_flag = false;
            black_picture_border = 0;
            break;
        default: // Should never happen
            fprintf(stderr, "Unsupported interpreter option (%lu)!\n", options.int_number);
            exit(1);
    }

    if (jg.BORDER_FLAG != 0) {
        // Whether there is a border around the screen, true on Amiga only
        set_global(jg.BORDER_FLAG, global_border_flag ? 1 : 0);

        // Whether to use Font 3. True for Amiga and Mac, false for MS DOS and Apple II
        set_global(jg.FONT3_FLAG, global_font_3_flag ? 1 : 0);

        // FWC-FLAG (fixed-width commands) tells whether to switch font when printing in command window,
        // i.e. whether there is a separate proportional font used elsewhere.
        // Always the same as FONT3-FLAG, and used interchangably in the original ZIL code.
        set_global(jg.FWC_FLAG, global_font_3_flag ? 1 : 0);

        // Whether to have a border around the image. BLACK_PICTURE_BORDER is only false on IBM PC
        set_global(jg.BLACK_PICTURE_BORDER, black_picture_border);
    }

    update_screen_size();

    //  COMMAND_START_LINE is first line below divider: SCREEN_HEIGHT_in_chars - 4 on non-Amiga, SCREEN_HEIGHT_in_chars - 5 on Amiga

    int top_screen_line, command_start_line;

    if (!global_border_flag) {
        top_screen_line = 1;
        command_start_line = screen_height_in_chars - 4;
    } else {
        top_screen_line = 2;
        command_start_line = (screen_height_in_chars - 5);
    }

    set_global(jg.TOP_SCREEN_LINE, top_screen_line);
    set_global(jg.COMMAND_START_LINE, command_start_line);

    // Width of a column (except Name column) in characters
    int command_width = screen_width_in_chars / 5;
    set_global(jg.COMMAND_WIDTH, command_width);

    // Width of column in pixels. Used in ERASE-COMMAND
    int command_width_pix = command_width - 1;


    int name_width = command_width + screen_width_in_chars % 5;
    set_global(jg.NAME_WIDTH, name_width);

    int name_width_pix = name_width - 1;

    if (jg.COMMAND_WIDTH_PIX != 0 && jg.COMMAND_WIDTH != jg.COMMAND_WIDTH_PIX) {
        set_global(jg.COMMAND_WIDTH_PIX, command_width_pix);
        set_global(jg.NAME_WIDTH_PIX, name_width_pix);
    }

    bool interpreter_is_apple_2 = (options.int_number == INTERP_APPLE_IIC || options.int_number == INTERP_APPLE_IIE || options.int_number == INTERP_APPLE_IIGS);

    int party_command_column = interpreter_is_apple_2 ? 1: 2;

    set_global(jg.PARTY_COMMAND_COLUMN, party_command_column);


    // NAME_COLUMN: Position of second column in characters (1-based)
    int name_column = party_command_column + command_width;
    set_global(jg.NAME_COLUMN, name_column);

    int chr_command_column = name_column + name_width;
    set_global(jg.CHR_COMMAND_COLUMN, chr_command_column);

    int command_object_column = chr_command_column + command_width;
    set_global(jg.COMMAND_OBJECT_COLUMN, command_object_column);
}

static int party_pcm(int chr) {
    uint16_t party_table = get_global(jg.PARTY);
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
    if (global_font_3_flag) {
        glk_put_char('_');
    } else {
        glk_put_char(UNICODE_SPACE);
    }
}

static void journey_move_cursor(int column, int line) {
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

    if (column > screen_width_in_chars || line > screen_height_in_chars) {
        fprintf(stderr, "Error! move_v6_cursor() moving cursor out of bounds\n");
        if (column > screen_width_in_chars)
            column = screen_width_in_chars - 1;
        if (line > screen_height_in_chars)
            line = screen_height_in_chars - 1;
    }

    glk_window_move_cursor(JOURNEY_BG_GRID.id, column, line);
}

static int print_tag_route_to_str(char *str) {
    int name_length = get_global(jg.TAG_NAME_LENGTH);
    int name_table = get_global(jg.NAME_TBL) + 4;
    for (int i = 0; i < name_length; i++) {
        str[i] = zscii_to_unicode[user_byte(name_table++)];
    }
    const char *routestring = " route";
    for (int i = 0; i < 6; i++) {
        if (name_length + i >= STRING_BUFFER_SIZE) {
            fprintf(stderr, "Error: Tag name too long!\n");
            return 0;
        }
        str[name_length + i] = routestring[i];
    }
    str[name_length + 6] = 0;
    return name_length + 6;
}

static void create_journey_party_menu(void) {
    int object;

    char str[STRING_BUFFER_SIZE];
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
        }
    }
}

static void journey_create_menu(JourneyMenuType type, bool is_second_noun) {

    struct JourneyMenu menu[10];

    int table, table_count;
    if (type == kJMenuTypeObjects) {
        table = get_global(jg.O_TABLE) + (is_second_noun ? 10 : 0);
        table_count = user_word(table);
    } else {
        table = get_global(jg.PARTY);
        table_count = 5;
    }

    struct JourneyMenu *m;

    bool subgroup = (get_global(jg.SUBGROUP_MODE) == 1 && type != kJMenuTypeObjects);
    int menu_counter = 0;

    for (int i = 1; i <= table_count; i++) {
        uint16_t object = word(table + 2 * i);
        if (object != 0 && (!(subgroup && !internal_test_attr(object, ja.SUBGROUP)) || internal_test_attr(object, ja.SHADOW))) {

            m = &menu[menu_counter];

            int TAG_NAME_LENGTH = get_global(jg.TAG_NAME_LENGTH);
            int NAME_TBL = get_global(jg.NAME_TBL) + 2;

            if ((object == jo.TAG_OBJECT || object == jo.TAG) && TAG_NAME_LENGTH != 0) {
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
                m->line = (i - 1) % 5;
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
                    m->column = 3 + ((menu_counter > 4 || is_second_noun) ? 1 : 0);
                }
                menu_counter++;
                if (menu_counter > 10)
                    return;
            }
        }
    }

    if (type == kJMenuTypeObjects) {
        // Add "glue words" such as "cast" "elevation" "at"
        for (int i = 0; i < number_of_printed_journey_words; i++) {
            JourneyWords *jword = &(printed_journey_words[i]);
            char string[STRING_BUFFER_SIZE];
            int len = print_zstr_to_cstr(jword->str, string);
            if (len > 1) {
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
                win_menuitem(kJMenuTypeVerbs, submenu->column, submenu->line, (j == m->submenu_entries - 1), submenu->name, submenu->length);
            }
            free(m->submenu);
            m->submenu = nullptr;
        }
    }
}

#pragma mark Input

static void journey_draw_cursor(void) {
    if (input_column > screen_width_in_chars - 1)
        return;
    journey_move_cursor(input_column, input_line);

    if (global_font_3_flag)
        garglk_set_reversevideo(1);
    else
        garglk_set_reversevideo(0);

    underscore_or_square();

    if (global_font_3_flag)
        garglk_set_reversevideo(0);
    else
        garglk_set_reversevideo(1);


    journey_move_cursor(input_column, input_line);
}

static uint16_t journey_read_keyboard_line(int x, int y, uint16_t table, int max, bool elvish, uint8_t *kbd) {

    input_column = x + journey_input_length;
    input_line = y;
    max_length = max;
    input_table = table;
    from_command_start_line = y - get_global(jg.COMMAND_START_LINE);

    set_current_window(&JOURNEY_BG_GRID);
    journey_move_cursor(x, input_line);

    if (!global_font_3_flag) {
        garglk_set_reversevideo(1);
    }

    for (int i = 0; i <= max_length; i++) {
        underscore_or_square();
    }

    journey_move_cursor(input_column, input_line);

    int start = input_column;

    win_menuitem(kJMenuTypeTextEntry, x, from_command_start_line, elvish, nullptr, STRING_BUFFER_SIZE);

    uint8_t character;

    journey_draw_cursor();

    while ((character = internal_read_char()) != ZSCII_NEWLINE) {
        if (global_font_3_flag) {
            garglk_set_reversevideo(0);
        }
        if (character == 0x7f || character == ZSCII_BACKSPACE || character == ZSCII_LEFT) { // DELETE-KEY ,BACK-SPACE ,LEFT-ARROW
            if (journey_input_length == 0) {
                win_beep(1);
                continue;
            } else {
                if (input_column < screen_width_in_chars - 1) {
                    journey_move_cursor(input_column, input_line);
                    underscore_or_square();
                } else {
                    journey_move_cursor(screen_width_in_chars - 1, input_line);
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

            if (input_column < screen_width_in_chars) {
                journey_move_cursor(input_column, input_line);
                put_char(character);
            }
            user_store_byte(table + journey_input_length, character);
            input_column++;
            journey_input_length++;
            journey_draw_cursor();
        }
    }

    if (elvish) {
        journey_move_cursor(start, input_line);
        for (int i = 0; i < max_length; i++) {
            put_char(UNICODE_SPACE);
        }
    }

    set_current_window(journey_text_buffer);
    journey_current_input = INPUT_PARTY;
    return journey_input_length;
}


static int GET_COMMAND(int cmd) {

    // Later releases have a separate table with abbreviated commands.
    // This seems to have been introduced in release 51, serial 890522.
    // (It is easier to do it this way than checking the release number
    // directly because of release 142, serial 890205.)

    if (serial_as_int >= 890522) {
        if (get_global(jg.COMMAND_WIDTH) < 13) {
            int STR = user_word(cmd + 10);
            if (STR)
                return STR;
        }
    }
    return user_word(cmd);
}

static int PRINT_COMMAND(int cmd) {
    return print_handler(unpack_string(GET_COMMAND(cmd)), nullptr);
}

static int GET_DESC(int obj) {
    int string;
    if (screen_width_in_chars < 0x32) {
        string = internal_get_prop(obj, ja.DESC8);
        if (string)
            return string;
    }
    if (screen_width_in_chars < 0x47) {
        string = internal_get_prop(obj, ja.DESC12);
        if (string)
            return string;
    }
    return (internal_get_prop(obj, ja.SDESC));
}


static int PRINT_DESC(int obj, bool cmd) {
    int name_table = get_global(jg.NAME_TBL) + 2;
    int tag_name_length = get_global(jg.TAG_NAME_LENGTH);

    if ((obj == jo.TAG_OBJECT || obj == jo.TAG) && tag_name_length != 0) {
        for (uint16_t j = 0; j < tag_name_length; j++) {
            put_char(user_byte(name_table++));
        }
        return tag_name_length;
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

static void journey_erase_command_chars(int line, int column, int num_spaces) {
    if (options.int_number == INTERP_MSDOS) {
        journey_move_cursor(column - 1, line);
    } else {
        journey_move_cursor(column, line);
    }

    bool in_rightmost_column = (column > screen_width_in_chars * 0.7);

    if (in_rightmost_column) {
        if (!global_font_3_flag) {
            num_spaces++;
        } else if (options.int_number == INTERP_AMIGA) {
            num_spaces--;
        }
    }

    for (int i = 0; i < num_spaces; i++)
        glk_put_char_stream(JOURNEY_BG_GRID.id->str, UNICODE_SPACE);

    if (!in_rightmost_column) {
        if (!global_font_3_flag) {
            garglk_set_reversevideo(1);
            glk_put_char_stream(JOURNEY_BG_GRID.id->str, UNICODE_SPACE);
            garglk_set_reversevideo(0);
        } else if (column > screen_width_in_chars * 0.5) {
            glk_set_style(style_BlockQuote);
            glk_put_char_stream(JOURNEY_BG_GRID.id->str, THIN_V_LINE);
            glk_set_style(style_Normal);
        }
    }


    journey_move_cursor(column, line);
}


#define LONG_ARROW_WIDTH 3
#define SHORT_ARROW_WIDTH 2
#define NO_ARROW_WIDTH 1

static void journey_print_character_commands(bool clear) {
    // Prints the character names and arrows, and their commands in the three rightmost columns.
    // If clear is true, the three columns to the right of the character names will be cleared.

    if (clear)
        number_of_printed_journey_words = 0;

    int line = get_global(jg.COMMAND_START_LINE);
    int partytable, character, position;
    if (get_global(jg.UPDATE_FLAG) == 1 && !clear) {
        internal_call(pack_routine(jr.FILL_CHARACTER_TBL));
    }

    if (!clear)
        journey_create_menu(kJMenuTypeMembers, false);

    partytable = get_global(jg.PARTY);

    set_current_window(&JOURNEY_BG_GRID);

    int commandwidth = get_global(jg.COMMAND_WIDTH);
    int namewidth = get_global(jg.NAME_WIDTH);
    int name_right_edge = get_global(jg.CHR_COMMAND_COLUMN) - 2;

    int number_of_printed_party_members = 0;
    int number_of_printed_verbs_and_objects = 0;

    // Print up to 5 character names and arrows
    for (int i = 1; i <= 5; i++) {
        character = word(partytable + 2 * i); // <GET .PTBL 1>
        position = get_global(jg.NAME_COLUMN); // NAME-COLUMN

        journey_erase_command_chars(line, position, namewidth - 1);

        // global 0x80 is SUBGROUP-MODE
        // attribute 0x2a is SUBGROUP flag
        if (character != 0 && !(get_global(jg.SUBGROUP_MODE) && !internal_test_attr(character, ja.SUBGROUP))) {
            int namelength = PRINT_DESC(character, false);

            if (screen_width_in_chars < 55) { // (<L? ,SCREEN-WIDTH ,8-WIDTH

                if (namewidth - namelength - 2 < SHORT_ARROW_WIDTH) {
                    journey_move_cursor(name_right_edge - NO_ARROW_WIDTH, line);
                    glk_put_string(const_cast<char*>(">"));
                } else {
                    journey_move_cursor(name_right_edge - SHORT_ARROW_WIDTH, line);
                    glk_put_string(const_cast<char*>("->"));
                }

            } else {
                journey_move_cursor(name_right_edge - LONG_ARROW_WIDTH, line);
                glk_put_string(const_cast<char*>("-->"));
            }
            number_of_printed_party_members++;
        }

        if (journey_current_input == INPUT_PARTY) {
            position = get_global(jg.CHR_COMMAND_COLUMN);

            // CHARACTER-INPUT-TBL contains pointers to tables of up to three verbs
            uint16_t verbs_table = word(get_global(jg.CHARACTER_INPUT_TBL) + i * 2);

            bool subgroup_mode = (get_global(jg.SUBGROUP_MODE) == 1);
            bool subgroup_attribute = internal_test_attr(character, ja.SUBGROUP); // attribute 0x2a is SUBGROUP flag
            // if the SHADOW_BIT attribute of a character is set, the character name is hidden
            // and its commands "belong" to the visible character above.
            // This is used to give Praxix more than three commands in the mines.
            bool shadow_attribute = internal_test_attr(character, ja.SHADOW); // attribute 0x17 is SHADOW flag

            bool should_print_command = true;

            if (clear || character == 0) {
                should_print_command = false;
            // The SHADOW bit attribute trumps subgroup mode: if it is set,
            // commands will be printed even though the character
            // does not belong to the subgroup.
            } else if ((subgroup_mode && subgroup_attribute) || shadow_attribute) {
                should_print_command = true;
            }

            // Print up to three verbs for each character to the right, or just erase the fields
            for (int j = 0; j <= 2; j++) {
                journey_erase_command_chars(line, position, commandwidth - 1);

                if (should_print_command && PRINT_COMMAND(word(verbs_table + j * 2)) > 2) {
                    number_of_printed_verbs_and_objects++;
                }

                position += commandwidth;
            }
        }

        line++;
    }


    if (get_global(jg.SMART_DEFAULT_FLAG) == 1) { // SMART-DEFAULT-FLAG
        set_global(jg.SMART_DEFAULT_FLAG, 0);
        internal_call(pack_routine(jr.SMART_DEFAULT)); //    SMART_DEFAULT();
    }

    // Delete the Individual Commands menu when the columns to the right are empty
    // (and also update party menu)
    if (number_of_printed_party_members == 0 && number_of_printed_verbs_and_objects == 0) {
        win_menuitem(kJMenuTypeDeleteMembers, 0, 0, false, nullptr, STRING_BUFFER_SIZE);
        create_journey_party_menu();
    }

    set_current_window(&windows[ja.buffer_window_index]);
}

bool journey_read_elvish(int actor) {
    journey_print_character_commands(true); // <CLEAR-FIELDS>

    user_store_byte(get_global(jg.E_LEXV), 0x14); // <PUTB ,E-LEXV 0 20>
    user_store_byte(get_global(jg.E_INBUF), 0x32); // <PUTB ,E-INBUF 0 50>

    int line = get_global(jg.COMMAND_START_LINE) + party_pcm(actor) - 1;

    journey_move_cursor(get_global(jg.CHR_COMMAND_COLUMN), line);
    glk_put_string_stream(JOURNEY_BG_GRID.id->str, const_cast<char*>("says..."));

    int MAX = screen_width_in_chars - get_global(jg.COMMAND_OBJECT_COLUMN) - 2;

    uint16_t table = get_global(jg.E_TEMP);

    int column = get_global(jg.COMMAND_OBJECT_COLUMN);
    uint16_t offset = journey_read_keyboard_line(column, line, table, MAX, true, nullptr);

    journey_input_length = 0;

    journey_refresh_character_command_area(get_global(jg.COMMAND_START_LINE) - 1);
    set_global(jg.UPDATE_FLAG, 1);

    if (offset == 0)
        return false;

    internal_call(pack_routine(jr.MASSAGE_ELVISH), {offset});
    set_global(jg.E_TEMP_LEN, offset);

    tokenize(get_global(jg.E_INBUF),get_global(jg.E_LEXV), 0, false);
    if (user_byte(get_global(jg.E_LEXV) + 1) == 0)
        return false;
    if (actor == jo.PRAXIX || actor == jo.BERGON)
        return true;
    internal_call(pack_routine(jr.PARSE_ELVISH));
    return true;
}

void journey_change_name() {
    int MAX;
    if (screen_width_in_chars < 50) // 8-WIDTH
        MAX = 5;
    else
        MAX = 8;
    
    int COL = get_global(jg.NAME_COLUMN); // NAME-COLUMN
    int LN = get_global(jg.COMMAND_START_LINE) + party_pcm(jo.TAG) - 1; // <SET LN <- <+ ,COMMAND-START-LINE <PARTY-PCM ,TAG>> 1>
    uint16_t table = get_global(jg.NAME_TBL) + 2;

    uint8_t key = 0;
    uint16_t offset = journey_read_keyboard_line(COL, LN, table, MAX, false, &key);

    journey_input_length = 0;

    if (offset == 0) {
        set_global(jg.UPDATE_FLAG, 1); // <SETG UPDATE-FLAG T>
    } else if (internal_call(pack_routine(jr.ILLEGAL_NAME), {offset}) == 1) { // ILLEGAL-NAME
        glk_put_string(const_cast<char*>("[The name you have chosen is reserved. Please try again.]"));
    } else {
        // Do the change
        internal_call(pack_routine(jr.REMOVE_TRAVEL_COMMAND));

        public_encode_text(table, offset, get_global(jg.TAG_NAME)); //  <ZWSTR ,NAME-TBL .OFF 2 ,TAG-NAME>

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

    serial_as_int = atoi((const char *)header.serial);

    journey_setup_windows();
    int text_window_left_edge = get_global(jg.TEXT_WINDOW_LEFT);


    // Draw a line at the top with a centered "JOURNEY"
    if (global_border_flag) {
        journey_font3_line(1, H_LINE, 47, 48);
        int x = screen_width_in_chars / 2 - 2;
        journey_move_cursor(x, 1);
        glk_put_string(const_cast<char*>("JOURNEY"));
    }

    // Draw vertical divider between graphics window
    // and buffer text output window

    int command_start_line = get_global(jg.COMMAND_START_LINE);

    // Starts at TOP-SCREEN-LINE which is 2 on Amiga (because of border)
    // otherwise 1
    int line;
    for (line = get_global(jg.TOP_SCREEN_LINE); line != command_start_line - 1; line++) {
        if (!global_border_flag) {
            journey_move_cursor(text_window_left_edge - 1, line);
            if (global_font_3_flag) {
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
            journey_move_cursor(0, line);
            glk_put_char(THIN_V_LINE);
            journey_move_cursor(text_window_left_edge - 1, line);
            glk_put_char(THIN_V_LINE);
            journey_move_cursor(screen_width_in_chars, line);
            glk_put_char(40);
            glk_set_style(style_Normal);
        }
    };

    // Draw horizontal line above "command area"
    if (global_font_3_flag) {
        if (global_border_flag) {
            journey_font3_line(line, H_LINE, THIN_V_LINE, 40);
        } else {
            journey_font3_line(line, H_LINE, H_LINE, H_LINE);
        }
    } else {
        journey_move_cursor(0, line);
        garglk_set_reversevideo(1);
        for (int i = 0; i < screen_width_in_chars; i++)
            glk_put_char(UNICODE_SPACE);
        garglk_set_reversevideo(0);
    }

    // Draw bottom border line
    if (global_border_flag) {
        journey_font3_line(screen_height_in_chars, 38, 46, 49);
    }

    if (!global_font_3_flag) {
        garglk_set_reversevideo(1);
    }

    int name_width = get_global(jg.NAME_WIDTH);

    int width = 9; //  WIDTH = TEXT_WIDTH("The Party");

    // Print "The Party" centered over the name column
    int x = get_global(jg.NAME_COLUMN) + (name_width - width) / 2 - 1;
    journey_move_cursor(x, line);
    glk_put_string(const_cast<char*>("The Party"));

    // Print "Individual Commands" centered in the empty space to the right of "The Party" text
    width = 19; // WIDTH = TEXT_WIDTH("Individual Commands");

    int character_command_column = get_global(jg.CHR_COMMAND_COLUMN);
    journey_move_cursor(character_command_column + (screen_width_in_chars - character_command_column - width) / 2 + (global_font_3_flag ? 1 : 0),  line);

    glk_put_string(const_cast<char*>("Individual Commands"));

    if (!global_font_3_flag) {
        garglk_set_reversevideo(0);
    }
    set_current_window(lastwin);
}

void TAG_ROUTE_PRINT(void) {
    int tag_name_length = get_global(jg.TAG_NAME_LENGTH);
    int name_table = get_global(jg.NAME_TBL) + 4;

    for (uint16_t j = 0; j < tag_name_length; j++) {
        put_char(user_byte(name_table++));
    }
    if (screen_width_in_chars < 71 || tag_name_length > 6) {
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
    // actor (journey_read_elvish argument) is set to Tag by default
    store_variable(1, (journey_read_elvish(variable(1)) ? 1 : 0));
}

void CHANGE_NAME(void) {
    journey_current_input = INPUT_NAME;
    journey_change_name();
}

// It would be nice if this could be merged with journey_erase_command_chars() somehow

void ERASE_COMMAND(void) {
    int pix = variable(1);
    glk_window_move_cursor(curwin->id, curwin->x, curwin->y);

    int num_spaces = get_global(jg.COMMAND_WIDTH) - 1;
    if (pix == get_global(jg.NAME_WIDTH_PIX)) {
        num_spaces = get_global(jg.NAME_WIDTH) - 1;
    }

    if (options.int_number == INTERP_MSDOS) {
        num_spaces--;
    }

    if (curwin->x > screen_width_in_chars * 0.7) {
        if (!global_font_3_flag) {
            num_spaces++;
        } else if (options.int_number == INTERP_AMIGA) {
            num_spaces--;
        }
    }

    for (int i = 0; i < num_spaces; i++)
        glk_put_char(UNICODE_SPACE);

    glk_window_move_cursor(curwin->id, curwin->x, curwin->y);
}

static void journey_print_columns(bool party, bool is_second_noun) {

//  is_second_noun is true for things like requesting the target of a spell,
//  It means we should print a selectable list of objects in the rightmost column.

    int column, table, object;
    int line = get_global(jg.COMMAND_START_LINE);

    int command_width = get_global(jg.COMMAND_WIDTH);
    set_current_window(&JOURNEY_BG_GRID);

    if (party) {
        column = get_global(jg.PARTY_COMMAND_COLUMN);
        table = get_global(jg.PARTY_COMMANDS);
    } else  {
        column = get_global(jg.COMMAND_OBJECT_COLUMN) + (is_second_noun ? command_width : 0);
        table = get_global(jg.O_TABLE) + (is_second_noun ? 10 : 0);
        journey_create_menu(kJMenuTypeObjects, is_second_noun);
    }

    int table_count = user_word(table);

    for (int i = 1; i <= table_count; i++) {
        object = user_word(table + 2 * i);
        journey_erase_command_chars(line, column, command_width - 1);
        if (party) {
            if (object == jo.TAG_ROUTE_COMMAND
                && get_global(jg.TAG_NAME_LENGTH)
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
            column += command_width;
            line = get_global(jg.COMMAND_START_LINE);
        }
    }
    journey_refresh_character_command_area(get_global(jg.COMMAND_START_LINE) - 1);
}

void PRINT_COLUMNS(void) {
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

static int journey_refresh_character_command_area(int16_t line) {

    update_internal_globals();

    // Width of a column (except Name column) in characters
    int command_width = get_global(jg.COMMAND_WIDTH);
    // Width of Name column in characters
    int name_width = get_global(jg.NAME_WIDTH);

    Window *lastwin = curwin;
    set_current_window(&JOURNEY_BG_GRID);

    int commands_bottom_line = get_global(jg.COMMAND_START_LINE) + 5;

    while (++line < commands_bottom_line) {
        int16_t position = 1;
        journey_move_cursor(position, line);
        while (position <= screen_width_in_chars) {
            if (global_font_3_flag) {
                if (position != 1 && position < screen_width_in_chars - 5) {
                    glk_set_style(style_BlockQuote);
                    journey_move_cursor(position, line);
                    if (position == command_width || position == command_width + 1 || position == command_width + name_width + 1 || position == command_width + name_width) {
                        glk_put_char(THICK_V_LINE);
                    } else {
                        glk_put_char(THIN_V_LINE);
                    }
                    glk_set_style(style_Normal);
                } else if (position == 1 && global_border_flag) {
                    glk_set_style(style_BlockQuote);
                    journey_move_cursor(position, line);
                    glk_put_char(THIN_V_LINE);
                    glk_set_style(style_Normal);
                }
            } else if (position != 1 && position < screen_width_in_chars - 5) {
                journey_move_cursor(position - 1, line);
                garglk_set_reversevideo(1);
                glk_put_char(UNICODE_SPACE);
                garglk_set_reversevideo(0);
            }

            if (position == command_width || position == command_width + 1) {
                position += name_width;
            } else {
                if (get_global(jg.COMMAND_WIDTH_PIX) == 0) {
                    journey_move_cursor(get_global(jg.PARTY_COMMAND_COLUMN), line);
                }
                position += command_width;
            }
        }

        if (global_border_flag) {
            glk_set_style(style_BlockQuote);
            journey_move_cursor(screen_width_in_chars, line);
            glk_put_char(40); // Draw right border char (40)
            glk_set_style(style_Normal);
        }
    }

    set_current_window(lastwin);
    glk_set_style(style_Normal);

    return line;
}

void REFRESH_CHARACTER_COMMAND_AREA(void) {
    int line = variable(1);
    journey_refresh_character_command_area(line);
}

static void journey_reprint_partial_input(int x, int y, int length_so_far, int max_length, int16_t table_address) {
    journey_move_cursor(x, y);
    glk_set_style(style_Normal);
    if (!global_font_3_flag) {
        garglk_set_reversevideo(1);
    }

    int max_screen = screen_width_in_chars - 1 - (options.int_number == INTERP_AMIGA ? 1 : 0);
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


static void journey_resize_graphics_and_buffer_windows(void) {

    int text_window_left = get_global(jg.TEXT_WINDOW_LEFT);

    if (global_border_flag) {
        JOURNEY_GRAPHICS_WIN.x_origin = ggridmarginx + gcellw + 5;
        JOURNEY_GRAPHICS_WIN.y_origin = ggridmarginy + gcellh + 1;
    } else {
        JOURNEY_GRAPHICS_WIN.x_origin = ggridmarginx + 1;
        JOURNEY_GRAPHICS_WIN.y_origin = ggridmarginy + 1;
    }

    if (screen_height_in_chars < 5)
        update_screen_size();

    JOURNEY_GRAPHICS_WIN.x_size = (float)(text_window_left - 2) * gcellw - JOURNEY_GRAPHICS_WIN.x_origin + ggridmarginx + (global_font_3_flag ? 0 : 2);

    if (graphics_type == kGraphicsTypeNoGraphics) {
        JOURNEY_GRAPHICS_WIN.x_size = 0;
        JOURNEY_GRAPHICS_WIN.x_origin -= gcellw;
        text_window_left = 0;
    }

    if (journey_text_buffer == nullptr) {
        journey_text_buffer = &windows[ja.buffer_window_index];
    }

    int command_start_line = screen_height_in_chars - 4 - (global_border_flag ? 1 : 0);

    uint16_t x_size = (screen_width_in_chars - text_window_left + 1) * gcellw;
    uint16_t y_size = (command_start_line - 2) * gcellh - global_font_3_flag;

    if (global_border_flag) {
        x_size -= gcellw * (graphics_type == kGraphicsTypeNoGraphics ? 4 : 1);
        y_size -= gcellh;
    }

    v6_define_window(journey_text_buffer, JOURNEY_GRAPHICS_WIN.x_origin + JOURNEY_GRAPHICS_WIN.x_size + gcellw + (global_font_3_flag ? 3 : 0), JOURNEY_GRAPHICS_WIN.y_origin, x_size, y_size);

    JOURNEY_GRAPHICS_WIN.y_size = y_size;

    glk_window_set_background_color(JOURNEY_GRAPHICS_WIN.id, monochrome_black);

    v6_sizewin(&JOURNEY_GRAPHICS_WIN);
}

#pragma mark adjust_journey_windows

static void journey_adjust_windows(bool restoring) {
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
    journey_refresh_character_command_area(screen_height_in_chars - 5 - global_border_flag);

    if (screenmode == MODE_NORMAL) {
        set_global(jg.SMART_DEFAULT_FLAG, 0); // SMART-DEFAULT-FLAG

        // Print party (the leftmost) column (call print columns with PARTY flag set
        journey_print_columns(true, false);

        if (journey_current_input != INPUT_PARTY) {
            // If the player is currently entering text, redraw typed text at new position
            if (journey_current_input == INPUT_NAME ) {
                journey_reprint_partial_input(get_global(jg.NAME_COLUMN), get_global(jg.COMMAND_START_LINE) + from_command_start_line, journey_input_length, max_length, input_table);
            } else if (journey_current_input == INPUT_ELVISH ) {
                journey_move_cursor(get_global(jg.CHR_COMMAND_COLUMN), get_global(jg.COMMAND_START_LINE) + from_command_start_line);
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
            if (selected_journey_column <= 0) { // call BOLD-PARTY-CURSOR
                internal_call(pack_routine(jr.BOLD_PARTY_CURSOR), {(uint16_t)selected_journey_line, 0});
            } else if (journey_current_input == INPUT_PARTY) { // call BOLD-CURSOR
                internal_call(pack_routine(jr.BOLD_CURSOR), {(uint16_t)selected_journey_line, (uint16_t)selected_journey_column});
            } else if (journey_current_input != INPUT_ELVISH) { // call BOLD-OBJECT-CURSOR
                int numwords = number_of_printed_journey_words;

                for (int i = 0; i < numwords; i++) {
                    JourneyWords *word = &printed_journey_words[i];
                    std::vector<uint16_t> args = {word->pcm, word->pcf, word->str};
                    internal_call(pack_routine(jr.BOLD_CURSOR), args);
                }
                internal_call(pack_routine(jr.BOLD_OBJECT_CURSOR), {(uint16_t)selected_journey_line, (uint16_t)selected_journey_column}); // BOLD-OBJECT-CURSOR(PCM, PCF)
            }
        }
    }
    journey_resize_graphics_and_buffer_windows();

    // Redraw image(s)
    // (unless we have no graphics)
    if (JOURNEY_GRAPHICS_WIN.id != nullptr && graphics_type != kGraphicsTypeNoGraphics) {
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
    journey_refresh_character_command_area(screen_height_in_chars - 5 - global_border_flag);
}

void BOLD_CURSOR(void) {
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
    selected_journey_line = variable(1);
    selected_journey_column = variable(2);
    redraw_vertical_lines_if_needed();
}

void after_INTRO(void) {
    screenmode = MODE_NORMAL;
}

void GRAPHIC_STAMP(void) {
    int16_t picnum, where = 0;
    if (znargs > 0 && JOURNEY_GRAPHICS_WIN.id != nullptr) {
        picnum = variable(1);
        if (znargs > 1)
            where = variable(2);
        journey_draw_stamp_image(JOURNEY_GRAPHICS_WIN.id, picnum, where, pixelwidth);
    }
}


void REFRESH_SCREEN(void) {
    store_variable(1, 0);
}

void INIT_SCREEN(void) {
    // We check if START-LOC has the SEEN flag set
    // This will be false at the start of the game
    // and on restart.
    if (!qset(jo.START_LOC, ja.SEEN) && screenmode == MODE_INITIAL_QUESTION) {
        // Show title image and wait for key press
        if (journey_text_buffer == NULL)
            journey_text_buffer = &windows[ja.buffer_window_index];
        if (graphics_type != kGraphicsTypeNoGraphics) {
            win_setbgnd(journey_text_buffer->id->peer, monochrome_black);
            glk_window_clear(journey_text_buffer->id);
            screenmode = MODE_SLIDESHOW;
            // We do a fake resize event to draw the title image
            journey_update_on_resize();
            glk_request_mouse_event(JOURNEY_GRAPHICS_WIN.id);
            glk_request_char_event(curwin->id);
            internal_read_char();
        } else {
            journey_update_on_resize();
        }
        screenmode = MODE_CREDITS;
        win_setbgnd(JOURNEY_BG_GRID.id->peer, zcolor_Default);
        win_setbgnd(journey_text_buffer->id->peer, zcolor_Default);
        glk_window_clear(journey_text_buffer->id);

        glk_window_set_background_color(JOURNEY_GRAPHICS_WIN.id, monochrome_black);
        glk_window_clear(JOURNEY_GRAPHICS_WIN.id);
        journey_adjust_windows(false);
    } else if (screenmode != MODE_CREDITS){
        journey_init_screen();
    }
}

void DIVIDER(void) {
    glk_set_style(style_Note);
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
    int16_t stringnum = variable(1);
    char str[1024];
    print_zstr_to_cstr(stringnum, str);
    str[7] = 0;
    if (strcmp(str, "JOURNEY") == 0)
        glk_set_style(style_User1);
    else
        glk_set_style(style_Note);
    print_handler(unpack_string(stringnum), nullptr);
    glk_set_style(style_Normal);
}

void journey_update_on_resize(void) {
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
        journey_draw_title_image();
    } else {
        journey_adjust_windows(false);
        glui32 result;
        glk_style_measure(JOURNEY_BG_GRID.id, style_Normal, stylehint_BackColor, &result);
        win_setbgnd(-1, result);
    }
}

#pragma mark Restoring

void journey_stash_state(library_state_data *dat) {
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

void journey_recover_state(library_state_data *dat) {
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

void journey_update_after_restore() {
    win_menuitem(kJMenuTypeDeleteAll, 0, 0, false, nullptr, STRING_BUFFER_SIZE);
    // Why is this needed? If set, we won't show title image in INIT_SCREEN(),
    // but when does this cause problems?
    if (jo.START_LOC != 0 && ja.SEEN != 0) {
        internal_set_attr(jo.START_LOC, ja.SEEN);
    }
    screenmode = MODE_NORMAL;
    journey_current_input = INPUT_PARTY;
    JOURNEY_BG_GRID.x = 0;
    JOURNEY_BG_GRID.y = 0;
    set_global(jg.CHRH, 1);
    set_global(jg.CHRV, 1);
    journey_setup_windows();
    journey_adjust_windows(true);
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

// Try to make the save file Frotz compatible.
// Still haven't worked out quite how to do this.
void journey_pre_save_hacks(void) {
    set_global(jg.CHRH, 0x08);
    set_global(jg.CHRV, 0x10);
    if (jg.NAME_WIDTH_PIX != 0)
        set_global(jg.NAME_WIDTH_PIX, 0x70);
    if (jg.COMMAND_WIDTH_PIX != 0)
        set_global(jg.COMMAND_WIDTH_PIX, 0x70);
    set_global(jg.SCREEN_HEIGHT, 0x19);
    set_global(jg.SCREEN_WIDTH, 0x50);
}

// Try to make save files from Frotz compatible.
void journey_post_save_hacks(void) {
    if (jg.INTERPRETER != 0)
        set_global(jg.INTERPRETER, options.int_number);
    set_global(jg.CHRH, 1);
    set_global(jg.CHRV, 1);
    update_internal_globals();
}

void COMPLETE_DIAL_GRAPHICS(void) {}
void TELL_AMOUNTS(void) {}
