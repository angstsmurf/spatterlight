//
//  z6_specific.h
//  Spatterlight
//
//  Created by Administrator on 2024-08-15.
//

#ifndef z6_specific_h
#define z6_specific_h

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

#define GLOBAL(X)   (header.globals + X * 2)
#define get_global(X)    word(GLOBAL(X))
#define set_global(X, Y)    store_word(GLOBAL(X), Y)

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

extern bool is_spatterlight_arthur;
extern bool is_spatterlight_journey;
extern bool is_spatterlight_shogun;
extern bool is_spatterlight_zork0;
extern bool is_spatterlight_v6;

#endif /* z6_specific_h */
