//
//  zorkzero.cpp
//  bocfel6
//
//  Created by Administrator on 2023-07-30.
//

#include <sstream>

#include "zterp.h"
#include "memory.h"
#include "screen.h"
#include "objects.h"

#include "entrypoints.hpp"
#include "draw_image.hpp"

#include "journey.hpp"


#include "zorkzero.hpp"

#define z0_left_status_window windows[1].id

winid_t z0_right_status_window = nullptr;

bool is_zorkzero_vignette(int pic) {
    return (pic >= 205 && pic <= 329);
}

bool is_zorkzero_border_image(int pic) {
    return (pic >= 2 && pic <= 24 && pic != 8);
}

bool is_zorkzero_game_bg(int pic) {
    return (pic == 41 || pic == 49 || pic == 73 || pic == 99);
}

bool is_zorkzero_rebus_image(int pic) {
    return (pic >= 34 && pic <= 40);
}

void after_SPLIT_BY_PICTURE(void) {
    if (screenmode == MODE_HINTS) {
        windows[1].y_size = 3 * gcellh + 2 * ggridmarginy;
        v6_sizewin(&windows[1]);
        v6_delete_win(&windows[0]);
        v6_remap_win_to_grid(&windows[0]);
        if (graphics_type == kGraphicsTypeApple2) {
            glk_request_char_event(windows[0].id);
            glk_request_mouse_event(windows[0].id);
            windows[7].y_origin = windows[1].y_size + 1;
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
            windows[0].y_origin = windows[7].y_origin + windows[7].y_size;
            windows[0].y_size = gscreenh - windows[0].y_origin;
            v6_sizewin(&windows[0]);
            windows[0].fg_color = Color(Color::Mode::ANSI, get_global(fg_global_idx));
            windows[0].bg_color = Color(Color::Mode::ANSI, get_global(bg_global_idx));
            set_current_style();
            glk_window_clear(windows[0].id);
        }
    } else if (screenmode == MODE_NOGRAPHICS) {
        windows[0].x_size = gscreenw;
        v6_sizewin(&windows[0]);
    }
}

void V_MODE(void) {
    if (screenmode != MODE_NOGRAPHICS) {
        screenmode = MODE_NOGRAPHICS;
        glk_window_clear(current_graphics_buf_win);
    } else {
        screenmode = MODE_NORMAL;
    }
}


// Shared with Shogun
void V_DEFINE(void) {
    update_user_defined_colors();
    glk_window_set_background_color(graphics_win_glk, user_selected_background);
    glk_window_clear(graphics_win_glk);
    screenmode = MODE_DEFINE;
    if (!is_game(Game::ZorkZero)) {
        v6_delete_win(&windows[0]);
        v6_delete_win(&windows[1]);
        if (!windows[2].id)
            windows[2].id = v6_new_glk_window(wintype_TextGrid, 0);
        glk_request_mouse_event(windows[2].id);
        windows[2].style.reset(STYLE_REVERSE);
    }
}

void V_REFRESH(void) {
    fprintf(stderr, "V-REFRESH\n");
    if (screenmode == MODE_DEFINE) {
        screenmode = MODE_NORMAL;
        v6_delete_win(&windows[2]);
        v6_remap_win_to_buffer(&windows[0]);
    }
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

void CENTER(void) {
    V_CREDITS();
    buffer_xpos = 0;
}


void SPLIT_BY_PICTURE(uint16_t ID, bool CLEAR_SCREEN) {
    set_global(0xb7, ID); // <SETG CURRENT-SPLIT .ID>
    if (CLEAR_SCREEN) {
        //         <CLEAR -1>)>
        glk_window_clear(windows[0].id);
        glk_window_clear(windows[1].id);
    }
    int width, height;
    get_image_size(ID, &width, &height);
    uint16_t Y = height + 1;
    windows[0].y_origin = Y;
    uint16_t X;
    if (get_global(0x83) == 0 && ID == 0x0183) { // <EQUAL? .ID ,TEXT-WINDOW-PIC-LOC>>
        X = 1;
    } else {
        X = width * pixelwidth + 1;
    }
    windows[0].x_origin = X;
    windows[0].y_size = gscreenh - Y;
    uint16_t x_size;
    if (get_global(0x83) == 0 && ID == 0x0183) { // <EQUAL? .ID ,TEXT-WINDOW-PIC-LOC>>
        x_size = gli_screenwidth;
    } else {
        x_size = gli_screenwidth - X * 2;
    }
    windows[0].x_size = x_size;
    v6_sizewin(&windows[0]);

    windows[1].x_origin = 1;
    windows[1].y_origin = 1;
    windows[1].x_size = gscreenw;
    windows[1].y_size = Y;
    v6_sizewin(&windows[1]);


    //1b96a:  cf 1f 6e 3b 00 03       LOADW           #6e3b,#00 -> L02
    //1b970:  54 03 01 08             ADD             L02,#01 -> L07
    //1b974:  a0 93 4e                JZ              G83 [FALSE] 1b983
    //1b977:  c1 8f 01 01 83 48       JE              L00,#0183 [FALSE] 1b983
    //1b97d:  e8 7f 01                PUSH            #01
    //1b980:  8c 00 0c                JUMP            1b98d
    //1b983:  cf 1f 6e 3b 01 04       LOADW           #6e3b,#01 -> L03
    //1b989:  54 04 01 00             ADD             L03,#01 -> -(SP)
    //1b98d:  be 10 6b 00 08 00       MOVE_WINDOW     #00,L07,(SP)+
}


// ;"Normal sizes/positions of windows 0 and 1."
// <ROUTINE SPLIT-BY-PICTURE (ID "OPT" (CLEAR-SCREEN? <>) "AUX" Y X YS XS)
//  <SETG CURRENT-SPLIT .ID>
//  <COND (.CLEAR-SCREEN?
//         <CLEAR -1>)>
//  <PICINF .ID ,PICINF-TBL>
//  <WINPOS ,S-TEXT <+ <SET Y <ZGET ,PICINF-TBL 0>> 1>
//  <COND (<AND <NOT ,BORDER-ON>
//         <EQUAL? .ID ,TEXT-WINDOW-PIC-LOC>>
//         1)
//  (T
//   <+ <SET X <ZGET ,PICINF-TBL 1>> 1>)>>
//  <WINSIZE ,S-TEXT <- <LOWCORE VWRD> .Y>
//  <COND (<AND <NOT ,BORDER-ON>
//         <EQUAL? .ID ,TEXT-WINDOW-PIC-LOC>>
//         <* ,WIDTH ,FONT-X>)
//  (T
//   <- <LOWCORE HWRD> <* .X 2>>)>>
//  <WINPOS ,S-WINDOW 1 1>
//  <WINSIZE ,S-WINDOW .Y <LOWCORE HWRD>>>


//Routine 1b958, 8 locals ;
//
//1b959:  2d 2e 01                STORE           G1e,L00
//1b95c:  a0 02 c6                JZ              L01 [TRUE] 1b963
//1b95f:  ed 3f ff ff             ERASE_WINDOW    #ffff
//1b963:  be 06 8f 01 6e 3b c2    PICTURE_DATA    L00,#6e3b [TRUE] 1b96a
//1b96a:  cf 1f 6e 3b 00 03       LOADW           #6e3b,#00 -> L02
//1b970:  54 03 01 08             ADD             L02,#01 -> L07
//1b974:  a0 93 4e                JZ              G83 [FALSE] 1b983
//1b977:  c1 8f 01 01 83 48       JE              L00,#0183 [FALSE] 1b983
//1b97d:  e8 7f 01                PUSH            #01
//1b980:  8c 00 0c                JUMP            1b98d
//1b983:  cf 1f 6e 3b 01 04       LOADW           #6e3b,#01 -> L03
//1b989:  54 04 01 00             ADD             L03,#01 -> -(SP)
//1b98d:  be 10 6b 00 08 00       MOVE_WINDOW     #00,L07,(SP)+
//1b993:  0f 00 12 00             LOADW           #00,#12 -> -(SP)
//1b997:  75 00 03 08             SUB             (SP)+,L02 -> L07
//1b99b:  a0 93 4f                JZ              G83 [FALSE] 1b9ab
//1b99e:  c1 8f 01 01 83 49       JE              L00,#0183 [FALSE] 1b9ab
//1b9a4:  76 87 aa 00             MUL             G77,G9a -> -(SP)
//1b9a8:  8c 00 0e                JUMP            1b9b7
//1b9ab:  0f 00 11 07             LOADW           #00,#11 -> L06
//1b9af:  56 04 02 00             MUL             L03,#02 -> -(SP)
//1b9b3:  75 07 00 00             SUB             L06,(SP)+ -> -(SP)
//1b9b7:  be 11 6b 00 08 00       WINDOW_SIZE     #00,L07,(SP)+
//1b9bd:  be 10 57 01 01 01       MOVE_WINDOW     #01,#01,#01
//1b9c3:  0f 00 11 00             LOADW           #00,#11 -> -(SP)
//1b9c7:  be 11 6b 01 03 00       WINDOW_SIZE     #01,L02,(SP)+
//1b9cd:  b0                      RTRUE


void PRINT_SPACES(int num) {
    for (int i = 0; i < num; i++)
        glk_put_char(' ');
}

#define H_INVERSE 1

// Draw LN inverted lines, unless INV is 0,
// then draw LN non-inverted lines.
void INVERSE_LINE(int LN, int INV) {
    if (INV == -1) {
        INV = H_INVERSE;
    }
    if (LN != 0) {
        glk_window_move_cursor(windows[1].id, 0, LN - 1);
    }
    glui32 width;
    glk_window_get_size(windows[1].id, &width, nullptr);
    garglk_set_reversevideo(INV);
    for (int i = 0; i < width; i++)
        glk_put_char(' ');
    garglk_set_reversevideo(0);
}


//35fd5:  ff 7f 02 c5             CHECK_ARG_COUNT #02 [TRUE] 35fdc
//35fd9:  0d 02 01                STORE           L01,#01
//35fdc:  a0 01 c8                JZ              L00 [TRUE] 35fe5
//35fdf:  f9 27 10 75 01 01       CALL_VN         12b04 (L00,#01)
//35fe5:  f1 bf 02                SET_TEXT_STYLE  L01
//35fe8:  a0 02 46                JZ              L01 [FALSE] 35fef
//35feb:  ee 7f 01                ERASE_LINE      #01
//35fee:  b0                      RTRUE
//35fef:  be 04 7f 04 00          SET_FONT        #04 -> -(SP)
//35ff4:  10 00 27 01             LOADB           #00,#27 -> L00
//35ff8:  be 13 1f ff fd 03 02    GET_WIND_PROP   #fffd,#03 -> L01
//35fff:  74 01 02 00             ADD             L00,L01 -> -(SP)
//36003:  77 00 01 00             DIV             (SP)+,L00 -> -(SP)
//36007:  3a c8 00                CALL_2N         ec50 ((SP)+)
//3600a:  10 00 1e 00             LOADB           #00,#1e -> -(SP)
//3600e:  c1 95 00 02 09 0a 57    JE              (SP)+,#02,#09,#0a [FALSE] 3602a
//36015:  f0 3f 70 bd             GET_CURSOR      #70bd
//36019:  cf 1f 70 bd 00 03       LOADW           #70bd,#00 -> L02
//3601f:  75 02 01 00             SUB             L01,L00 -> -(SP)
//36023:  ef af 03 00             SET_CURSOR      L02,(SP)+
//36027:  e5 7f 20                PRINT_CHAR      ' '
//3602a:  be 04 7f 01 00          SET_FONT        #01 -> -(SP)
//3602f:  f1 7f 00                SET_TEXT_STYLE  ROMAN
//36032:  b0                      RTRUE

// ; "Make text window extend to bottom of screen (more or less) or to
// top of bottom edge of border, depending..."
void ADJUST_TEXT_WINDOW(int ID) {
    int ADJ = 0;;
    if (ID) {
        int width, height;
        get_image_size(ID, &width, &height);
        ADJ = height;
    }
    int WINY = gscreenh - (int)windows[0].y_origin + 1 - ADJ;
    WINY = gcellh * (WINY / gcellh);
    windows[0].y_size = WINY;
    v6_sizewin(&windows[0]);
}

// ; "Make text window extend to bottom of screen (more or less) or to
// top of bottom edge of border, depending..."
// <ROUTINE ADJUST-TEXT-WINDOW (ID "AUX" (SCRY <LOWCORE VWRD>) WINY (ADJ 0))
// <COND (.ID
//         <PICINF .ID ,PICINF-TBL>
//         <SET ADJ <ZGET ,PICINF-TBL 0>>)>
//  <SET WINY <- .SCRY <- <WINGET ,S-TEXT ,WTOP> 1> .ADJ>>
//  <SET WINY <* ,FONT-Y </ .WINY ,FONT-Y>>>
//  <WINSIZE ,S-TEXT .WINY <WINGET ,S-TEXT ,WWIDE>>>

// <ROUTINE DISPLAY-BORDER (B "AUX" BL BR Y X)
// <SETG NEW-COMPASS T>
// <DISPLAY .B 1 1>
// <PICINF .B ,PICINF-TBL>
// <SET Y <GET ,PICINF-TBL 0>>
// <SET X <GET ,PICINF-TBL 1>>
// <COND (<EQUAL? .B ,OUTSIDE-BORDER>
//        <SET BL ,OUTSIDE-BORDER-L>
//        <SET BR ,OUTSIDE-BORDER-R>)
// (<EQUAL? .B ,CASTLE-BORDER>
//  <SET BL ,CASTLE-BORDER-L>
//  <SET BR ,CASTLE-BORDER-R>)
// (<EQUAL? .B ,UNDERGROUND-BORDER>
//  <SET BL ,UNDERGROUND-BORDER-L>
//  <SET BR ,UNDERGROUND-BORDER-R>)
// (<EQUAL? .B ,HINT-BORDER>
//  <SET BL ,HINT-BORDER-L>
//  <SET BR ,HINT-BORDER-R>)>
// <COND (<PICINF .BL ,PICINF-TBL>
//        <DISPLAY .BL <+ 1 .Y> 1>)>
// <COND (<PICINF .BR ,PICINF-TBL>
//        <DISPLAY .BR <+ 1 .Y> <+ 1 <- .X <GET ,PICINF-TBL 1>>>>)>
//     .B>

//int SET_BORDER(void) {
//    return 0;
//}

//Routine 1baf8, 6 locals ; <ROUTINE DISPLAY-BORDER (B "AUX" BL BR Y X)
//
//1baf9:  0d 7b 01                STORE           G6b,#01
//1bafc:  be 05 97 01 01 01       DRAW_PICTURE    L00,#01,#01
//1bb02:  be 06 8f 01 6e 3b c2    PICTURE_DATA    L00,#6e3b [TRUE] 1bb09
//1bb09:  cf 1f 6e 3b 00 04       LOADW           #6e3b,#00 -> L03
//1bb0f:  cf 1f 6e 3b 01 05       LOADW           #6e3b,#01 -> L04
//1bb15:  41 01 06 4f             JE              L00,#06 [FALSE] 1bb26
//1bb19:  cd 4f 02 01 f5          STORE           L01,#01f5
//1bb1e:  cd 4f 03 01 f6          STORE           L02,S166
//1bb23:  8c 00 32                JUMP            1bb56
//1bb26:  41 01 05 4f             JE              L00,#05 [FALSE] 1bb37
//1bb2a:  cd 4f 02 01 f1          STORE           L01,#01f1
//1bb2f:  cd 4f 03 01 f2          STORE           L02,#01f2
//1bb34:  8c 00 21                JUMP            1bb56
//1bb37:  41 01 07 4f             JE              L00,#07 [FALSE] 1bb48
//1bb3b:  cd 4f 02 01 f3          STORE           L01,S165
//1bb40:  cd 4f 03 01 f4          STORE           L02,#01f4
//1bb45:  8c 00 10                JUMP            1bb56
//1bb48:  41 01 08 4c             JE              L00,#08 [FALSE] 1bb56
//1bb4c:  cd 4f 02 01 f7          STORE           L01,#01f7
//1bb51:  cd 4f 03 01 f8          STORE           L02,#01f8
//1bb56:  be 06 8f 02 6e 3b 4c    PICTURE_DATA    L01,#6e3b [FALSE] 1bb67
//1bb5d:  34 01 04 00             ADD             #01,L03 -> -(SP)
//1bb61:  be 05 a7 02 00 01       DRAW_PICTURE    L01,(SP)+,#01
//1bb67:  be 06 8f 03 6e 3b c4    PICTURE_DATA    L02,#6e3b [TRUE] 1bb70
//1bb6e:  ab 01                   RET             L00
//1bb70:  34 01 04 06             ADD             #01,L03 -> L05
//1bb74:  cf 1f 6e 3b 01 00       LOADW           #6e3b,#01 -> -(SP)
//1bb7a:  75 05 00 00             SUB             L04,(SP)+ -> -(SP)
//1bb7e:  34 01 00 00             ADD             #01,(SP)+ -> -(SP)
//1bb82:  be 05 ab 03 06 00       DRAW_PICTURE    L02,L05,(SP)+
//1bb88:  ab 01                   RET             L00




// <ROUTINE BLANK-IT (TBL-SLOT COLS "AUX" (NCOLS 0) X)
// <COND (<L? .COLS 0>
//        <SET NCOLS <- 0 .COLS>>)>
// <SET X <- <GET ,SL-LOC-TBL .TBL-SLOT> <* ,FONT-X .NCOLS>>>
// <CURSET <GET ,SL-LOC-TBL <- .TBL-SLOT 1>> .X>
// <PRINT-SPACES <ABS .COLS>>
//     .X>
// 
// <ROUTINE DRAW-NEW-HERE ("AUX" X)
// <SETG OLD-HERE ,HERE>
// <CURSET <GET ,SL-LOC-TBL 0>
// <BLANK-IT 1 <COND (,NARROW? 18) (T 24)>>>
// <COND (<AND <OR ,NARROW?
//        <EQUAL? ,HERE ,PHIL-HALL>>
//        <SET X <GETP ,HERE ,P?APPLE-DESC>>>
//        <TELL .X>)
// (<EQUAL? ,HERE ,G-U-MOUNTAIN ,G-U-SAVANNAH ,G-U-HIGHWAY>
//  <TELL "Great Undergd. ">
//  <COND (<EQUAL? ,HERE ,G-U-MOUNTAIN>
//         <TELL "Mountain">)
//  (<EQUAL? ,HERE ,G-U-SAVANNAH>
//   <TELL "Savannah">)
//  (T
//   <TELL "Highway">)>)
// (T
//  <PRINTD ,HERE>)>>
// 
// <ROUTINE DRAW-NEW-REGION ()
// <SETG OLD-REGION <GETP ,HERE ,P?REGION>>
// <DIROUT ,D-TABLE-ON ,SLINE>    ;"start printing to buffer"
// <PUT ,SLINE 0 0>
// <TELL ,OLD-REGION>
// <DIROUT ,D-TABLE-OFF>
// <BLANK-IT 3 <COND (,NARROW? -19) (T -23)>>
// ;"longest region on Apple is FRIGID RIVER VALLEY"
// <CURSET <GET ,SL-LOC-TBL 2>
// <- <GET ,SL-LOC-TBL 3> <* ,FONT-X <GET ,SLINE 0>>>>
// <TELL ,OLD-REGION>>
// 
// <ROUTINE DRAW-NEW-SCORE ("AUX" Y X)
// <SETG SL-SCORE ,SCORE>
// <SET Y <+ <GET ,SL-LOC-TBL 2> ,FONT-Y>>
// <SET X <GET ,SL-LOC-TBL 3>>
// <CURSET .Y <- .X <* ,FONT-X 4>>>
// <PRINT-SPACES 4>
// <CURSET .Y <- .X <* ,FONT-X <COND (<OR <G? ,SCORE 999>
//                                    <L? ,SCORE -99>> 4)
// (<OR <G? ,SCORE 99>
//  <L? ,SCORE -9>> 3)
// (<OR <G? ,SCORE 9>
//  <L? ,SCORE 0>> 2)
// (T 1)>>>>
// <PRINTN ,SCORE>>


#define OUTSIDE_BORDER 6
#define OUTSIDE_BORDER_L 0x1f5
#define OUTSIDE_BORDER_R 0x1f6

#define CASTLE_BORDER 5
#define CASTLE_BORDER_L 0x1f1
#define CASTLE_BORDER_R 0x1f2

#define UNDERGROUND_BORDER 7
#define UNDERGROUND_BORDER_L 0x1f3
#define UNDERGROUND_BORDER_R 0x1f4

#define HINT_BORDER 8
#define HINT_BORDER_L 0x1f7
#define HINT_BORDER_R 0x1f8

void DISPLAY_BORDER(int border) {
    screenmode = MODE_NORMAL;
    if (current_graphics_buf_win != graphics_win_glk && current_graphics_buf_win != nullptr) {
        v6_delete_glk_win(current_graphics_buf_win);
    }
    current_graphics_buf_win = graphics_win_glk;
    glk_request_mouse_event(current_graphics_buf_win);

    set_global(0x6b, 1);
    clear_image_buffer();
    ensure_pixmap(current_graphics_buf_win);
    draw_to_pixmap_unscaled(border, 0, 0);
    int X, Y;
    get_image_size(border, &X, &Y);
    int BL = -1, BR = -1;
    switch(border) {
        case OUTSIDE_BORDER:
            BL = OUTSIDE_BORDER_L;
            BR = OUTSIDE_BORDER_R;
            break;
        case CASTLE_BORDER:
            BL = CASTLE_BORDER_L;
            BR = CASTLE_BORDER_R;
            break;
        case UNDERGROUND_BORDER:
            BL = UNDERGROUND_BORDER_L;
            BR = UNDERGROUND_BORDER_R;
            break;
        case HINT_BORDER:
            BL = HINT_BORDER_L;
            BR = HINT_BORDER_R;
            break;
    }
    if (find_image(BL)) {        
        draw_to_pixmap_unscaled(BL, 0, Y);
    }

    if (find_image(BR)) {
        int width;
        get_image_size(BR, &width, nullptr);
        draw_to_pixmap_unscaled(BR, X - width - 1, Y);
    }
}

#define COMPASS_PICSET_TBL 1

#define F_BOTTOM 423
#define F_SPLIT 422
#define PBOZ_BOTTOM 424

#define TEXT_WINDOW_PIC_LOC 387

#define SL_LOC_TBL 0x70a5

// <CONSTANT SL-LOC-TBL
// <TABLE 0 0 ;HERE-LOC
// 0 0 ;REGION-LOC
// 0 0 ;COMPASS-PIC-LOC
// 0 0 ;U-BOX-LOC
// 0 0 ;D-BOX-LOC
// 0 0 ;ICON-OFFSET>>

extern winid_t z0_right_status_window;

bool z0_init_status_line(bool DONT_CLEAR) {
//    set_global(0xb7, 0);  // <SETG OLD-HERE <>>
//    set_global(0x7d, 0);  // <SETG OLD-REGION <>>
//    set_global(0x79,-1);  // <SETG SL-SCORE -1>
    set_global(0x59, 1);  // <SETG COMPASS-CHANGED T>
    set_global(0x6b, 1);  // <SETG NEW-COMPASS T>

    bool border_on = get_global(0x83) == 1;
    if (!DONT_CLEAR) {
        // <CLEAR -1>
        if (windows[0].id != nullptr) {
            gli_delete_window(windows[0].id);
            windows[0].id = gli_new_window(wintype_TextBuffer, 0);
        }
        z0_clear_margin_images();
    }
//    SPLIT_BY_PICTURE(get_global(0x1e), false);

    internal_call_with_2_args(pack_routine(0x1b958), get_global(0x1e), 0);

    if (get_global(0x1e) == TEXT_WINDOW_PIC_LOC) {
        ADJUST_TEXT_WINDOW(0);
    } else if (get_global(0x1e) == F_SPLIT) { // (<EQUAL? ,CURRENT-SPLIT ,F-SPLIT>
            ADJUST_TEXT_WINDOW(F_BOTTOM);
            return true;
    } else {
        ADJUST_TEXT_WINDOW(PBOZ_BOTTOM);
        return true;
    }

    if (z0_left_status_window != nullptr) {
        if (current_graphics_buf_win == z0_left_status_window)
            current_graphics_buf_win = nullptr;
        gli_delete_window(z0_left_status_window);
        z0_left_status_window = nullptr;
    }
    if (z0_right_status_window != nullptr) {
        gli_delete_window(z0_right_status_window);
        z0_right_status_window = nullptr;
    }

    if (border_on) { // <COND (,BORDER-ON

        glk_stylehint_clear(wintype_TextGrid, style_Normal, stylehint_BackColor);

        if (graphics_type == kGraphicsTypeCGA || graphics_type == kGraphicsTypeMacBW) {
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, monochrome_black);
        } else {
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
        }

        set_current_window(&windows[7]); // <SCREEN ,S-FULL>
        set_global(0x14, internal_call(pack_routine(0x1bb8c))); // <SETG CURRENT-BORDER <SET-BORDER>>
        DISPLAY_BORDER(get_global(0x14));
//        PICSET(COMPASS_PICSET_TBL);
        for (int i = 1; i < 14; i++) {
            internal_clear_attr(0x166, i);
        }

    } else {
        set_current_window(&windows[1]); // <SCREEN ,S-WINDOW>
//        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_ReverseColor, 1);
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_background);
        glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_BackColor, user_selected_foreground);
    }

    set_global(0x9a, letterwidth); //  <SETG FONT-X <LOWCORE (FWRD 1)>>

    set_current_window(&windows[0]); // <SCREEN ,S-TEXT>

    return false;
}

void INIT_STATUS_LINE(void) {
    fprintf(stderr, "INIT-STATUS-LINE(DONT-CLEAR: %s)\n", variable(1) == 1 ? "true" : "false");
    z0_init_status_line(variable(1));
}


//FG_COLOR = 0->45; // Header byte 0x2d is default foreground color
//BG_COLOR = 0->44; // Header byte 0x2c is default background color

// Header byte 0x1e is interpreter number
// Header word 8 is flags 2, bit 6 means game wants to use colours

//if (0->30 == IBM && 0-->8 & 64 == 64) {
//    DEFAULT_FG = BLACK;
//    DEFAULT_BG = WHITE;
//    Default_Colors();
//} else if (0->30 == AMIGA) {
//    DEFAULT_FG = BLACK;
//    DEFAULT_BG = LIGHTGREY;
//    Default_Colors();
//}

#define PHIL_HALL 0xc7
#define G_U_MOUNTAIN 0xac
#define G_U_SAVANNAH 0xc3
#define G_U_HIGHWAY 0xce

#define P_APPLE_DESC 0x19

void DRAW_NEW_HERE(void) {
    set_global(0xb7, get_global(0x0b)); // OLD-HERE = HERE
                                        //    SET_CURSOR (user_word(SL_LOC_TBL), BLANK_IT(1, NARROW ? 18 : 24));
    bool NARROW = get_global(0x93) == 1;
    int16_t HERE = get_global(0x0b);

    if (NARROW || HERE == PHIL_HALL) {
        int16_t X = internal_get_prop(HERE, P_APPLE_DESC);
        if (X) {
            print_handler(X, nullptr);
            return;
        }
    }

    if (HERE == G_U_MOUNTAIN || HERE == G_U_SAVANNAH || HERE == G_U_HIGHWAY) {
        glk_put_string(const_cast<char*>("Great Undergd. "));
        //  <TELL "Great Undergd. ">
        if (HERE == G_U_MOUNTAIN) {
            // <TELL "Mountain">
        } else if (HERE == G_U_SAVANNAH) {
            glk_put_string(const_cast<char*>("Savannah"));
            // <TELL "Savannah">
        } else {
            glk_put_string(const_cast<char*>("Highway"));
            // <TELL "Highway">
        }
    } else {
        print_object(HERE, nullptr);
    }
}

#define P_REGION 0x27

void print_number(int number) {
    std::ostringstream ss;
    ss << number;
    for (const auto &c : ss.str()) {
        glk_put_char(c);
    }
}

void DRAW_NEW_SCORE(void) {
    int16_t SCORE = get_global(0x61);
    set_global(0x79, SCORE);

    int spaces;
    if (SCORE > 999 || SCORE < -99) {
        spaces = 0;
    } else if (SCORE > 99 || SCORE < -9) {
        spaces = 1;
    } else if (SCORE > 9 || SCORE < 0) {
        spaces = 2;
    } else {
        spaces = 3;
    }

    for (int i = 0; i < spaces; i++)
        glk_put_char(' ');

    print_number(SCORE);
}


// <ROUTINE DRAW-NEW-SCORE ("AUX" Y X)
// <SETG SL-SCORE ,SCORE>
// <SET Y <+ <GET ,SL-LOC-TBL 2> ,FONT-Y>>
// <SET X <GET ,SL-LOC-TBL 3>>
// <CURSET .Y <- .X <* ,FONT-X 4>>>
// <PRINT-SPACES 4>
// <CURSET .Y <- .X <* ,FONT-X <COND (<OR <G? ,SCORE 999>
//                                    <L? ,SCORE -99>> 4)
// (<OR <G? ,SCORE 99>
//  <L? ,SCORE -9>> 3)
// (<OR <G? ,SCORE 9>
//  <L? ,SCORE 0>> 2)
// (T 1)>>>>
// <PRINTN ,SCORE>>

#define MAX(a,b) (a > b ? a : b)

glui32 z0_right_status_width;

#define HERE_LOC 382

void z0_resize_status_windows(void) {

    bool BORDER_ON = (get_global(0x83) == 1);

//    for (auto &window : windows) {
//        if (window.id != nullptr && window.id->type == wintype_TextGrid && window.id != z0_left_status_window && window.id != z0_right_status_window) {
//            v6_delete_glk_win(window.id);
//            fprintf(stderr, "z0_resize_status_windows: deleting stray text grid window %d\n", window.index);
//        }
//    }

    if (z0_left_status_window == nullptr) {
        z0_left_status_window = v6_new_glk_window(wintype_TextGrid, 0);
        fprintf(stderr, "z0_resize_status_windows: creating a new left status window with peer %d\n", z0_left_status_window->peer);
        if (BORDER_ON)
            win_maketransparent(z0_left_status_window->peer);
    }

    int16_t CURRENT_BORDER = get_global(0x14);

    int imgwidth, imgheight;
    get_image_size(CURRENT_BORDER, &imgwidth, &imgheight);

    float xscale = (float)gscreenw / imgwidth; // width of an image "pixel"
    float yscale = xscale / pixelwidth;  // height of an image "pixel"

    int x, y, width, height;

    windows[1].x_cursor = 1;
    windows[1].y_cursor = 1;

    int hereoffx, hereoffy;
    get_image_size(HERE_LOC, &hereoffx, &hereoffy);

    height = 2 * (gcellh + ggridmarginy);

    int text_area_height = 9;

    if (graphics_type == kGraphicsTypeMacBW) {
        text_area_height = 18;
    } else if (graphics_type == kGraphicsTypeCGA) {
        text_area_height = 10;
    } else if (graphics_type == kGraphicsTypeEGA) {
        text_area_height = 10;
    }

    int text_area_y_offset = text_area_height * yscale - gcellh;

    x = (hereoffx - 1) * xscale - ggridmarginx + text_area_y_offset;
    y = (hereoffy - 1) * yscale - ggridmarginy + text_area_y_offset;


    if (!BORDER_ON) {
        windows[1].x_origin = 1;
        windows[1].y_origin = 1;

        if (z0_right_status_window != nullptr) {
            gli_delete_window(z0_right_status_window);
            z0_right_status_window = nullptr;
        }

        windows[1].x_size = gscreenw;
        windows[1].y_size = height;

        v6_sizewin(&windows[1]);

        glk_window_clear(z0_left_status_window);

        z0_right_status_width = (gscreenw - 2 * ggridmarginx) / gcellw;

        return;
    }

    if (z0_right_status_window == nullptr) {
        z0_right_status_window = v6_new_glk_window(wintype_TextGrid, 0);
        fprintf(stderr, "z0_resize_status_windows: creating a new right status window with peer %d\n", z0_right_status_window->peer);
        win_maketransparent(z0_right_status_window->peer);
    }


    uint16_t HERE = get_global(0x0b);

    int stringlength = count_characters_in_object(HERE);

    //    int width_in_chars = 13; // 13 characters to make room for "MOVES:" plus 2 spaces and 10000 moves
    int width_in_chars = MAX(stringlength, 13); // 13 characters to make room for "MOVES:" plus 2 spaces and 10000 moves

    width = width_in_chars * gcellw + 2 * ggridmarginx;

    fprintf(stderr, "z0_resize_status_windows: width_in_chars of left window: %d\n", width_in_chars);
    fprintf(stderr, "z0_resize_status_windows: z0_left_status_window: x:%d y:%d w:%d h:%d\n", x, y, width, height);

    z0_left_status_window->bbox.x0 = x;

    win_sizewin(z0_left_status_window->peer, x, y, x + width, y + height);
    glk_window_clear(z0_left_status_window);

    int16_t REGION = internal_get_prop(HERE, P_REGION);

    z0_right_status_width = MAX(count_characters_in_zstring(REGION), 10); // 10 characters to make room for "SCORE:" plus 1000 points

    fprintf(stderr, "z0_resize_status_windows: z0_right_status_width: %d\n", z0_right_status_width);

    width = z0_right_status_width * gcellw + 2 * ggridmarginx;

    x = gscreenw - x - width;

    fprintf(stderr, "z0_resize_status_windows: z0_right_status_window: x:%d y:%d w:%d h:%d\n", x, y, width, height);
    
    win_sizewin(z0_right_status_window->peer, x, y, x + width, y + height);

    // We trick the text printing routine into
    // thinking that it is printing to the left
    // window when it prints to the right one,
    // so we have to pretend that the left one
    // is much wider than it really is
    z0_left_status_window->bbox.x1 = x + width;

    glk_window_clear(z0_right_status_window);
}

void update_status_line(void) {
    // Set proportional font / style_Preformatted
    
    bool BORDER_ON = (get_global(0x83) == 1);
    bool COMPASS_CHANGED = (get_global(0x59) == 1);
    uint16_t HERE = get_global(0x0b);
    uint16_t REGION = internal_get_prop(HERE, P_REGION);

    // if interpreter is IBM and color flag is unset
    // or border is off
    // set reverse video

    z0_resize_status_windows();

    set_current_window(&windows[1]);
    glk_window_move_cursor(z0_left_status_window, 0, 0);

    DRAW_NEW_HERE();

    glk_window_move_cursor(z0_left_status_window, 0, 1);
    glk_put_string(const_cast<char*>("Moves:  "));
    print_number(get_global(0xe3));

    if (BORDER_ON) {
        if (COMPASS_CHANGED) {
            internal_call(pack_routine(0x1bd08));
            //        DRAW_NEW_COMP();
        }
        glk_set_window(z0_right_status_window);
    }

    glk_window_move_cursor(BORDER_ON ? z0_right_status_window : z0_left_status_window, z0_right_status_width - count_characters_in_zstring(REGION), 0);

    fprintf(stderr, "update_status_line: move cursor in z0_right_status_window to x:%d y:0\n", z0_right_status_width - count_characters_in_zstring(REGION));
    print_handler(unpack_string(REGION), nullptr);

    glk_window_move_cursor(BORDER_ON ? z0_right_status_window : z0_left_status_window, z0_right_status_width - 10, 1);

    glk_put_string(const_cast<char*>("Score:"));

    DRAW_NEW_SCORE();

    set_current_window(&windows[0]);
}


void UPDATE_STATUS_LINE(void) {
    update_status_line();
}

static int margin_images[100];
static int number_of_margin_images = 0;

static int counter = 0;

void z0_update_colors(void) {
    // Every system has its own default colors:
    // Amiga: black text on grey
    // IBM: EGA & VGA: black text on white. CGA: white text on black (on a real CGA monitor, "white" may be green or orange)
    // Macintosh: Black text on white
    // Apple 2: White text on black

    // In r393-s890714,
//    FG-COLOR   is global 0x1c
//    BG_COLOR   is global 0xcc
//    DEFAULT_FG is global 0x4f
//    DEFAULT_BG is global 0x81

    fprintf(stderr, "update_z0_colors: Called %d times\n", ++counter);

    uint16_t default_fg = get_global(0x4f);
    uint16_t default_bg = get_global(0x81);

    uint16_t current_fg = get_global(fg_global_idx);
    uint16_t current_bg = get_global(bg_global_idx);


    switch (options.int_number) {
        case INTERP_AMIGA:
            default_fg = BLACK_COLOUR;
            default_bg = LIGHTGREY_COLOUR;
            break;
        case INTERP_MSDOS:
            if (gli_z6_graphics == kGraphicsTypeCGA) {
                default_fg = WHITE_COLOUR;
                default_bg = BLACK_COLOUR;
            } else {
                default_fg = BLACK_COLOUR;
                default_bg = WHITE_COLOUR;
            }
            break;
        case INTERP_MACINTOSH:
            default_fg = BLACK_COLOUR;
            default_bg = WHITE_COLOUR;
            break;
        case INTERP_APPLE_IIE:
            if (gli_z6_graphics == kGraphicsTypeApple2) {
                default_fg = WHITE_COLOUR;
                default_bg = BLACK_COLOUR;
            } else {
                default_fg = BLACK_COLOUR;
                default_bg = WHITE_COLOUR;
            }
            break;
        default:
            fprintf(stderr, "ERROR! Illegal interpreter number!\n");
            break;
    }

    if (default_fg == 0)
        default_fg = 1;
    if (default_bg == 0)
        default_bg = 1;

    set_global(0x4f, default_fg);
    set_global(0x81, default_bg);

    if (graphics_type == kGraphicsTypeApple2) { // Only default colors are allowed with Apple 2 graphics
        current_fg = WHITE_COLOUR;
        current_bg = BLACK_COLOUR;
    } else {     // If machine is changed, the currently selected colors may not be available on the new machine. If so, reset them to default.
        if (options.int_number != INTERP_AMIGA) {
            if (current_fg == LIGHTGREY_COLOUR || current_fg == DARKGREY_COLOUR || current_bg == LIGHTGREY_COLOUR || current_bg == DARKGREY_COLOUR) {
                current_fg = default_fg;
                current_bg = default_bg;
            }
        }
    }

    if (counter == 1 && !(options.int_number != INTERP_MACINTOSH && graphics_type == kGraphicsTypeMacBW)) {
        current_fg = 1;
        current_bg = 1;
    }

    set_global(fg_global_idx, current_fg);
    set_global(bg_global_idx, current_bg);

    fprintf(stderr, "update_z0_colors: set FG-COLOR (global 0x1c) to %d\n", current_fg);
    fprintf(stderr, "update_z0_colors: set BG_COLOR (global 0xcc) to %d\n", current_bg);

    if (current_fg == 1) {
        current_fg = default_fg;
    }

    if (current_bg == 1) {
        current_bg = default_bg;
    }

    // When gli_enable_styles is true, all Spatterlight color settings (gfgcol, gbgcol) are ignored

    if (gli_enable_styles) {
        user_selected_foreground = rgb_colour_from_index(current_fg);
        user_selected_background = rgb_colour_from_index(current_bg);
        
        glk_stylehint_set(wintype_TextBuffer, style_Input, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_TextColor, user_selected_foreground);
        if (options.int_number == INTERP_MSDOS && gli_z6_graphics == kGraphicsTypeCGA)
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_background);
        else
            glk_stylehint_set(wintype_TextGrid, style_Normal, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Preformatted, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Subheader, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Emphasized, stylehint_TextColor, user_selected_foreground);
        glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_BackColor, user_selected_background);
    }

    if (gli_z6_colorize &&
        (gli_z6_graphics == kGraphicsTypeCGA || gli_z6_graphics == kGraphicsTypeMacBW)) {
        monochrome_black = darkest(user_selected_foreground, user_selected_background);
        monochrome_white = brightest(user_selected_foreground, user_selected_background);
    } else {
        monochrome_black = 0;
        monochrome_white = 0xffffff;
    }
}

void z0_update_on_resize(void) {
    // Window 0 is S-TEXT, buffer text window
    // Window 1 is S-WINDOW, status text grid
    // Window 7 is S-FULL, the entire background

//    if (current_graphics_buf_win)
//        glk_window_set_background_color(current_graphics_buf_win, user_selected_background);
//    if (windows[0].id)
//        win_setbgnd(windows[0].id->peer, user_selected_background);

    // Input text color should also be the same as the other text.

    // Global 0x2f is MACHINE

    int MACHINE = byte(0x1e);
    fprintf(stderr, "HW interpreter number (byte 0x1e): %d options.int_number:%lu\n", MACHINE, options.int_number);

    int FONT_X = byte(0x27);
    int FONT_Y = byte(0x26);

    fprintf(stderr, "HW font width (byte 0x1e): %d gcellw: %f HW font height (byte 0x1e): %d gcellh:%f\n", FONT_X, gcellw, FONT_Y, gcellh);


    int WIDTH = word(17 * 2);
    int HEIGHT = word(18 * 2);

    fprintf(stderr, "HW screen width (word 18): %d gscreenw: %d HW screen height (word 17): %d gscreenh:%d\n", WIDTH, gscreenw, HEIGHT, gscreenh);

    WIDTH = byte(33);

    fprintf(stderr, "WIDTH (Global 119) (byte 33): %d\n", WIDTH);

    int stored_margin_image_number = number_of_margin_images;

    glk_request_timer_events(0);
    
    internal_call(pack_routine(0x1ca6c)); // SETUP-SCREEN
//    if (screenmode == MODE_NORMAL) {
        internal_call_with_arg(pack_routine(0x13614), 1); // V-$REFRESH(DONT-CLEAR:true)
        if (windows[0].id) {
            z0_refresh_margin_images();
            float yscalefactor = 2.0;
            if (graphics_type == kGraphicsTypeMacBW)
                yscalefactor = (float)gscreenw / hw_screenwidth;
            float xscalefactor = yscalefactor * pixelwidth;
            win_refresh(windows[0].id->peer, xscalefactor, yscalefactor);
            win_setbgnd(windows[0].id->peer, user_selected_background);
        }
//    } else if (screenmode == MODE_MAP) {
//
//    }
    flush_image_buffer();

    number_of_margin_images = stored_margin_image_number;
}

void z0_resize_windows_after_restore(void) {
    
}


void z0_add_margin_image(int image) {
    for (int i = 0; i < number_of_margin_images; i++) {
        if (margin_images[i] == image) {
            // We have already added this image to the list
            return;
        }
    }
    fprintf(stderr, "z0_add_margin_image: Adding margin image %d to the update list\n", image);
    margin_images[number_of_margin_images] = image;
    number_of_margin_images++;
}

void z0_clear_margin_images(void) {
    number_of_margin_images = 0;
}

void z0_refresh_margin_images(void) {
    // We uncache all used margin images and create new ones

    for (int i = 0; i < number_of_margin_images; i++) {
        recreate_image(margin_images[i], false);
    }
}

