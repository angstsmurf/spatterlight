/* $Header: d:/cvsroot/tads/html/win32/htmlres.h,v 1.4 1999/07/11 00:46:46 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  htmlres.h - HTML-TADS resource header
Function
  
Notes
  
Modified
  10/26/97 MJRoberts  - Creation
*/

#ifndef HTMLRES_H
#define HTMLRES_H


#define IDC_STATIC                      -1

#define IDI_EXE_ICON                    1
#define IDI_GAM_ICON                    2
#define IDI_SAV_ICON                    3
#define IDI_TDC_ICON                    4
#define IDI_GAM3_ICON                   5
#define IDI_SAV3_ICON                   6
#define IDI_T_ICON                      7
#define IDI_H_ICON                      8
#define IDI_TL_ICON                     9
#define IDI_RESFILE_ICON                10
#define IDI_SYSFILE_ICON                11
#define IDI_MAINWINICON                 12
#define IDI_DBGWIN                      13

#define IDI_DBG_CURLINE                 200
#define IDI_DBG_BP                      201
#define IDI_DBG_BPDIS                   202
#define IDI_DBG_CURBP                   203
#define IDI_DBG_CURBPDIS                204
#define IDI_DBG_CTXLINE                 205
#define IDI_DBG_CTXBP                   206
#define IDI_DBG_CTXBPDIS                207
#define IDI_BOX_PLUS                    208
#define IDI_BOX_MINUS                   209
#define IDI_VDOTS                       210
#define IDI_HDOTS                       211
#define IDI_NO_SOUND                    212
#define IDB_BMP_PAT50                   213
#define IDB_BMP_SMALL_CLOSEBOX          214
#define IDB_BMP_START_NEW               215
#define IDB_BMP_START_OPEN              216
#define IDB_PROJ_ROOT                   217
#define IDB_PROJ_FOLDER                 218
#define IDB_PROJ_SOURCE                 219
#define IDB_PROJ_RESOURCE               220
#define IDB_PROJ_RES_FOLDER             221
#define IDB_PROJ_EXTRESFILE             222
#define IDB_PROJ_IN_INSTALL             226
#define IDB_PROJ_NOT_IN_INSTALL         227
#define IDB_TERP_TOOLBAR                228
#define IDI_MOVE_UP                     229
#define IDI_MOVE_DOWN                   230
#define IDB_CHEST_SEARCH                231
#define IDB_PROJ_LIB                    232
#define IDB_PROJ_CHECK                  233
#define IDB_PROJ_NOCHECK                234

#define IDS_MORE                        4
#define IDS_DEBUG_GO                    5
#define IDS_DEBUG_STEPINTO              6
#define IDS_DEBUG_STEPOVER              7
#define IDS_DEBUG_RUNTOCURSOR           8
#define IDS_DEBUG_BREAK                 9
#define IDS_DEBUG_SETCLEARBREAKPOINT    10
#define IDS_DEBUG_DISABLEBREAKPOINT     11
#define IDS_DEBUG_EVALUATE              12
#define IDS_VIEW_GAMEWIN                13
#define IDS_VIEW_WATCH                  14
#define IDS_VIEW_LOCALS                 15
#define IDS_VIEW_STACK                  16
#define IDS_DEBUG_SHOWNEXTSTATEMENT     17
#define IDS_DEBUG_SRCWIN                18
#define IDS_DEBUG_STEPOUT               19
#define IDS_DEBUG_SETNEXTSTATEMENT      20
#define IDS_FILE_SAFETY                 21
#define IDS_QUITTING                    22
#define IDS_MEM                         23
#define IDS_DEBUG_PROMPTS               24
#define IDS_DEBUG_EDITOR                25
#define IDS_BUILD_SRC                   26
#define IDS_BUILD_INC                   27
#define IDS_BUILD_OUTPUT                28
#define IDS_BUILD_DEFINES               29
#define IDS_BUILD_ADVANCED              30
#define IDS_NEWWIZ_WELCOME              31
#define IDS_NEWWIZ_SOURCE               32
#define IDS_NEWWIZ_GAMEFILE             33
#define IDS_NEWWIZ_TYPE                 34
#define IDS_NEWWIZ_SUCCESS              35
#define IDS_LDSRCWIZ_WELCOME            36
#define IDS_LDSRCWIZ_GAMEFILE           37
#define IDS_LDSRCWIZ_SUCCESS            38
#define IDS_BUILD_INSTALL               39
#define IDS_INFO_DISPNAME               40
#define IDS_INFO_SAVE_EXT               41
#define IDS_INFO_ICON                   42
#define IDS_INFO_LICENSE                43
#define IDS_INFO_PROGDIR                44
#define IDS_INFO_STARTFOLDER            45
#define IDS_INFO_NONE                   46
#define IDS_BUILD_COMPDBG               47
#define IDS_BUILD_COMP_AND_RUN          48
#define IDS_STARTING                    49
#define IDS_DEBUG_SRCPATH               50
#define IDS_INFO_README                 51
#define IDS_INFO_README_TITLE           52
#define IDS_FILE_LOADGAME               53
#define IDS_FILE_SAVEGAME               54
#define IDS_FILE_RESTOREGAME            55
#define IDS_EDIT_CUT                    56
#define IDS_EDIT_COPY                   57
#define IDS_EDIT_PASTE                  58
#define IDS_CMD_UNDO                    59
#define IDS_VIEW_FONTS_NEXT_SMALLER     60
#define IDS_VIEW_FONTS_NEXT_LARGER      61
#define IDS_VIEW_OPTIONS                62
#define IDS_GO_PREVIOUS                 63
#define IDS_GO_NEXT                     64
#define IDS_EDIT_FIND                   65
#define IDS_FILE_TERMINATEGAME          66
#define IDS_BTN_ADD_GAMES               67
#define IDS_GCH_SEARCH_FOLDER_TITLE     68
#define IDS_GCH_SEARCH_FOLDER_PROMPT    69
#define IDS_GAME_CHEST_SEARCH_ROOT      70
#define IDS_GAME_CHEST_SEARCH_RESULTS   71
#define IDS_GAME_CHEST_SEARCH_GROUP     72
#define IDS_BUILD_DIAGNOSTICS           73
#define IDS_DEBUG_STARTOPT              74
#define IDS_GAMECHEST_PREFS             75
#define IDS_MANAGE_PROFILES             76
#define IDS_CUSTOMIZE_THEME             77
#define IDS_BUILD_INTERMEDIATE          78
#define IDS_INFO_CHARLIB                79
#define IDS_APPEARANCE                  80
#define IDS_KEYS                        81
#define IDS_COLORS                      82
#define IDS_FONT                        83
#define IDS_MEDIA                       84
#define IDS_THEMEDESC_MULTIMEDIA        85
#define IDS_THEMEDESC_PLAIN_TEXT        86
#define IDS_THEMEDESC_WEB_STYLE         87
#define IDS_LINKS_ALWAYS                88
#define IDS_LINKS_CTRL                  89
#define IDS_LINKS_NEVER                 90
#define IDS_THEMES_DROPDOWN             91
#define IDS_MUTE_SOUND                  92
#define IDS_BUILD_STOP                  93
#define IDS_MORE_STATUS_MSG             94
#define IDS_WORKING_MSG                 95
#define IDS_PRESS_A_KEY_MSG             96
#define IDS_EXIT_PAUSE_MSG              97
#define IDS_MORE_PROMPT                 98
#define IDS_FIND_NO_MORE                99
#define IDS_NO_GAME_MSG                 100
#define IDS_GAME_OVER_MSG               101
#define IDS_CANNOT_OPEN_HREF            102
#define IDS_LINK_PREF_CHANGE            103
#define IDS_SHOW_DBG_WIN_MENU           104
#define IDS_ABOUT_GAME_WIN_TITLE        105
#define IDS_NO_RECENT_GAMES_MENU        106
#define IDS_REALLY_QUIT_MSG             107
#define IDS_REALLY_GO_GC_MSG            108
#define IDS_REALLY_NEW_GAME_MSG         109
#define IDS_CHOOSE_GAME                 110
#define IDS_CHOOSE_NEW_GAME             111
#define IDS_ABOUTBOX_1                  112
#define IDS_ABOUTBOX_2                  113
#define IDS_COMCTL32_WARNING            114
#define IDS_SET_DEF_PROFILE             115
#define IDS_NEWWIZ_BIBLIO               116


#define DLG_COLORS                      107
#define DLG_KEYS                        108
#define DLG_FONT                        109
#define DLG_MORE                        110
#define DLG_FILE_SAFETY                 111
#define DLG_MEDIA                       112
#define IDB_BMPMORE1                    113
#define IDB_BMPMORE2                    114
#define IDR_MAIN_MENU                   115
#define IDR_EDIT_POPUP_MENU             116
#define IDR_DEBUGWIN_MENU               117
#define IDR_DEBUGMAIN_MENU              118
#define IDR_DEBUG_POPUP_MENU            119
#define DLG_MEM                         120
#define IDC_SPLIT_EW_CSR                128
#define IDC_SPLIT_NS_CSR                129
#define IDB_DEBUG_TOOLBAR               131
#define IDB_BMP_TABPIX                  135
#define IDR_DEBUG_EXPR_POPUP            137
#define IDR_PROJ_POPUP_MENU             138
#define IDB_VDOTS                       140
#define IDB_HDOTS                       141
#define IDR_MAINWIN_POPUP               142
#define DLG_APPEARANCE                  143
#define IDR_DOCKWIN_POPUP               150
#define IDR_DOCKEDWIN_POPUP             151
#define IDR_DOCKABLEMDI_POPUP           152
#define ID_DOCKWIN_DOCK                 153
#define ID_DOCKWIN_UNDOCK               154
#define ID_DOCKWIN_HIDE                 155
#define ID_DOCKWIN_DOCKVIEW             156
#define IDR_MDICLIENT_POPUP             157
#define IDR_DEBUGLOG_POPUP              158
#define IDC_DRAG_MOVE_CSR               159
#define IDC_BMP_1                       160
#define IDC_BMP_2                       161
#define IDR_ACCEL_WIN                   900
#define IDR_ACCEL_EMACS                 901
#define IDR_ACCEL_DEBUGMAIN             902
#define IDC_RB_CTLV_EMACS               1002
#define IDC_RB_CTLV_PASTE               1003
#define IDC_RB_ARROW_SCROLL             1004
#define IDC_RB_ARROW_HIST               1005
#define IDC_MAINFONTPOPUP               1006
#define IDC_MONOFONTPOPUP               1007
#define IDC_TXTCOLOR                    1008
#define IDC_LINKCOLOR                   1009
#define IDC_HLINKCOLOR                  1010
#define IDC_ALINKCOLOR                  1011
#define IDC_BKCOLOR                     1012
#define IDC_CK_USEWINCLR                1013
#define IDC_CK_OVERRIDECLR              1014
#define IDC_CK_UNDERLINE                1020
#define IDC_RB_MORE_NORMAL              1022
#define IDC_RB_MORE_ALT                 1023
#define IDC_CK_HOVER                    1024
#define IDC_RB_SAFETY0                  1025
#define IDC_RB_SAFETY1                  1026
#define IDC_RB_SAFETY2                  1027
#define IDC_RB_SAFETY3                  1028
#define IDC_RB_SAFETY4                  1029
#define IDC_RB_ALTV_EMACS               1030
#define IDC_RB_ALTV_WIN                 1031
#define IDC_STXTCOLOR                   1032
#define IDC_SBKCOLOR                    1033
#define IDC_FONTSERIF                   1034
#define IDC_FONTSANS                    1035
#define IDC_FONTSCRIPT                  1036
#define IDC_FONTTYPEWRITER              1037
#define IDC_POP_MAINFONTSIZE            1038
#define IDC_POP_MONOFONTSIZE            1039
#define IDC_POP_SERIFFONTSIZE           1040
#define IDC_POP_SANSFONTSIZE            1041
#define IDC_POP_SCRIPTFONTSIZE          1042
#define IDC_POP_TYPEWRITERFONTSIZE      1043
#define IDC_POP_INPUTFONTSIZE           1044
#define IDC_CK_INPFONT_NORMAL           1101
#define IDC_POP_INPUTFONT               1102
#define IDC_BTN_INPUTCOLOR              1103
#define IDC_CK_INPUTBOLD                1104
#define IDC_CK_INPUTITALIC              1105
#define IDC_CK_GRAPHICS                 1106
#define IDC_CK_SOUND_FX                 1107
#define IDC_CK_MUSIC                    1108
#define IDC_POP_SHOW_LINKS              1109
#define IDC_POP_MEM                     1150
#define DLG_BREAKPOINTS                 2000
#define IDC_LB_BP                       2001
#define IDC_BTN_NEWGLOBAL               2002
#define IDC_BTN_GOTOCODE                2003
#define IDC_BTN_CONDITION               2004
#define IDC_BTN_REMOVE                  2005
#define IDC_BTN_REMOVEALL               2006
#define IDC_BTN_DISABLEBP               2007
#define DLG_BPCOND                      2050
#define IDC_EDIT_COND                   2051
#define IDC_RB_TRUE                     2052
#define IDC_RB_CHANGED                  2053
#define DLG_FIND                        2100
#define IDC_CBO_FINDWHAT                2101
#define IDC_CK_MATCHCASE                2102
#define IDC_CK_WRAP                     2103
#define IDC_CK_STARTATTOP               2104
#define IDC_RD_UP                       2105
#define IDC_RD_DOWN                     2106
#define DLG_DEBUG_SRCWIN                2200
#define IDC_BTN_DBGTXTCLR               2201
#define IDC_BTN_DBGBKGCLR               2202
#define IDC_CK_DBGWINCLR                2203
#define IDC_CBO_DBGFONT                 2204
#define IDC_CBO_DBGFONTSIZE             2205
#define IDC_CBO_DBGTABSIZE              2206
#define DLG_DIRECTX_WARNING             2320
#define IDC_STATIC_DXVSN                2321
#define IDC_STATIC_DXINST               2322
#define IDC_CK_SUPPRESS                 2323
#define IDC_STATIC_DXVSN2               2324
#define IDC_STATIC_DXINST2              2325
#define IDC_STATIC_DXVSN3               2326
#define IDC_STATIC_DXINST3              2337
#define IDC_POP_THEME                   2338
#define IDC_STATIC_SAMPLE               2339
#define IDC_BTN_CUSTOMIZE               2340

#define DLG_QUITTING                    2350
#define IDC_RB_CLOSE_QUITCMD            2351
#define IDC_RB_CLOSE_PROMPT             2352
#define IDC_RB_CLOSE_IMMEDIATE          2353
#define IDC_RB_POSTQUIT_EXIT            2354
#define IDC_RB_POSTQUIT_KEEP            2355

#define DLG_STARTING                    2360
#define IDC_CK_ASKGAME                  2361
#define IDC_FLD_INITFOLDER              2362

#define DLG_GAMECHEST_PREFS             2370
#define IDC_FLD_GC_FILE                 2371
#define IDC_FLD_GC_BKG                  2372
#define IDC_BTN_DEFAULT                 2373
#define IDC_BTN_DEFAULT2                2374

#define DLG_PROFILES                    2380
#define IDC_CBO_PROFILE                 2381

#define DLG_NEW_PROFILE                 2390
#define IDC_FLD_PROFILE                 2391

#define IDD_FOLDER_SELECTOR2            2400
#define IDC_BTN_NETWORK                 2401
#define IDC_FLD_FOLDER_PATH             2402
#define IDC_LB_FOLDERS                  2403
#define IDC_CBO_DRIVES                  2404
#define IDC_TXT_PROMPT                  2405

#define DLG_BUILD_DIAGNOSTICS           2410
#define IDC_CK_VERBOSE                  2411
#define IDC_RB_WARN_NONE                2412
#define IDC_RB_WARN_STANDARD            2413
#define IDC_RB_WARN_PEDANTIC            2414
#define IDC_CK_CLR_DBGLOG               2415

#define DLG_DBGWIZ_START                2420
#define IDC_CK_SHOWAGAIN                2421
#define IDC_BTN_CREATE                  2422
#define IDC_BTN_OPEN                    2423
#define IDC_ST_CREATE                   2424
#define IDC_ST_OPEN                     2425
#define IDC_ST_WELCOME                  2426

#define DLG_DEBUG_PROMPTS               2430
#define IDC_CK_SHOW_WELCOME             2431
#define IDC_CK_SHOW_GAMEOVER            2432
#define IDC_CK_SHOW_BPMOVE              2433
#define IDC_CK_ASK_EXIT                 2434
#define IDC_CK_ASK_LOAD                 2435
#define IDC_CK_ASK_RLSRC                2436

#define DLG_DEBUG_EDITOR                2450
#define IDC_FLD_EDITOR                  2451
#define IDC_FLD_EDITOR_ARGS             2452
#define IDC_BTN_BROWSE                  2453
#define IDC_BTN_ADVANCED                2454
#define IDC_BTN_AUTOCONFIG              2455

#define DLG_DBG_EDIT_ADV                2460
#define IDC_FLD_DDESERVER               2461
#define IDC_FLD_DDETOPIC                2462
#define IDC_CK_USE_DDE                  2463
#define IDC_FLD_DDEPROG                 2464

#define DLG_BUILD_SRC                   2470
#define IDC_FLD_SOURCE                  2471
#define IDC_FLD_RSC                     2472
#define IDC_BTN_ADDFILE                 2473
#define IDC_BTN_ADDDIR                  2474
#define IDC_BTN_ADDRS                   2475

#define DLG_BUILD_OUTPUT                2480
#define IDC_FLD_GAM                     2481
#define IDC_BTN_BROWSE2                 2482
#define IDC_FLD_EXE                     2483
#define IDC_FLD_RLSGAM                  2484
#define IDC_BTN_BROWSE3                 2485
#define IDC_BTN_BROWSE4                 2486

#define DLG_BUILD_ADVANCED              2490
#define IDC_CK_CASE                     2491
#define IDC_CK_C_OPS                    2492
#define IDC_CK_CHARMAP                  2493
#define IDC_FLD_CHARMAP                 2494
#define IDC_FLD_OPTS                    2495
#define IDC_BTN_HELP                    2496
#define IDC_CK_PREINIT                  2497
#define IDC_CK_DEFMOD                   2489

#define DLG_BUILD_INC                   2500
#define IDC_FLD_INC                     2501

#define DLG_BUILD_DEFINES               2510
#define IDC_FLD_DEFINES                 2511
#define IDC_FLD_UNDEF                   2512
#define IDC_BTN_ADD                     2513

#define DLG_ADD_MACRO                   2520
#define IDC_FLD_MACRO                   2521
#define IDC_FLD_MACRO_EXP               2522

#define DLG_BUILD_WAIT                  2530
#define IDC_BUILD_DONE                  2531

#define DLG_BUILD_INSTALL               2540
#define IDC_FLD_INSTALL_EXE             2541
#define IDC_FLD_INSTALL_OPTS            2542
#define IDC_BTN_EDIT                    2543

#define DLG_INSTALL_OPTIONS             2550
#define IDC_FLD_DISPNAME                2551
#define IDC_FLD_SAVE_EXT                2552
#define IDC_FLD_ICON                    2553
#define IDC_FLD_LICENSE                 2554
#define IDC_FLD_PROGDIR                 2555
#define IDC_FLD_STARTFOLDER             2556
#define IDC_FLD_INFO                    2557
#define IDC_FLD_README                  2558
#define IDC_FLD_README_TITLE            2559
#define IDC_FLD_CHARLIB                 2560

#define DLG_DEBUG_SRCPATH               2570
#define IDC_FLD_SRCPATH                 2571

#define DLG_DEBUG_STARTOPT              2580
#define IDC_RB_START_WELCOME            2581
#define IDC_RB_START_RECENT             2582
#define IDC_RB_START_NONE               2583

#define DLG_NEWWIZ_WELCOME              2600
#define DLG_NEWWIZ_SOURCE               2601
#define DLG_NEWWIZ_GAMEFILE             2602
#define DLG_NEWWIZ_TYPE                 2603
#define DLG_NEWWIZ_SUCCESS              2604
#define DLG_LDSRCWIZ_WELCOME            2605
#define DLG_LDSRCWIZ_GAMEFILE           2606
#define DLG_LDSRCWIZ_SUCCESS            2607

#define IDC_FLD_NEWSOURCE               2610
#define IDC_FLD_NEWGAMEFILE             2611
#define IDC_RB_INTRO                    2612
#define IDC_RB_ADV                      2613
#define IDB_DEBUG_NEWGAME               2614
#define IDC_TXT_CONFIG_OLD              2615
#define IDC_TXT_CONFIG_NEW              2616
#define IDC_RB_CUSTOM                   2617
#define IDC_RB_PLAIN                    2618

#define DLG_PROGARGS                    2700
#define IDC_FLD_CMDFILE                 2701
#define IDC_FLD_LOGFILE                 2702
#define IDC_FLD_PROGARGS                2703
#define IDC_BTN_SEL_CMDFILE             2704
#define IDC_BTN_SEL_LOGFILE             2705

#define DLG_DBG_EDITOR_AUTOCONFIG       2720
#define IDC_LB_EDITORS                  2721

#define DLG_GOTOLINE                    2740
#define IDC_EDIT_LINENUM                2741

#define DLG_GAME_CHEST_URL              2800
#define IDC_FLD_KEY                     2801
#define IDC_FLD_NAME                    2802
#define IDC_FLD_DESC                    2803
#define IDC_FLD_BYLINE                  2804
#define IDC_CB_DESC_HTML                2805
#define IDC_CB_BYLINE_HTML              2806

#define DLG_GAME_CHEST_GAME             2820
#define IDC_POP_GROUP                   2821

#define DLG_DOWNLOAD                    2830
#define IDC_TXT_URL                     2831
#define IDC_TXT_FILE                    2832
#define IDC_TXT_BYTES                   2833

#define DLG_GAME_CHEST_GROUPS           2840
#define IDC_TREE_GROUPS                 2841
#define IDC_BTN_DELETE                  2842
#define IDC_BTN_RENAME                  2843
#define IDC_BTN_NEW                     2844
#define IDC_BTN_CLOSE                   2845
#define IDC_BTN_MOVE_UP                 2846
#define IDC_BTN_MOVE_DOWN               2847

#define DLG_GAME_CHEST_SEARCH           2860
#define IDC_FLD_FOLDER                  2861
#define IDC_LB_RESULTS                  2862
#define IDC_MSG_SEARCH_DONE             2863

#define DLG_MISSING_SOURCE              2870
#define IDC_BTN_FIND                    2871

#define DLG_ABS_SOURCE                  2880
#define IDC_POP_PARENT                  2881

#define DLG_GAME_CHEST_SEARCH_ROOT      2900
#define IDC_TXT_FOLDER                  2901

#define DLG_GAME_CHEST_SEARCH_RESULTS   2920
#define IDC_TV_GAMES                    2921

#define DLG_GAME_CHEST_SEARCH_GROUP     2940

#define DLG_GAME_CHEST_SEARCH_BUSY      2960

#define DLG_SCAN_INCLUDES               2980

#define DLG_BUILD_INTERMEDIATE          3000
#define IDC_FLD_SYMDIR                  3001
#define IDC_FLD_OBJDIR                  3002

#define DLG_NEWWIZ_BIBLIO               3100
#define IDC_BIB_TITLE                   3101
#define IDC_BIB_AUTHOR                  3102
#define IDC_BIB_EMAIL                   3103
#define IDC_BIB_DESC                    3104

#define ID_FILE_ABOUT                   40001
#define ID_FILE_LOADGAME                40002
#define ID_EDIT_UNDO                    40003
#define ID_EDIT_CUT                     40004
#define ID_EDIT_COPY                    40005
#define ID_EDIT_PASTE                   40006
#define ID_EDIT_DELETE                  40007
#define ID_FILE_OPEN                    40008
#define ID_FILE_OPEN_NONE               40009
#define ID_WINDOW_NONE                  40010
#define ID_FILE_EDIT_TEXT               40011
#define ID_EDIT_SELECTALL               40012
#define ID_EDIT_SELECTLINE              40013
#define ID_VIEW_FONTS_SMALLEST          40015
#define ID_VIEW_FONTS_SMALLER           40016
#define ID_VIEW_FONTS_MEDIUM            40017
#define ID_VIEW_FONTS_LARGER            40018
#define ID_VIEW_FONTS_LARGEST           40019
#define ID_VIEW_OPTIONS                 40020
#define ID_VIEW_SOUNDS                  40021
#define ID_VIEW_MUSIC                   40022
#define ID_FILE_QUIT                    40023
#define ID_FILE_SAVEGAME                40024
#define ID_FILE_RESTOREGAME             40025
#define ID_FILE_RESTARTGAME             40026
#define ID_VIEW_LINKS                   40027
#define ID_VIEW_LINKS_HIDE              40028
#define ID_VIEW_LINKS_CTRL              40029
#define ID_VIEW_LINKS_TOGGLE            40030
#define ID_VIEW_GRAPHICS                40031
#define ID_FILE_VIEW_SCRIPT             40032
#define ID_FILE_RELOADGAME              40033
#define ID_FILE_TERMINATEGAME           40034
#define ID_WINDOW_CLOSE                 40035
#define ID_WINDOW_CLOSE_ALL             40036
#define ID_WINDOW_NEXT                  40037
#define ID_WINDOW_PREV                  40038
#define ID_WINDOW_CASCADE               40039
#define ID_WINDOW_TILE_HORIZ            40040
#define ID_WINDOW_TILE_VERT             40041
#define ID_FILE_CREATEGAME              40042
#define ID_FILE_EDITING_TEXT            40043
#define ID_FLUSH_GAMEWIN                40044
#define ID_FLUSH_GAMEWIN_AUTO           40045
#define ID_VIEW_FONTS_NEXT_SMALLER      40046
#define ID_VIEW_FONTS_NEXT_LARGER       40047
#define ID_CMD_UNDO                     40048
#define ID_VIEW_TOOLBAR                 40049
#define ID_EDIT_FIND                    20027
#define ID_EDIT_FINDNEXT                20028
#define ID_EDIT_GOTOLINE                20029
#define ID_GO_PREVIOUS                  40050
#define ID_GO_NEXT                      40051
#define ID_VIEW_TIMER                   40052
#define ID_VIEW_TIMER_HHMMSS            40053
#define ID_VIEW_TIMER_HHMM              40054
#define ID_TIMER_PAUSE                  40055
#define ID_TIMER_RESET                  40056
#define ID_GO_TO_GAME_CHEST             40057
#define ID_FILE_EXIT                    40058
#define ID_MUTE_SOUND                   40059
#define ID_FILE_HIDE                    40061
#define ID_SHOW_DEBUG_WIN               40070
#define ID_DEBUG_GO                     40100
#define ID_DEBUG_STEPINTO               40101
#define ID_DEBUG_STEPOVER               40102
#define ID_DEBUG_RUNTOCURSOR            40103
#define ID_DEBUG_EVALUATE               40104
#define ID_DEBUG_EDITBREAKPOINTS        40105
#define ID_DEBUG_STEPOUT                40106
#define ID_DEBUG_OPEN                   40107
#define ID_DEBUG_SHOWNEXTSTATEMENT      40108
#define ID_DEBUG_SETCLEARBREAKPOINT     40109
#define ID_DEBUG_DISABLEBREAKPOINT      40111
#define ID_DEBUG_SETNEXTSTATEMENT       40112
#define ID_DEBUG_BREAK                  40113
#define ID_DEBUG_SHOWHIDDENOUTPUT       40114
#define ID_DEBUG_HISTORYLOG             40115
#define ID_DEBUG_CLEARHISTORYLOG        40116
#define ID_DEBUG_OPTIONS                40117
#define ID_DEBUG_COPY                   40118
#define ID_DEBUG_ABORTCMD               40119
#define ID_DEBUG_ADDTOWATCH             40120
#define ID_BUILD_COMPDBG                40121
#define ID_BUILD_COMPRLS                40122
#define ID_BUILD_COMPEXE                40123
#define ID_BUILD_SETTINGS               40124
#define ID_BUILD_COMPINST               40125
#define ID_DEBUG_SHOWERRORLINE          40126
#define ID_BUILD_COMP_AND_RUN           40127
#define ID_FILE_RECENT_1                40128
#define ID_FILE_RECENT_2                40129
#define ID_FILE_RECENT_3                40130
#define ID_FILE_RECENT_4                40131
#define ID_FILE_RECENT_NONE             40132
#define ID_GAME_RECENT_1                40133
#define ID_GAME_RECENT_2                40134
#define ID_GAME_RECENT_3                40135
#define ID_GAME_RECENT_4                40136
#define ID_GAME_RECENT_NONE             40137
#define ID_BUILD_COMPDBG_FULL           40138
#define ID_DEBUG_PROGARGS               40139
#define ID_BUILD_CLEAN                  40140
#define ID_PROJ_SCAN_INCLUDES           40141
#define ID_BUILD_COMPDBG_AND_SCAN       40142
#define ID_THEMES_DROPDOWN              40143
#define ID_BUILD_STOP                   40144
#define ID_VIEW_STACK                   40200
#define ID_VIEW_TRACE                   40201
#define ID_VIEW_GAMEWIN                 40202
#define ID_VIEW_WATCH                   40203
#define ID_VIEW_LOCALS                  40204
#define ID_VIEW_PROJECT                 40205
#define ID_VIEW_STATUSLINE              40206
#define ID_APPEARANCE_OPTIONS           40207
#define ID_MANAGE_PROFILES              40208
#define ID_SET_DEF_PROFILE              40209
#define ID_HELP_TOPICS                  40250
#define ID_HELP_ABOUT                   40251
#define ID_HELP_ABOUT_GAME              40252
#define ID_HELP_COMMAND                 40253
#define ID_HELP_TADSMAN                 40254
#define ID_HELP_TADSPRSMAN              40255
#define ID_HELP_T3LIB                   40256
#define ID_HELP_CONTENTS                40257
#define ID_DBGEXPR_NEW                  40300
#define ID_DBGEXPR_EDITEXPR             40301
#define ID_DBGEXPR_EDITVAL              40302
#define ID_PROJ_OPEN                    40320
#define ID_PROJ_REMOVE                  40321
#define ID_PROJ_ADDFILE                 40322
#define ID_PROJ_ADDEXTRES               40323
#define ID_PROJ_ADDRESDIR               40324
#define ID_PROJ_INCLUDE_IN_INSTALL      40325
#define ID_PROFILER_START               40400
#define ID_PROFILER_STOP                40401

/*
 *   File-open subcommands for the line sources.  The line sources are
 *   numbered consecutively starting at zero; for line source number n, we
 *   generate a command ID of (ID_FILE_OPENLINESRC_FIRST + n).  We reserve
 *   space for 500 line sources, which is probably ten or twenty times
 *   more than even the largest game will actually use.  
 */
#define ID_FILE_OPENLINESRC_FIRST       41000
/* --------------- taken: 41001 through 41498 */
#define ID_FILE_OPENLINESRC_LAST        41499

/*
 *   Same idea for preference profiles.  Since these are used only in the
 *   game window, and the line source ID's are used only in the debugger
 *   window, we can re-use the same command ID's - we'll always know which we
 *   mean by virtue of which window is receiving them. 
 */
#define ID_PROFILE_FIRST                41000
#define ID_PROFILE_LAST                 41499

/*
 *   Same idea for items in the Window menu.  The windows are numbered
 *   starting at zero; for window n, we generate a command ID of
 *   (ID_WINDOW_FIRST + n).  
 */
#define ID_WINDOW_FIRST                 41500
/* --------------- taken: 41500 through 41999 */
#define ID_WINDOW_LAST                  41999

#endif /* HTMLRES_H */

