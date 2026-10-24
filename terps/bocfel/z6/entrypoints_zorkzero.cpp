//
//  entrypoints_zorkzero.cpp
//  bocfel6
//
// Zork Zero's rows of the entrypoint table and its globals finder. The table
// format and the search helpers are in entrypoints_internal.hpp, the scan
// that fills in the table in entrypoints.cpp.

#include "screen.h"
#include "dict.h"
#include "v6_shared.hpp"
#include "zorkzero.hpp"

#include "entrypoints.hpp"
#include "entrypoints_internal.hpp"

// Hook functions that only name a row (see entrypoints_internal.hpp)
static void BLINK(void) {}
static void WINPROP(void) {}
static void SET_BORDER(void) {}
static void DRAW_NEW_HERE(void) {}
static void DRAW_NEW_COMP(void) {}
static void DRAW_COMPASS_ROSE(void) {}
static void SETUP_SCREEN(void) {}
static void MAP_X(void) {}
static void PLAY_SELECTED(void) {}
static void SCORE_CHECK(void) {}
static void TOWER_WIN_CHECK(void) {}
static void DRAW_PEGS(void) {}
static void SET_B_PIC(void) {}

std::vector<EntryPoint> zorkzero_entrypoints = {
    {
        Game::ZorkZero,
        "UPDATE-STATUS-LINE",
        { 0x03, 0xeb, 0x7f, 0x01, WILDCARD, 0x04, 0x7f, 0x04, 0x00 },
        1,
        0,
        true,
        z0_UPDATE_STATUS_LINE
    },

    // Shared with Shogun
    {
        Game::ZorkZero,
        "V-DEFINE",
        { 0x98, 0x40, 0x00, 0xa0, 0x00, 0xca },
        0,
        0,
        true,
        V_DEFINE
    },

    {
        Game::ZorkZero,
        "V-DEFINE alt",
        { 0xed, 0x7f, 0x00, 0x36, 0x04, 0x01, 0x00, 0x34, 0x02, 0x00, 0x00 },
        0,
        0,
        true,
        V_DEFINE
    },

    // BLINK-WHILE-AWAITING-INPUT: SET_WINDOW L04; DRAW_PICTURE L01 L02 L03;
    // SET_WINDOW #00. Its handler reads zg.BLINK_TBL from the STOREW that
    // immediately follows (the release/r366 map drives the blinking marker via
    // that table; the r242/r296/r66 betas store the icon in a frame local
    // instead, so the BLINK_TBL search there simply finds nothing and the beta
    // redraw path is used). global_search because the routine sits at a low
    // address in some revisions (r366: 0x130f5) and the forward cursor overshoots
    // it, leaving zg.BLINK_TBL unset so the wrong (beta) resize path is taken and
    // the you-are-here marker is never repositioned. The pattern is globally
    // unique in every revision that has it (and absent in the true betas).
    {
        Game::ZorkZero,
        "BLINK",
        { 0xeb, 0xbf, 0x05, 0xbe, 0x05, 0xab, 0x02, 0x03, 0x04, 0xeb, 0x7f, 0x00 },
        0,
        0,
        false,
        BLINK,
        true,
    },

    {
        Game::ZorkZero,
        "V_REFRESH",
        // Anchor on V-REFRESH's own prologue -- locals-count 01, then the
        // routine's first two instructions, whose shape (41 xx 02 48 8f) is
        // stable across revisions; only the second byte (a global number)
        // varies. The previous signature keyed off the *preceding* routine's
        // tail, which is not stable: it false-matched a different routine in
        // r296 and r343 (and matched nothing for the correct routine there).
        // offset 1 -> found_at_address = routine + 1 (zr.V_REFRESH = found-1).
        { 0x01, 0x41, WILDCARD, 0x02, 0x48, 0x8f },
        1,
        0,
        false,
        V_REFRESH,
        // V-REFRESH sits before V-DEFINE in r366 (and after it in r393), so a
        // forward-only search from the V-DEFINE match would skip it. The
        // pattern is globally unique, so scan the whole story instead.
        true,
    },

    {
        Game::ZorkZero,
        // The r242 beta variant. The release V-$REFRESH opens with
        // `JE P-CAN-UNDO,#02` (the pattern above), but r242 reorders the body
        // so the routine instead opens with its DONT-CLEAR handling:
        // `JZ DONT-CLEAR; JZ BORDER-ON; CALL_1N CLEAR-BORDER`
        // (01 a0 01 .. a0 .. .. 8f .. ..). Without this the encyclopedia's
        // parchment text-window background never gets torn down on refresh
        // (adjust_encyclopedia_text_window runs -- the encyclopedia pictures
        // 25/26-33 are the same numbers in r242 -- but V_REFRESH is its only
        // cleanup), so it would bleed into the main text window. This alt
        // pattern matches ONLY r242/beta (head 0x14f9c); every other revision
        // matches the release pattern above, and neither pattern matches both.
        // global_search for the usual cursor reasons.
        "V-REFRESH alt",
        { 0x01, 0xa0, 0x01, WILDCARD, 0xa0, WILDCARD, WILDCARD, 0x8f, WILDCARD, WILDCARD },
        1,
        0,
        false,
        V_REFRESH,
        true,
    },

    // Shared with Arthur and Shogun
    {
        Game::ZorkZero,
        "V-COLOR",
        { 0x06, 0x80, 0xa5, 0xcf, 0x2f},
        3,
        0,
        false,
        V_COLOR
    },

    {
        Game::ZorkZero,
        "after V-COLOR",
        {},
        0,
        0,
        false,
        after_V_COLOR
    },

    {
        Game::ZorkZero,
        "DEFAULT-COLORS",
        // Match the routine's two leading stores -- FG-COLOR <- DEFAULT-FG and
        // BG-COLOR <- DEFAULT-BG -- which is what find_zork0_globals() reads to
        // recover fg_global_idx/DEFAULT_FG/bg_global_idx/DEFAULT_BG. The old
        // signature instead keyed off the trailing SET_COLOUR/SET_WINDOW pair,
        // which only exists in r392/r393; r383 and r387 guard that set with a
        // JZ, so DEFAULT-COLORS was never located there and zg.DEFAULT_BG was
        // left at 0 (its only finder lives in this branch). That made
        // z0_update_colors() write the default background into global 0 and
        // never initialise the real DEFAULT-BG global, so those releases opened
        // with the wrong text-window background -- the text buffer was the
        // default grey while the pillar/border graphics stayed black -- until a
        // resize/autorestore repainted it.
        //
        // Match just the two stores (0x2d), wildcarding the four global-number
        // operands: in r383..r393 they are fixed (FG-COLOR=Gc2/0x2c,
        // DEFAULT-FG=0x5f) but they differ in the older r296/r343/r366 (where
        // the old hard-coded prefix matched nothing, so the Amiga demo opened
        // with black border graphics under the grey text). The first such store
        // pair after V-COLOR is the routine start in every revision, giving the
        // same address as before on r383..r393 and now locating it on r366 too.
        { 0x2d, WILDCARD, WILDCARD, 0x2d, WILDCARD, WILDCARD },
        0,
        0,
        false,
        DEFAULT_COLORS
    },

    {
        Game::ZorkZero,
        "V-CREDITS",
        { 0xf9, 0x07, 0x15, 0x53, 0x00, 0xa1, 0x01 },
        0,
        0,
        false,
        V_CREDITS
    },

    {
        Game::ZorkZero,
        "after V-CREDITS alt",
        { 0x40, 0x00, 0xb8, 0x00, 0x05},
        2,
        0,
        false,
        after_V_CREDITS
    },

    {
        Game::ZorkZero,
        "CENTER-1",
        // Anchor on CENTER-1's entry the same way CENTER-2/3 do: the
        // leading 0x05 is the routine's locals-count header byte, and
        // 0xa0 0x02 is its first instruction (jz BOLD). The +1 offset
        // lands found_at_address on that first instruction so the hook
        // fires at the routine entry (with the string arg already in
        // local 1) and can do_return cleanly. The previous pattern was
        // anchored on a print_addr that doesn't exist in CENTER-1, so it
        // matched nothing in this release and the hook never fired.
        { 0x05, 0xa0, 0x02, 0xc8, 0xe8, 0x7f },
        1,
        0,
        false,
        CENTER_1
    },

    {
        Game::ZorkZero,
        "CENTER-1 alt",
        { 0x00, 0x00, 0xbb, 0xb0, 0x00, 0x00},
        -7,
        0,
        false,
        CENTER_1
    },

    {
        Game::ZorkZero,
        "CENTER-2",
        { 0x06, 0xf0, 0x3f},
        1,
        0,
        false,
        CENTER_2
    },

    {
        Game::ZorkZero,
        "CENTER-3",
        { 0x08, 0xf0, 0x3f },
        1,
        0,
        false,
        CENTER_3
    },

    {
        Game::ZorkZero,
        "V-MODE",
        { 0x00, 0xed, 0x3f, 0xff, 0xff, 0xa0 },
        1,
        0,
        false,
        V_MODE,
        // The real V-MODE routine can precede the (global_search) V-REFRESH match
        // (it does in r343, r296, ...), so a forward-only search from the
        // V-REFRESH cursor would skip it. This pattern is globally unique in
        // every revision that has it (absent only in the r242 betas, which rely
        // on "V-MODE alt"), so scanning the whole story always lands on the
        // correct routine. It comes before "V-MODE alt", whose anchor lies
        // three bytes into this routine's first instruction in every revision
        // but r242: a hook there would never fire.
        true,
    },

    {
        Game::ZorkZero,
        "V-MODE alt",
        { 0xa0, WILDCARD, 0xc8, 0x0d, WILDCARD, 0x00, 0x8c, 0x00, 0x05 },
        -3,
        0,
        false,
        V_MODE,
        // This pattern is not globally unique -- besides the real V-MODE routine
        // it false-matches an unrelated word-parsing routine late in the file
        // (e.g. 0x33cd1 in r343). Because the real V-MODE sits *before* the
        // (global_search) V-REFRESH match, the forward cursor is already past it
        // by the time this row is reached, so a forward search would lock onto
        // the late false positive and skip ~40 later routines (status line,
        // mini-games, borders) -- which left INIT-STATUS-LINE un-stubbed and
        // crashed r343 with "invalid attribute: 63". Scan from the start so the
        // first (real, or harmlessly routine-adjacent) match wins instead.
        true,
    },

    {
        Game::ZorkZero,
        "V_REFRESH alt",
        { 0xa0, 0x01, 0xd1, 0xa0, WILDCARD, 0xc5, 0x8f, 0x34, 0x3a, 0xf9, 0x27, 0x34, 0x81, WILDCARD, 0x01 },
        0 ,
        0,
        false,
        V_REFRESH
    },

    {
        Game::ZorkZero,
        "V-MAP-LOOP",
        { 0x88, 0x1e, WILDCARD, 0x00, 0xa0, 0x00, 0xf3 },
        0,
        0,
        false,
        V_MAP_LOOP
    },

    {
        Game::ZorkZero,
        "V-MAP-LOOP alt",
        { 0xfa, 0x26, 0x9f, 0x15, 0xf7, 0x00, 0xcc, 0x06, 0x05, 0x07  },
        0,
        0,
        false,
        V_MAP_LOOP,
        false,
        kVMapLoopAlt,
    },

    // The r296/r66 betas (and the r366 demo) also inline the map loop into
    // V-MAP, but their BLINK-WHILE-AWAITING-INPUT call is a CALL_VS2 (it stores
    // the input result into a local) rather than the r242 "alt" CALL_VN2, with
    // the you-are-here CX/CY in vars 4/5 (the release layout) instead of 5/6:
    //   ec 26 9f <blink-rtn> 00 cc 05 04 07 06
    //   = CALL_VS2 BLINK ((SP)+,#cc,L04,L03,#07) -> L05
    // This exact call also appears in the releases, but there it lives in a
    // standalone V-MAP-LOOP routine claimed first by the "V-MAP-LOOP" row above;
    // find_entrypoints() skips the rows of a routine already claimed, which
    // keeps this global match from shadowing it. So this row only ever wins
    // on the inline-map betas, which match neither of the rows above.
    // global_search because, like the betas' other map routines, its position
    // relative to its neighbours varies.
    {
        Game::ZorkZero,
        "V-MAP-LOOP inline",
        { 0xec, 0x26, 0x9f, WILDCARD, WILDCARD, 0x00, 0xcc, 0x05, 0x04, 0x07, 0x06 },
        0,
        0,
        false,
        V_MAP_LOOP,
        true,
        kVMapLoopInline,
    },

    // MAP-X (and the near-identical MAP-Y right after it, derived in the
    // handler) sit at ~0x16xxx-0x17xxx, but in the pre-release revisions the
    // CENTER-n credit routines -- which precede this row in the table -- live
    // *after* the status block (r296: ~0x1d779), so the forward cursor overshoots
    // MAP-X and it was never found (no resize map-coordinate recompute -> the
    // marker stayed at its stale entry scale). Both patterns are globally unique
    // (only the two adjacent MAP-X/MAP-Y routines match, no false hits), so scan
    // the whole story. Same overshoot fix as INIT-STATUS-LINE/SET-BORDER below;
    // a release uses the non-alt JE branch (4c), the r242/r296/r66 betas the alt
    // one (4e da). Only one pattern matches per revision.
    {
        Game::ZorkZero,
        "MAP-X",
        { 0x43, 0x01, 0x00, 0x4c },
        -1,
        0,
        false,
        MAP_X,
        true,
    },

    {
        Game::ZorkZero,
        "MAP-X alt",
        { 0x43, 0x01, 0x00, 0x4e, 0xda },
        -1,
        0,
        false,
        MAP_X,
        true,
    },

    {
        Game::ZorkZero,
        "after SPLIT-BY-PICTURE",
        { 0x11, 0x6b, 0x01, 0x03, 0x00, 0xb0 },
        5,
        0,
        false,
        after_SPLIT_BY_PICTURE
    },

    {
        Game::ZorkZero,
        "INIT-STATUS-LINE",
        { 0x0d, 0x02, 0x01, 0x0d, WILDCARD, WILDCARD, 0x0d, WILDCARD, 00 },
        0,
        0,
        true,
        INIT_STATUS_LINE,
        // global_search: the pre-release revisions move the CENTER-n credit
        // routines *after* the status-line block in memory (r296 has them at
        // ~0x1d71d, past INIT-STATUS-LINE at 0x1cca0), but they come earlier in
        // this table and so advance the forward cursor past INIT-STATUS-LINE.
        // The pattern is globally unique in every revision, so scan the whole
        // story. (A behind-cursor global match doesn't rewind the cursor.)
        true,
    },

    {
        Game::ZorkZero,
        "SET-BORDER",
        { 0x41, WILDCARD, WILDCARD, 0x48, 0x43, WILDCARD, 0x00, 0x44  },
        -1,
        0,
        false,
        SET_BORDER,
        // global_search for the same reason as INIT-STATUS-LINE above: the
        // pattern is globally unique in every revision but the forward cursor
        // overshoots it in r296 (SET-BORDER is at 0x1cd8c, behind the cursor
        // after the CENTER-n routines).
        true,
    },

    {
        Game::ZorkZero,
        "DRAW-NEW-HERE",
        { 0x2d, WILDCARD, WILDCARD, 0xcf, 0x1f, WILDCARD, WILDCARD, 0x00, 0x02, 0xa0  },
        -1,
        0,
        false,
        DRAW_NEW_HERE
    },

    {
        Game::ZorkZero,
        "DRAW-NEW-COMP",
        // Anchor on the first compass-element CALL_VN (...,#3f,#09,#11), which is
        // stable across revisions, instead of the routine head. The old head
        // pattern { 0d 69 00 f9 15 .. .. 3f 09 11 } hardcoded COMPASS-CHANGED's
        // global (G59) and assumed the STORE was immediately followed by the
        // CALL_VN loop; r343/r366 number COMPASS-CHANGED differently and insert
        // an extra CALL_2N before the loop, so it matched only r383..r393. The
        // handler back-scans from here to the routine head (needed for
        // pack_routine, and to avoid the broken PICINF_TBL/DRAW-COMPASS-ROSE
        // fallback in z0_update_status_line on those releases).
        { 0xf9, 0x15, WILDCARD, WILDCARD, 0x3f, 0x09, 0x11 },
        0,
        0,
        false,
        DRAW_NEW_COMP
    },

    {
        Game::ZorkZero,
        "SETUP-SCREEN",
        { 0x10, 0x00, 0x1e, 0x3f, 0x41, 0x3f, 0x06, 0xc8},
        -1,
        0,
        false,
        SETUP_SCREEN
    },

    {
        Game::ZorkZero,
        // The 2-locals variant of UPDATE-STATUS-LINE, used by the pre-release
        // revisions r242 and r296 (the releases/r343/r66 use the 3-locals
        // pattern above, where the real routine is a low-address 3-locals one).
        // In r242 the routine sits at 0x1c3b4 and in r296 at 0x1cdc4 -- *before*
        // SET-BORDER/DRAW-NEW-COMP, which are earlier in this table but later in
        // memory -- so a forward-only search can never reach it. The pattern is
        // globally unique in those revisions, so scan the whole story. It is
        // guarded against double-matching in find_entrypoints(): once any
        // z0_UPDATE_STATUS_LINE row has matched, this row is skipped. (The
        // 3-locals row's own false-interior matches in r296 are rejected before
        // they can claim the routine -- see the 0xda check in find_entrypoints --
        // so the false 3-locals match never shadows this real 2-locals one.)
        "UPDATE-STATUS-LINE alt",
        { 0x02, 0xeb, 0x7f, 0x01, WILDCARD, 0x04, 0x7f, 0x04, 0x00 },
        1,
        0,
        true,
        z0_UPDATE_STATUS_LINE,
        true,
    },

    {
        Game::ZorkZero,
        "DRAW-COMPASS-ROSE",
        // Head shape: [N locals] CALL_VS SHOW-DIRECTION?(HERE, .DIR) -> ...;
        // JZ ... The old pattern hardcoded SHOW-DIRECTION?'s packed address
        // (35 6f) and the HERE-global slot, so it only matched the r242 beta.
        // Wildcard the packed callee (bytes 2-3) and the HERE global (byte 4):
        // { e0 29 .. .. .. 01 00 00 a0 } is globally unique in EVERY revision
        // and resolves the real DRAW-COMPASS-ROSE (r242 0x1c5f0, r296 0x1d088,
        // and the releases too -- harmless there since z0_UPDATE_STATUS_LINE
        // prefers DRAW-NEW-COMP and never reads the DRAW-COMPASS-ROSE fallback).
        // global_search because the forward cursor overshoots it in r296.
        { 0xe0, 0x29, WILDCARD, WILDCARD, WILDCARD, 0x01, 0x00, 0x00, 0xa0 },
        -1,
        0,
        false,
        DRAW_COMPASS_ROSE,
        true,
    },

    {
        Game::ZorkZero,
        "WINPROP",
        { 0xbe, 0x13, 0x8b, 0x01, 0x70, 0x8b, 0x02 },
        0,
        0,
        false,
        WINPROP
    },

    {
        Game::ZorkZero,
        "PEG-GAME",
        { 0xa0, 0x02, 0xe8, 0x56, 0x02, 0x02, 0x00 },
        0,
        0,
        false,
        PEG_GAME
    },

    // The Peggleboz routines below have "alt" patterns for the early
    // revisions, which differ from the later ones in three ways:
    //   - r296 and r66 call routines by constant (CALL_2N #xxxx = da 0f)
    //     where the later revisions call through a global (da 4f).
    //   - r242 and r296 keep the pegs in PEG-TABLE, a table of words (1 = peg
    //     present), rather than in attributes of NOT-HERE-OBJECT (set = peg
    //     gone), so SETUP-PBOZ lacks the leading <FSET ,NOT-HERE-OBJECT 7>
    //     and DRAW-PEGS / PBOZ-WIN-CHECK read the table.
    //   - r242 has no mouse support in Peggleboz: no PBOZ-CLICK.
    // Exactly one pattern per routine matches in any given revision, and
    // find_entrypoints() skips the rest once one has.

    {
        Game::ZorkZero,
        "PBOZ-CLICK",
        { 0x0d, 0x05, 0x02, 0xda, 0x4f, WILDCARD, 0x01, 0xd8 },
        0,
        0,
        false,
        PBOZ_CLICK
    },

    // r296, r66
    {
        Game::ZorkZero,
        "PBOZ-CLICK alt",
        { 0x0d, 0x05, 0x02, 0xda, 0x0f, WILDCARD, WILDCARD, 0x01, 0xd8 },
        0,
        0,
        false,
        PBOZ_CLICK
    },

    {
        Game::ZorkZero,
        "SETUP-PBOZ",
        // Bytes 6-7 were 0x01, 0x59, the object number of NOT-HERE-OBJECT in
        // r343 and later; it is 0x1fa in r66.
        { 0x0d, 0x01, 0x02, 0xcb, 0x1f, WILDCARD, WILDCARD, 0x07 },
        0,
        0,
        true,
        SETUP_PBOZ
    },

    // r296: STORE I,#02 ; SET_ATTR PBOZ-OBJECT,TOUCHBIT (byte constants) ;
    // STORE TT,PBOZ-PIC-TABLE ; LOADW TT,#00
    {
        Game::ZorkZero,
        "SETUP-PBOZ alt",
        { 0x0d, 0x01, 0x02, 0x0b, WILDCARD, WILDCARD, 0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x4f, 0x02, 0x00, 0x00 },
        0,
        0,
        true,
        SETUP_PBOZ
    },

    // r242 (SETUP-PEGGLEBOZ): STORE I,#02 ; TEST_ATTR PEGGLEBOZ-OBJECT,TOUCHBIT ;
    // SET_ATTR PEGGLEBOZ-OBJECT,TOUCHBIT ; STORE TT,PEGGLEBOZ-PIC-TABLE
    {
        Game::ZorkZero,
        "SETUP-PBOZ alt 2",
        { 0x0d, 0x01, 0x02, 0xca, 0x1f, WILDCARD, WILDCARD, WILDCARD, 0x80, WILDCARD, 0xcb, 0x1f, WILDCARD, WILDCARD, WILDCARD, 0xcd, 0x4f, 0x02 },
        0,
        0,
        true,
        SETUP_PBOZ
    },

    {
        Game::ZorkZero,
        "DRAW-PEGS",
        // Bytes 6-7 were 0x01, 0x59 (NOT-HERE-OBJECT); see SETUP-PBOZ.
        { 0x0d, 0x01, 0x01, 0xca, 0x2f, WILDCARD, WILDCARD, 0x01, 0xe3 },
        -1,
        0,
        false,
        DRAW_PEGS
    },

    // r242, r296: STORE NUM,#01 ; LOADW PEG-TABLE,NUM -> -(SP) ; JE (SP)+,#01
    {
        Game::ZorkZero,
        "DRAW-PEGS alt",
        { 0x0d, 0x01, 0x01, 0xcf, 0x2f, WILDCARD, WILDCARD, 0x01, 0x00, 0x41, 0x00, 0x01 },
        -1,
        0,
        false,
        DRAW_PEGS
    },

    {
        Game::ZorkZero,
        "PBOZ-WIN-CHECK",
        // Last byte was 0xcf, the branch offset of JE CNT,#16, which is 0xd3
        // in the revisions that count the pegs in PEG-TABLE (r242, r296).
        { 0x0d, 0x01, 0x01, 0xa0, WILDCARD, 0x40, 0x41, 0x01, 0x16, WILDCARD },
        0,
        0,
        false,
        PBOZ_WIN_CHECK
    },

    {
        Game::ZorkZero,
        "DISPLAY-MOVES",
        { 0xf3, 0x3f, 0xff, 0xff, 0xbb, 0xf3, 0x7f, 0x01, 0x55, WILDCARD, 0x02, 0x00},
        0,
        0,
        false,
        DISPLAY_MOVES
    },

    {
        Game::ZorkZero,
        "SETUP-FANUCCI",
        { 0x98, 0x40, 0x00, 0xa0, 0x00, 0xc8 },
        0,
        0,
        true,
        SETUP_FANUCCI
    },

    {
        Game::ZorkZero,
        "SETUP-FANUCCI alt",
        // r296 has no APPLE? test at the top of the routine: it starts with
        // SET_WINDOW #07; ERASE_WINDOW (7 locals, hence the leading 0x07).
        { 0x07, 0xeb, 0x7f, 0x07, 0xbe, 0x05, 0x57, 0x63, 0x01, 0x01, 0xeb, 0x7f, 0x01 },
        1,
        0,
        true,
        SETUP_FANUCCI
    },

    {
        Game::ZorkZero,
        "FANUCCI",
        // The short signature (CALL_2N <PICINF-PLUS-ONE> ,F-MENU-LOC ; LOADW)
        // is not unique: in r343 and the r366 demo the same bytes occur
        // several times before the real routine, so a first-match search
        // landed mid-code (corrupting it via stub_original and putting the
        // global search >300 bytes off target). Extend through the routine's
        // distinctive prologue -- two LOADWs from the F-MENU table (column
        // #01 then #00) followed by STORE PTR,#00 -- which is unique in every
        // revision that has this routine, and resolves to the very same
        // address as before in r383..r393.
        { 0xda, 0x4f, WILDCARD, 0x01, 0x80,
          0xcf, 0x1f, WILDCARD, WILDCARD, 0x01, 0x01,
          0xcf, 0x1f, WILDCARD, WILDCARD, 0x00, 0x02,
          0x0d, 0x03, 0x00 },
        0,
        0,
        true,
        FANUCCI
    },

    {
        Game::ZorkZero,
        "FANUCCI alt",
        // r296 and r66 call PICINF-PLUS-ONE with a 16-bit packed address
        // (da 0f) rather than through a global (da 4f).
        { 0xda, 0x0f, WILDCARD, WILDCARD, 0x01, 0x80,
          0xcf, 0x1f, WILDCARD, WILDCARD, 0x01, 0x01,
          0xcf, 0x1f, WILDCARD, WILDCARD, 0x00, 0x02,
          0x0d, 0x03, 0x00 },
        0,
        0,
        true,
        FANUCCI
    },

    {
        Game::ZorkZero,
        "SCORE_CHECK",
        { 0x63, WILDCARD, WILDCARD, 0x54, 0x75, WILDCARD, WILDCARD, 0x00, 0xc3, 0x8f, 0x00 },
        -1,
        0,
        false,
        SCORE_CHECK
    },

    {
        Game::ZorkZero,
        "J-PLAY",
        { 0xb2, 0x14, 0xe0, 0x00, 0x03, 0x62, 0xe0, 0x52 },
        -1,
        0,
        false,
        J_PLAY
    },

    {
        Game::ZorkZero,
        "J-PLAY alt",
        // The opening text is worded differently in r296 and r66
        { 0xb2, 0x14, 0xe0, 0x00, 0x03, 0x6a, 0x95, 0x56 },
        -1,
        0,
        false,
        J_PLAY
    },

    {
        Game::ZorkZero,
        "PLAY-SELECTED",
        { 0x41, 0x01, 0x0e, 0xc1, 0x41, 0x01, 0x0d },
        -1,
        0,
        false,
        PLAY_SELECTED
    },

    {
        Game::ZorkZero,
        "SMALL-DOOR-F",
        { 0xb2, 0x00, 0x00, 0x13, 0x3e, 0x55, 0x40, 0x04 },
        0,
        0,
        false,
        SMALL_DOOR_F
    },

    {
        Game::ZorkZero,
        "TOWER-MODE",
        // Was { 0xa0, 0xe1, 0x80, 0x70 }, which hardcoded TOWER-CHANGED's global
        // number (Gd1) and a branch offset, so it only matched r392/r393. The
        // routine always opens JZ <TOWER-CHANGED> [TRUE] ; JZ L04 [FALSE] ;
        // CALL_1S TOWER-WIN-CHECK -- anchor on that shape so it resolves to the
        // routine's first instruction across revisions (same address in
        // r392/r393; also fixes r343/r366/r383/r387, where TOWER-CHANGED is
        // Gca..Gd0). The TOWER_CHANGED extraction below reads it from there.
        { 0xa0, WILDCARD, WILDCARD, WILDCARD, 0xa0, 0x05, 0x00, WILDCARD, 0x88 },
        0,
        0,
        false,
        TOWER_MODE
    },

    {
        Game::ZorkZero,
        "TOWER-MODE alt",
        { 0xa0, 0x05, 0x00, 0x79  },
        0,
        0,
        false,
        TOWER_MODE
    },

    {
        Game::ZorkZero,
        "B-MOUSE-PEG-PICK",
        { 0xbe, 0x06, 0x4f, 0x2b },
        0,
        0,
        false,
        B_MOUSE_PEG_PICK
    },

    {
        Game::ZorkZero,
        "B-MOUSE-WEIGHT-PICK",
        { 0xda, 0x4f, WILDCARD, 0x01, 0xdb, 0xcf, 0x1f },
        0,
        0,
        false,
        B_MOUSE_WEIGHT_PICK
    },

    // r296, r66: PICINF-PLUS-ONE is called by constant rather than through a
    // global (see the Peggleboz "alt" rows above).
    {
        Game::ZorkZero,
        "B-MOUSE-WEIGHT-PICK alt",
        { 0xda, 0x0f, WILDCARD, WILDCARD, 0x01, 0xdb, 0xcf, 0x1f },
        0,
        0,
        false,
        B_MOUSE_WEIGHT_PICK
    },

    {
        Game::ZorkZero,
        "TOWER-WIN-CHECK",
        { 0xc6, 0x0f, 0x01, WILDCARD, 0x01, WILDCARD, WILDCARD,
            0xc6, 0x0f, 0x01, WILDCARD, 0x01, WILDCARD, WILDCARD,
            0xc6
        },
        -1,
        0,
        false,
        TOWER_WIN_CHECK
    },

    {
        Game::ZorkZero,
        "DRAW-TOWER",
        { 0xed, 0x7f, 0x07, 0xeb, 0x7f, 0x07, 0xbe, 0x05, 0x57 },
        0,
        0,
        true,
        DRAW_TOWER
    },

    {
        Game::ZorkZero,
        "SET-B-PIC",
        { 0xc1, 0x8f, 0x01, 0x01, WILDCARD, WILDCARD, 0x9b, 0x2b },
        -1,
        0,
        false,
        SET_B_PIC
    },

    {
        Game::ZorkZero,
        "SETUP-SN",
        { 0xeb, 0x7f, 0x07, WILDCARD, 0x05, 0x57, 0x49, 0x01, 0x01},
        // Was offset 43 (the routine's final RTRUE in r383..r393), but r343's
        // SETUP-SN is 41 bytes and ends in RET_POPPED, so +43 overshot onto the
        // next routine's header and the dispatch never fired. Stub at offset 0
        // instead: SETUP_SN() already does the full setup via draw_snarfem()
        // (which is reused on every redraw), so the original routine is
        // redundant. Revision-independent; resolves to the routine's first
        // instruction in every revision.
        0,
        0,
        true,
        SETUP_SN
    },

    {
        Game::ZorkZero,
        "DRAW-SN-BOXES",
        { 0x0d, 0x05, 0x01, 0xeb, 0x7f, 0x01, 0xda, 0x4f, WILDCARD, 0x01, 0xd6 },
        0,
        0,
        true,
        DRAW_SN_BOXES
    },

    // r296, r66: PICINF-PLUS-ONE called by constant, as above.
    {
        Game::ZorkZero,
        "DRAW-SN-BOXES alt",
        { 0x0d, 0x05, 0x01, 0xeb, 0x7f, 0x01, 0xda, 0x0f, WILDCARD, WILDCARD, 0x01, 0xd6 },
        0,
        0,
        true,
        DRAW_SN_BOXES
    },

    {
        Game::ZorkZero,
        "DRAW-PILE",
        // 6th byte was 0x73, the high byte of PILE_TABLE's address (#73f9 in
        // r383..r393); r343 has it at #726d, so wildcard the table address.
        { 0xeb, 0x7f, 0x01, 0xcf, 0x2f, WILDCARD, WILDCARD, 0x01, 0x02 },
        0,
        0,
        true,
        DRAW_PILE
    },

    {
        Game::ZorkZero,
        "DRAW-FLOWERS",
        // 11th byte was 0x73, again the high byte of PILE_TABLE (#726d in r343);
        // wildcard it so the CALL_2S ... ,PILE_TABLE matches across revisions.
        { 0x0d, 0x01, 0x01, 0x0d, 0x02, 0x01, 0xd9, 0x0f, WILDCARD, WILDCARD, WILDCARD, WILDCARD, 0x00, 0xa0 },
        0,
        0,
        true,
        DRAW_FLOWERS
    },

    {
        Game::ZorkZero,
        "SNARFEM",
        // Last byte was 0x75, the low byte of the JZ L02 branch offset, which
        // differs per revision (0x35 in r343); wildcard it.
        { 0xa0, 0x04, 0x41, 0xa0, 0x03, 0x81, WILDCARD },
        0,
        0,
        false,
        SNARFEM
    },

    {
        Game::ZorkZero,
        "SN-CLICK",
        // 9th byte was 0x6e, the high byte of the picinf table in PICTURE_DATA
        // #01bd,<table> (#6e3b in r383..r393); r343 uses #6cdb, so wildcard the
        // table address. The picture number (#01bd) is stable across revisions.
        { 0x0d, 0x08, 0x01, 0xbe, 0x06, 0x0f, 0x01, 0xbd, WILDCARD, WILDCARD, 0xc2 },
        0,
        0,
        false,
        SN_CLICK
    },

    // The end of the demo's SLIDE-SHOW (r66 and later), just after the
    // HIT-ANY-KEY that follows the version banner:
    //   NEW_LINE; CALL_1N HIT-ANY-KEY; INSERT_OBJ DIRIGIBLE,SMALLER-HANGAR;
    //   PUT_PROP GONDOLA,P?REGION,"Fenshire"
    // The hook sits on the INSERT_OBJ, so it runs once the key is pressed.
    // global_search because nothing else in the table is near SLIDE-SHOW.
    {
        Game::ZorkZero,
        "SLIDE-SHOW end",
        { 0xbb, 0x8f, WILDCARD, WILDCARD, 0xce, 0x1f, WILDCARD, WILDCARD, WILDCARD, 0xe3, 0x53, WILDCARD, 0x27 },
        4,
        0,
        false,
        SLIDE_SHOW_END,
        true,
    },

    // Shared with Arthur and Shogun
    {
        Game::ZorkZero,
        "DO-HINTS",
        { 0xf1, 0x7f, 0x00, 0xef, 0x1f, 0xff, 0xff, 0x00, 0x88 },
        0,
        0,
        false,
        DO_HINTS
    },

    {
        Game::ZorkZero,
        "DISPLAY-HINT",
        { 0xf1, 0x7f, 0x00, 0xed, 0x7f, 0x00, 0xeb },
        0,
        0,
        false,
        DISPLAY_HINT
    },
};

// Scan backward from a Zork Zero BLINK-WHILE-AWAITING-INPUT call site for the
// head of the V-MAP routine the call is inlined into, so z0_redraw_map() can
// re-invoke it. V-MAP either opens with the held-item GET_CHILD preamble
// (r66/r242/r343/releases: <hdr> A0 .. .. 82 .. .. .. C2) or goes straight to the
// map-mode guard (r296/r366: <hdr> A0 .. .. 8F .. .. 9B 02 = JZ MAP-MODE; CALL_1N
// ..; RET #02). Prefer the GET_CHILD head: the guard shape also occurs
// mid-routine in the revisions that have the preamble, so matching it first there
// would latch onto a false (interior) header. Returns 0 if no head is found.
static uint32_t find_zork0_v_map_head(uint32_t blink_call, uint32_t max_distance) {
    uint32_t lo = blink_call > max_distance ? blink_call - max_distance : 0;
    for (uint32_t a = blink_call; a > lo; a--) {
        if (memory[a] <= 15 && memory[a + 1] == 0xA0 &&
            memory[a + 4] == 0x82 && memory[a + 8] == 0xC2) {
            return a;
        }
    }
    for (uint32_t a = blink_call; a > lo; a--) {
        if (memory[a] <= 15 && memory[a + 1] == 0xA0 &&
            memory[a + 4] == 0x8F && memory[a + 7] == 0x9B && memory[a + 8] == 0x02) {
            return a;
        }
    }
    return 0;
}

// The Double Fanucci replacement (SETUP_FANUCCI / FANUCCI) plays the game
// through the story's own PLAY-SELECTED, J-PLAY and SCORE-CHECK routines and
// reads its card and play-name tables. If any of those could not be located
// (as in the r242 beta), restore the stubbed-out originals and leave the
// game to the Z-code.
void unhook_incomplete_fanucci(void) {
    if (zr.PLAY_SELECTED != 0 && zr.J_PLAY != 0 && zr.SCORE_CHECK != 0 &&
        zt.F_CARD_TABLE != 0 && zt.F_PLAY_TABLE != 0 && zt.F_PLAY_TABLE_APPLE != 0)
        return;
    for (auto &entrypoint : zorkzero_entrypoints) {
        if ((entrypoint.fn == FANUCCI || entrypoint.fn == SETUP_FANUCCI) && entrypoint.found_at_address != 0) {
            fprintf(stderr, "Double Fanucci support is incomplete: unhooking %s\n", entrypoint.title.c_str());
            // The stubbed byte is the one the pattern matched at `offset`,
            // unless that slot is a wildcard, which matches anything.
            if (entrypoint.stub_original && entrypoint.offset >= 0 && entrypoint.offset < (int)entrypoint.pattern.size() &&
                entrypoint.pattern[entrypoint.offset] != WILDCARD)
                store_byte(entrypoint.found_at_address, entrypoint.pattern[entrypoint.offset]);
            entrypoint.found_at_address = 0;
        }
    }
}

// DRAW-NEW-COMP: the routine itself, when there is one, and the status line location table.
static void z0_find_draw_new_comp(EntryPoint &entrypoint, int &start) {
    // found_at_address is the first compass-element CALL_VN; recover the
    // routine address (for pack_routine) by scanning back to its opening
    // STORE COMPASS-CHANGED,#00 (0d <g> 00). The byte before that STORE
    // is the locals-count header. The intervening prologue is just the
    // STORE (+ an optional CALL_2N on r343/r366), neither of which
    // contains a 0d ?? 00, so the first match going back is the head.
    uint32_t comp_call = entrypoint.found_at_address;
    uint32_t head = 0;
    for (uint32_t back = 3; back <= 16 && back < comp_call; back++) {
        uint32_t a = comp_call - back;
        if (memory[a] == 0x0d && memory[a + 2] == 0x00) {
            head = a - 1;
            break;
        }
    }
    // Validate that the candidate is a real routine header: a V6
    // routine begins with a locals-count byte (0..15). In the early
    // betas (r242) the compass loop lives *inside* UPDATE-STATUS-LINE
    // (routine 0x1c3b4) rather than in a dedicated DRAW-NEW-COMP, so
    // the back-scan can't reach the routine head (it's ~500 bytes
    // back) and instead latches onto a mid-routine STORE; the byte it
    // computes as the "header" is really a branch offset (0x43 in
    // r242 -> 67 locals, impossible). Reject such matches and leave
    // zr.DRAW_NEW_COMP = 0 so z0_UPDATE_STATUS_LINE falls back to
    // calling DRAW-COMPASS-ROSE directly -- the path those revisions
    // are designed to use (see zorkzero.cpp).
    if (head == 0 || memory[head] > 15) {
        zr.DRAW_NEW_COMP = 0;
        fprintf(stderr, "DRAW-NEW-COMP match at 0x%x has no standalone routine (header byte 0x%x); using DRAW-COMPASS-ROSE fallback\n",
                comp_call, head ? memory[head] : 0);
        entrypoint.found_at_address = 0;
    } else {
        zr.DRAW_NEW_COMP = head;
        entrypoint.found_at_address = head;
        fprintf(stderr, "zr.DRAW_NEW_COMP at address 0x%x\n", zr.DRAW_NEW_COMP);
        // DRAW-NEW-COMP reads the compass-element positions out of
        // SL-LOC-TBL; its first access is LOADW SL_LOC,#06 -> L01
        // (cf 1f <addr> 06 02), stable across r383..r393. We need the
        // table address so z0_refresh_sl_loc_tbl() can refill it on a
        // graphics-format switch (see zorkzero.cpp).
        start = find_16_bit_values_in_pattern({ 0xcf, 0x1f, WILDCARD, WILDCARD, 0x06, 0x02 }, { &zt.SL_LOC_TBL }, entrypoint.found_at_address, 200);
        if (start == -1)
            fprintf(stderr, "zt.SL_LOC_TBL not found!\n");
        else
            fprintf(stderr, "zt.SL_LOC_TBL at address 0x%x\n", zt.SL_LOC_TBL);
        entrypoint.found_at_address = 0;
    }
}

// TOWER-WIN-CHECK: the objects and tables of the Tower of Bozbar.
static void z0_find_tower_globals(EntryPoint &entrypoint, int &start) {
    zr.TOWER_WIN_CHECK = entrypoint.found_at_address;
    std::vector<uint16_t *> weights = {&zo.ONE_WEIGHT, &zo.TWO_WEIGHT, &zo.THREE_WEIGHT, &zo.FOUR_WEIGHT, &zo.FIVE_WEIGHT, &zo.SIX_WEIGHT};

    start = entrypoint.found_at_address;
    int32_t oldstart;

    for (auto weight : weights) {
        oldstart = start;
        start = find_16_bit_values_in_pattern({0xc6, 0x0f, WILDCARD, WILDCARD, WILDCARD, WILDCARD, WILDCARD },
                                              { weight, &zo.LEFT_PEG,  &zo.LEFT_PEG, &zo.RIGHT_PEG,  &zo.RIGHT_PEG }, start - 4, 18);
        if (start == -1) {
            start = find_16_bit_values_in_pattern({0xc6, WILDCARD, WILDCARD, WILDCARD, WILDCARD, WILDCARD },
                                                  { weight, &zo.LEFT_PEG,  &zo.LEFT_PEG, &zo.RIGHT_PEG,  &zo.RIGHT_PEG }, oldstart - 4, 20);
            *weight &= 0xff;
        }

        if (start == -1) {
            fprintf(stderr, "ERROR! Weight object not found!\n");
            break;
        }
    }

    uint8_t one_high = zo.ONE_WEIGHT >> 8;
    uint8_t one_lo = zo.ONE_WEIGHT & 0xff;
    oldstart = start;
    start = find_16_bit_values_in_pattern({0xc6, 0x0f, one_high, one_lo, WILDCARD, WILDCARD },
                                          { &zo.RIGHT_PEG }, start, 50);

    if (start == -1) {
        fprintf(stderr, "Error!\n");
        start = oldstart;
    } else {
        zo.PYRAMID_L = memory[start - 5];
        start = find_16_bit_values_in_pattern({0xc6, 0x0f, one_high, one_lo, WILDCARD, WILDCARD },
                                              { &zo.CENTER_PEG }, start, 50);
        if (start != -1)
            zo.PYRAMID_R = memory[start - 5];
        fprintf(stderr, "zo.LEFT_PEG = 0x%x zo.CENTER_PEG = 0x%x zo.RIGHT_PEG = 0x%x\n", zo.LEFT_PEG, zo.CENTER_PEG, zo.RIGHT_PEG);
    }

    uint8_t dummy = 0;
    oldstart = start;
    start = find_values_in_pattern({0x40, 0x9b, WILDCARD }, {&dummy}, start, 50);
    if (start == -1) {
        fprintf(stderr, "zo.PYRAMID not found!\n");
        start = oldstart;
    } else {
        zo.PYRAMID = dummy;
    }

    start = find_16_bit_values_in_pattern({0xc1, 0x8f, 0x01, 0x01, WILDCARD, WILDCARD, 0x8b, WILDCARD, WILDCARD, 0xc1, 0x8f, 0x01, WILDCARD, WILDCARD, WILDCARD, 0x8b, WILDCARD, WILDCARD, 0x8b, WILDCARD, WILDCARD, 0 }, {&zt.LEFT_PEG_TABLE, &zt.LEFT_PEG_TABLE, &zt.CENTER_PEG_TABLE, &zt.CENTER_PEG_TABLE, &zt.RIGHT_PEG_TABLE, &zt.CENTER_PEG_TABLE}, start, 100 );

    if (start != -1) {
        fprintf(stderr, "zo.LEFT_PEG_TABLE = 0x%x zt.CENTER_PEG_TABLE = 0x%x zt.RIGHT_PEG_TABLE = 0x%x\n", zt.LEFT_PEG_TABLE, zt.CENTER_PEG_TABLE, zt.RIGHT_PEG_TABLE);
    } else {
        fprintf(stderr, "zo.LEFT_PEG_TABLE, zt.CENTER_PEG_TABLE, zt.RIGHT_PEG_TABLE not found!\n");
    }
    entrypoint.found_at_address = 0;
}

// UPDATE-STATUS-LINE: the globals the status line is drawn from.
static void z0_find_status_line_globals(EntryPoint &entrypoint, int &start) {
    start = find_globals_in_pattern({ 0xdb, 0x8f, WILDCARD, 0xff, 0xff, 0x61, WILDCARD }, { &zg.DEFAULT_FG, &zg.HERE }, entrypoint.found_at_address, 300);
    if (start == -1) {
        start = find_globals_in_pattern({ 0xdb, 0x4f, 0x01, 0xff, 0xff, 0x61, WILDCARD }, { &zg.HERE }, entrypoint.found_at_address, 300);
    }
    if (start != -1) {
        fprintf(stderr, "zg.DEFAULT_FG = 0x%x zg.HERE = 0x%x\n", zg.DEFAULT_FG, zg.HERE);

        start = find_values_in_pattern({ 0x51, WILDCARD, WILDCARD, 0x00, 0x61, 0x00}, { &zp.P_REGION, &zp.P_REGION }, start, 200);

        int32_t score_start = find_globals_in_pattern({ 0xef, 0xaf, 0x02, WILDCARD, 0xe6, 0xbf, WILDCARD, 0x61, WILDCARD }, { &zg.MOVES, &zg.MOVES, &zg.SCORE }, start, 300);
        if (score_start == -1) {
            // r66 (pre-release) builds the score/moves line with the Y
            // coordinate in a different local, so SET_CURSOR encodes as
            // ef af 03 .. instead of ef af 02 .., and the P_REGION
            // anchor that seeds `start` above doesn't match either. The
            // PRINT_NUM <MOVES> / JE <SCORE> shape is otherwise the
            // same, so scan the whole UPDATE-STATUS-LINE routine for it.
            score_start = find_globals_in_pattern({ 0xef, 0xaf, 0x03, WILDCARD, 0xe6, 0xbf, WILDCARD, 0x61, WILDCARD }, { &zg.MOVES, &zg.MOVES, &zg.SCORE }, entrypoint.found_at_address, 0x400);
        }
        if (score_start == -1) {
            fprintf(stderr, "Could not find zg.SCORE!\n");
        } else {
            fprintf(stderr, "zg.MOVES = 0x%x zg.SCORE = 0x%x\n", zg.MOVES, zg.SCORE);
        }

        // BORDER-ON gates the entire border + compass draw (border_on
        // in zorkzero.cpp), so it must be located independently of
        // SCORE. The pre-release r66 layout doesn't match the
        // MOVES/SCORE idiom above; nesting the BORDER-ON search under
        // SCORE's success therefore left zg.BORDER_ON == 0, so border_on
        // read global 0, came out false, and r66 drew no pillars or
        // compass at all. r66's BORDER-ON read (a0 af 45 f1 7f 00) does
        // match the pattern -- it was just never searched for. When
        // SCORE was found keep the original forward scan from there;
        // otherwise scan the whole UPDATE-STATUS-LINE routine.
        if (score_start != -1) {
            start = find_globals_in_pattern({ 0xa0, WILDCARD, 0x45, 0xf1, 0x7f, 0x00 }, { &zg.BORDER_ON }, score_start, 200);
        } else {
            start = find_globals_in_pattern({ 0xa0, WILDCARD, 0x45, 0xf1, 0x7f, 0x00 }, { &zg.BORDER_ON }, entrypoint.found_at_address, 0x400);
        }
        if (start == -1) {
            fprintf(stderr, "Could not find zg.BORDER_ON!\n");
        } else {
            fprintf(stderr, "zg.BORDER_ON = 0x%x\n", zg.BORDER_ON);
        }

        // The pre-release revisions special-case the status-line room
        // names *inline* in UPDATE-STATUS-LINE (the releases use a
        // separate DRAW-NEW-HERE routine, wired up in its own handler;
        // its pattern doesn't match here). After the room-name
        // SET_CURSOR the routine runs a short series of `JE HERE,#room`
        // tests printing an abbreviated label ("Great Undergd. Mountain"
        // etc.) instead of the longer object DESC ("Great Underground
        // Mountain"). Anchor on `SET_CURSOR L01,(SP)+; JE HERE,#room`
        // (ef af 02 00 41 <here> .. 54) and collect the run: the last
        // three are always the Great Underground Mountain/Savannah/
        // Highway trio, and a fourth-from-start (r296+, not r242) is
        // Philharmonic Hall. draw_new_here() only reads these to pick the
        // abbreviation, so the releases leaving them to DRAW-NEW-HERE --
        // and r242 having no PHIL-HALL -- are both fine. The anchor does
        // not occur in the release UPDATE-STATUS-LINE routines, so this
        // is inert there.
        if (zg.HERE != 0 && zo.G_U_MOUNTAIN == 0) {
            uint8_t here_op = (uint8_t)(zg.HERE + kFirstGlobal);
            int32_t anchor = find_pattern_in_mem({ 0xef, 0xaf, 0x02, 0x00, 0x41, here_op, WILDCARD, 0x54 }, entrypoint.found_at_address, 0x300);
            if (anchor != -1) {
                uint8_t rooms[4];
                int nrooms = 0;
                for (uint32_t i = anchor + 4; i < anchor + 4 + 0x50 && nrooms < 4; i++) {
                    if (memory[i] == 0x41 && memory[i + 1] == here_op && memory[i + 3] == 0x54) {
                        rooms[nrooms++] = memory[i + 2];
                        i += 3;
                    }
                }
                if (nrooms >= 3) {
                    zo.G_U_HIGHWAY  = rooms[nrooms - 1];
                    zo.G_U_SAVANNAH = rooms[nrooms - 2];
                    zo.G_U_MOUNTAIN = rooms[nrooms - 3];
                    if (nrooms >= 4)
                        zo.PHIL_HALL = rooms[nrooms - 4];
                    fprintf(stderr, "zo.PHIL_HALL = 0x%x zo.G_U_MOUNTAIN = 0x%x zo.G_U_SAVANNAH = 0x%x zo.G_U_HIGHWAY = 0x%x (inline)\n",
                            zo.PHIL_HALL, zo.G_U_MOUNTAIN, zo.G_U_SAVANNAH, zo.G_U_HIGHWAY);
                }
            }
        }
    } else {
        fprintf(stderr, "Could not find zg.DEFAULT_FG!\n");
        fprintf(stderr, "Could not find zg.HERE!\n");
    }
}

// SETUP-PBOZ: the globals, objects and tables of Peggleboz.
static void z0_find_peggleboz_globals(EntryPoint &entrypoint, int &start) {
    // After STORE I,#02 the routine sets its attributes, which is
    // where the revisions differ (see the three SETUP-PBOZ rows).
    const uint32_t a = entrypoint.found_at_address + 3;
    if (memory[a] == 0xcb && memory[a + 5] == 0xcb) {
        // r343 on: SET_ATTR NOT-HERE-OBJECT,#07 ; SET_ATTR PBOZ-OBJECT,TOUCHBIT
        zo.NOT_HERE_OBJECT = word(a + 2);
        zo.PBOZ_OBJECT = word(a + 7);
        zp.TOUCHBIT = memory[a + 9];
    } else if (memory[a] == 0xcb && memory[a + 5] == 0x0b) {
        // r66: the same, with PBOZ-OBJECT as a byte constant
        zo.NOT_HERE_OBJECT = word(a + 2);
        zo.PBOZ_OBJECT = memory[a + 6];
        zp.TOUCHBIT = memory[a + 7];
    } else if (memory[a] == 0x0b) {
        // r296: SET_ATTR PBOZ-OBJECT,TOUCHBIT only (the pegs are in
        // PEG-TABLE, so NOT-HERE-OBJECT plays no part)
        zo.PBOZ_OBJECT = memory[a + 1];
        zp.TOUCHBIT = memory[a + 2];
    } else if (memory[a] == 0xca) {
        // r242: TEST_ATTR PEGGLEBOZ-OBJECT,TOUCHBIT ; SET_ATTR likewise
        zo.PBOZ_OBJECT = word(a + 2);
        zp.TOUCHBIT = memory[a + 4];
    }
    fprintf(stderr, "zo.NOT_HERE_OBJECT: 0x%x zo.PBOZ_OBJECT: 0x%x zp.TOUCHBIT: 0x%x\n", zo.NOT_HERE_OBJECT, zo.PBOZ_OBJECT, zp.TOUCHBIT);

    start = find_16_bit_values_in_pattern({0xcd, 0x4f, 0x02, WILDCARD, WILDCARD, 0x4f  }, {&zt.PBOZ_PIC_TABLE }, entrypoint.found_at_address, 40);
    if (start != -1) {
        fprintf(stderr, "zt.PBOZ_PIC_TABLE: 0x%x\n", zt.PBOZ_PIC_TABLE);
    } else {
        fprintf(stderr, "zt.PBOZ_PIC_TABLE not found!\n");
    }
    start = find_16_bit_values_in_pattern({0xe1, 0x2b, WILDCARD, WILDCARD, 0x01, 0x00}, {&zt.BOARD_TABLE }, entrypoint.found_at_address, 80);
    if (start != -1) {
        fprintf(stderr, "zt.BOARD_TABLE: 0x%x\n", zt.BOARD_TABLE);
    } else {
        fprintf(stderr, "zt.BOARD_TABLE not found!\n");
    }

}

// V-MAP-LOOP: where the map loop waits for input, and what it needs to be redrawn.
static void z0_find_map_globals(EntryPoint &entrypoint, int &start) {
    // Record which local slots hold the you-are-here CX/CY in the frame
    // our V_MAP_LOOP hook runs in. On the releases V-MAP-LOOP is its own
    // routine with CX in var 4 and CY in var 5. On the r242/r296 betas the
    // loop is inlined into V-MAP (locals TBL MAP-NUM RM RM-ICON CX CY), so
    // CX is var 5 and CY is var 6 -- the "alt" BLINK-WHILE-AWAITING-INPUT
    // call we anchor on reads X from var 5, Y from var 6 (operand bytes
    // ...cc 06 05 07). Storing into the wrong slots leaves the marker's X
    // holding the Y coordinate, drawing it too far left.
    if (entrypoint.variant == kVMapLoopAlt) {
        zr.MAP_CX_VAR = 5;
        zr.MAP_CY_VAR = 6;

        // The "alt" anchor is the BLINK-WHILE-AWAITING-INPUT call inside
        // the betas' monolithic V-MAP. Record it (to patch to rtrue) and
        // scan back for V-MAP's start so z0_redraw_map() can re-invoke the
        // game's own map drawing on resize/autorestore. V-MAP opens with
        // "<n locals> JZ DEBUG[..] GET_CHILD ROOMS -> L2 [..]", i.e.
        // (hdr<=15) A0 .. .. 82 .. .. 03 C2 -- distinctive enough to find
        // the header by scanning back a few hundred bytes.
        zr.V_MAP_BLINK_CALL = entrypoint.found_at_address;
        zr.V_MAP = find_zork0_v_map_head(entrypoint.found_at_address, 0x400);
        fprintf(stderr, "zr.V_MAP: 0x%x (BLINK call 0x%x)\n", zr.V_MAP, zr.V_MAP_BLINK_CALL);
    } else if (entrypoint.variant == kVMapLoopInline) {
        // r296/r66/r366 inline the loop into V-MAP too, but unlike r242
        // their CALL_VS2 reads X from var 4 and Y from var 5 (operand
        // bytes ...cc 05 04 07), the same as the release V-MAP-LOOP.
        zr.MAP_CX_VAR = 4;
        zr.MAP_CY_VAR = 5;

        // The anchor IS the BLINK-WHILE-AWAITING-INPUT call. Record it so
        // z0_redraw_map() can patch it to rtrue, and scan back for V-MAP's
        // start so it can re-invoke the game's own map drawing on
        // resize/autorestore (these revisions have no map-aware V-$REFRESH
        // and no BLINK-TBL, so without this the empty pixmap is never
        // repainted and the map shows as black).
        zr.V_MAP_BLINK_CALL = entrypoint.found_at_address;
        zr.V_MAP = find_zork0_v_map_head(entrypoint.found_at_address, 0x120);
        fprintf(stderr, "zr.V_MAP: 0x%x (BLINK call 0x%x)\n", zr.V_MAP, zr.V_MAP_BLINK_CALL);
    } else {
        // "V-MAP-LOOP" (non-alt): the anchor is the post-blink
        // `CALL_1S <handle-click>; JZ` (88 1e .. 00 a0 00 f3), not the
        // blink call itself. CX/CY are in vars 4/5 as for the inline case.
        zr.MAP_CX_VAR = 4;
        zr.MAP_CY_VAR = 5;

        // The release-style revisions (BLINK-TBL present) redraw the map
        // on resize via V-$REFRESH and never use zr.V_MAP, but the
        // beta-style r343 (frame-local marker, BLINK-TBL absent) also
        // matches this row and DOES need z0_redraw_map(), so resolve
        // zr.V_MAP/V_MAP_BLINK_CALL here too. Back up from the anchor to
        // the CALL_VS2 blink call (ec 26 9f .. .. 00 cc 05 04 07 06) that
        // precedes it within ~0x20 bytes, then scan back for V-MAP's head.
        uint32_t a = entrypoint.found_at_address;
        uint32_t lo = a > 0x20 ? a - 0x20 : 0;
        for (; a > lo; a--) {
            if (memory[a] == 0xec && memory[a + 1] == 0x26 && memory[a + 2] == 0x9f &&
                memory[a + 5] == 0x00 && memory[a + 6] == 0xcc && memory[a + 7] == 0x05 &&
                memory[a + 8] == 0x04 && memory[a + 9] == 0x07 && memory[a + 10] == 0x06) {
                zr.V_MAP_BLINK_CALL = a;
                zr.V_MAP = find_zork0_v_map_head(a, 0x120);
                break;
            }
        }
        fprintf(stderr, "zr.V_MAP: 0x%x (BLINK call 0x%x)\n", zr.V_MAP, zr.V_MAP_BLINK_CALL);
    }
    // P-MAP-LOC is the room property holding the map-coordinate table:
    // `GETP HERE,P?MAP-LOC >TBL` == GET_PROP HERE,#prop -> L00 then
    // LOADW L00 (51 .. .. 01 4f). On the releases the V-MAP-LOOP anchor
    // sits at/just before this; on the r242 "alt" the anchor is the
    // BLINK-WHILE-AWAITING-INPUT call at the END of the loop, ~0x28 bytes
    // *after* the GET_PROP, so the forward search misses it. Fall back to
    // a short backward scan. Without P-MAP-LOC, V_MAP_LOOP does
    // internal_get_prop(HERE, 0) -> "invalid property: 0" abort on MAP.
    start = find_values_in_pattern({0x51, WILDCARD, WILDCARD, 0x01, 0x4f}, {&zp.P_MAP_LOC, &zp.P_MAP_LOC}, entrypoint.found_at_address, 50);
    if (start == -1) {
        uint32_t back = entrypoint.found_at_address > 0x40 ? entrypoint.found_at_address - 0x40 : 0;
        start = find_values_in_pattern({0x51, WILDCARD, WILDCARD, 0x01, 0x4f}, {&zp.P_MAP_LOC, &zp.P_MAP_LOC}, back, 0x40);
    }
    if (start != -1) {
        fprintf(stderr, "zp.P_MAP_LOC: 0x%x\n", zp.P_MAP_LOC);
    } else {
        fprintf(stderr, "Could not find zp.P_MAP_LOC!\n");
    }
}

// PEG_MOVE_NUMBER fallback. The DISPLAY-MOVES entrypoint anchors on an
// OUTPUT_STREAM hook used by the scrollable move-list in r383..r393, which
// does not exist in earlier revisions (r343, r366), so PEG_MOVE_NUMBER was
// left unresolved (== 0) there -- and SETUP_PBOZ() would then write global 0.
// The routine's move-coordinate math (SET_FONT #04; MOD/DIV/ADD ...) is
// identical across revisions and sits a fixed 24 bytes after the
// ED 7F 00 / A0 <PEG-MOVE-NUMBER> 54 prologue, so anchor on it and read the
// global out of the prologue. r383..r393 already found it via the hook, so
// this only runs on the revisions that need it.
static void z0_find_peg_move_number_fallback(void) {
    if (zg.PEG_MOVE_NUMBER == 0) {
        int32_t m = find_pattern_in_mem({ 0xbe, 0x04, 0x7f, 0x04, 0x00, 0x58, 0x03, 0x0a, 0x00, 0x57, 0x00, 0x02, 0x00, 0x54, 0x00, 0x01, 0x02, 0x57, 0x03, 0x0a, 0x00, 0x56, 0x00, 0x0c, 0x00, 0x54, 0x00, 0x01, 0x01 },
                                         header.static_start, memory_size - header.static_start);
        if (m != -1 && find_globals_in_pattern({ 0xed, 0x7f, 0x00, 0xa0, WILDCARD, 0x54 }, { &zg.PEG_MOVE_NUMBER }, m - 24, 8) != -1) {
            fprintf(stderr, "zg.PEG_MOVE_NUMBER (fallback) is global 0x%x\n", zg.PEG_MOVE_NUMBER);
        } else {
            fprintf(stderr, "zg.PEG_MOVE_NUMBER fallback not found!\n");
        }
    }
}

// FG-COLOR/BG-COLOR fallback for revisions with no detectable V-COLOR
// routine (r66/r296/r343). Without it fg_global_idx/bg_global_idx stay 0,
// so z0_update_colors() reads/writes global 0 and z0_update_after_autorestore
// restores garbage colours (the reported r343 "cyan on yellow on reopen").
// The COLOR command's "<fg> text on a <bg> background" confirmation loads
// both colour-name strings from one table indexed by FG-COLOR then BG-COLOR:
//   cf 2f <tbl-hi> <tbl-lo> <FG> 00  ...  cf 2f <tbl-hi> <tbl-lo> <BG> 00
// The first such pair (same table, two *distinct global* indices) pins both
// globals directly. Verified against the recurring SET_COLOUR <FG>,<BG> in
// r66/r296/r343 (and it reproduces the V-COLOR-derived values on r366..r393,
// though those already detect them so this never runs there). The r242 betas
// have no such message and only yield fg==bg, which the distinct-index guard
// rejects, leaving them as before.
static void z0_find_color_globals_fallback(void) {
    if (fg_global_idx == 0) {
        int32_t p = header.static_start;
        while ((p = find_pattern_in_mem({ 0xcf, 0x2f, WILDCARD, WILDCARD, WILDCARD, 0x00 },
                                        p, memory_size - p)) != -1) {
            uint8_t tbl_hi = memory[p + 2], tbl_lo = memory[p + 3], fg = memory[p + 4];
            int32_t q = find_pattern_in_mem({ 0xcf, 0x2f, tbl_hi, tbl_lo, WILDCARD, 0x00 }, p + 6, 40);
            if (q != -1 && fg >= kFirstGlobal && memory[q + 4] >= kFirstGlobal && fg != memory[q + 4]) {
                fg_global_idx = global_index(fg);
                bg_global_idx = global_index(memory[q + 4]);
                fprintf(stderr, "fg/bg fallback: fg_global_idx = 0x%x bg_global_idx = 0x%x\n",
                        fg_global_idx, bg_global_idx);
                break;
            }
            p += 1;
        }
    }
}

// Second-pass scanner for Zork Zero. Locates routine addresses for the
// minigames (Tower of Bozbar, Peggleboz, Snarfem, Double Fanucci) and
// the map/compass system, plus their backing tables and the BLINK_TBL
// used by the map cursor.
void find_zork0_globals(void) {
    int start = 0;
    for (auto &entrypoint : zorkzero_entrypoints) {
        if (entrypoint.fn == V_COLOR && entrypoint.found_at_address != 0) {
            start = find_v_color_globals(entrypoint.found_at_address);
            entrypoint.found_at_address = 0; // V_COLOR
        } else if (entrypoint.fn == SET_BORDER && entrypoint.found_at_address != 0) {
            zr.SET_BORDER = entrypoint.found_at_address;
            fprintf(stderr, "zr.SET_BORDER at address 0x%x\n", entrypoint.found_at_address);
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == DRAW_NEW_COMP && entrypoint.found_at_address != 0) {
            z0_find_draw_new_comp(entrypoint, start);
        } else if (entrypoint.fn == SETUP_SCREEN && entrypoint.found_at_address != 0) {
            zr.SETUP_SCREEN = entrypoint.found_at_address;
            fprintf(stderr, "zr.SETUP_SCREEN at address 0x%x\n", entrypoint.found_at_address);
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == MAP_X && entrypoint.found_at_address != 0) {
            zr.MAP_X = entrypoint.found_at_address;
            int32_t map_y = find_pattern_in_mem({0x43, 0x01, 0x00}, zr.MAP_X + 0x1a, 300);
            if (map_y != -1) // otherwise MAP_Y would become a garbage routine address
                zr.MAP_Y = map_y - 1;
            fprintf(stderr, "zr.MAP_X at address 0x%x zr.MAP_Y at address 0x%x\n", zr.MAP_X, zr.MAP_Y);
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == PLAY_SELECTED && entrypoint.found_at_address != 0) {
            zr.PLAY_SELECTED = entrypoint.found_at_address;
            fprintf(stderr, "zr.PLAY_SELECTED at address 0x%x\n", entrypoint.found_at_address);
            // <COND (<APPLE?> <GET ,F-PLAY-TABLE-APPLE .PTR>) (T <GET ,F-PLAY-TABLE .PTR>)>:
            // the abbreviated Apple II table comes first.
            start = find_16_bit_values_in_pattern({0xcf, 0x2f, WILDCARD, WILDCARD, 0x03, 0x00, 0x8c, 0x00, 0x08, 0xcf, 0x2f, WILDCARD, WILDCARD, 0x03, 0x00, 0xad, 0x00}, {&zt.F_PLAY_TABLE_APPLE, &zt.F_PLAY_TABLE}, entrypoint.found_at_address - 24, 20);
            if (start == -1) {
                // r296 has a single table of play names, no Apple II variant
                start = find_16_bit_values_in_pattern({0xcf, 0x2f, WILDCARD, WILDCARD, 0x03, 0x00, 0xad, 0x00}, {&zt.F_PLAY_TABLE}, entrypoint.found_at_address - 24, 20);
                if (start != -1)
                    zt.F_PLAY_TABLE_APPLE = zt.F_PLAY_TABLE;
            }
            if (start != -1) {
                fprintf(stderr, "zt.F_PLAY_TABLE: 0x%x zt.F_PLAY_TABLE_APPLE 0x%x\n", zt.F_PLAY_TABLE, zt.F_PLAY_TABLE_APPLE);
            } else {
                fprintf(stderr, "zt.F_PLAY_TABLE not found!\n");
            }
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == SCORE_CHECK && entrypoint.found_at_address != 0) {
            zr.SCORE_CHECK = entrypoint.found_at_address;
            fprintf(stderr, "zr.SCORE_CHECK at address 0x%x\n", entrypoint.found_at_address);
            // The routine opens with <G? ,J-SCORE ,YOUR-SCORE>
            zg.J_SCORE = global_index(memory[entrypoint.found_at_address + 2]);
            zg.YOUR_SCORE = global_index(memory[entrypoint.found_at_address + 3]);
            fprintf(stderr, "zg.J_SCORE: 0x%x zg.YOUR_SCORE: 0x%x\n", zg.J_SCORE, zg.YOUR_SCORE);
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == J_PLAY && entrypoint.found_at_address != 0) {
            zr.J_PLAY = entrypoint.found_at_address;
            fprintf(stderr, "zr.J_PLAY at address 0x%x\n", entrypoint.found_at_address);
            start = find_16_bit_values_in_pattern({0xe2, 0x1b, WILDCARD, WILDCARD, 0x00, 0x02, 0xe2, 0x1b, WILDCARD, WILDCARD, 0x01, 0x03 }, {&zt.F_CARD_TABLE, &zt.F_CARD_TABLE}, entrypoint.found_at_address, 500);
            if (start == -1) {
                // r296 stores the cards as words (STOREW rather than STOREB),
                // and flags the slot for DRAW-CARDS in DRAW-CARDS-TABLE first.
                start = find_16_bit_values_in_pattern({0xe1, 0x17, WILDCARD, WILDCARD, 0x00, 0x01, 0xe1, 0x1b, WILDCARD, WILDCARD, 0x00, 0x02, 0xe1, 0x1b, WILDCARD, WILDCARD, 0x01, 0x03 }, {&zt.DRAW_CARDS_TABLE, &zt.F_CARD_TABLE, &zt.F_CARD_TABLE}, entrypoint.found_at_address, 500);
            }
            if (start == -1) {
                fprintf(stderr, "zt.F_CARD_TABLE not found!\n");
            } else {
                fprintf(stderr, "zt.F_CARD_TABLE: 0x%x zt.DRAW_CARDS_TABLE: 0x%x\n", zt.F_CARD_TABLE, zt.DRAW_CARDS_TABLE);
            }
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == DRAW_PEGS && entrypoint.found_at_address != 0) {
            zr.DRAW_PEGS = entrypoint.found_at_address;
            fprintf(stderr, "zr.DRAW_PEGS at address 0x%x\n", entrypoint.found_at_address);
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == DISPLAY_MOVES && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0xED, 0x7F, 0x00, 0xA0, WILDCARD, 0x54}, { &zg.PEG_MOVE_NUMBER }, entrypoint.found_at_address - 110, 10);
            if (start != -1) {
                fprintf(stderr, "zg.PEG_MOVE_NUMBER is global 0x%x\n", zg.PEG_MOVE_NUMBER);
            } else {
                fprintf(stderr, "zg.PEG_MOVE_NUMBER not found!\n");
            }
        } else if (entrypoint.fn == SET_B_PIC && entrypoint.found_at_address != 0) {
            zr.SET_B_PIC = entrypoint.found_at_address;
            fprintf(stderr, "zr.SET_B_PIC at address 0x%x\n", entrypoint.found_at_address);
            start = find_16_bit_values_in_pattern({0x05, 0xcf, 0x2f, WILDCARD, WILDCARD, 0x02, 0x02}, {&zt.B_X_TBL}, entrypoint.found_at_address - 50, 50);
            if (start == -1) {
                fprintf(stderr, "zt.B_X_TBL not found!\n");
                start = entrypoint.found_at_address;
            }
            start = find_16_bit_values_in_pattern({0x05, 0xcf, 0x2f, WILDCARD, WILDCARD, 0x04, 0x00}, {&zt.B_Y_TBL}, start, 50);
            if (start == -1) {
                fprintf(stderr, "zt.B_Y_TBL not found!\n");
            }
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == TOWER_WIN_CHECK && entrypoint.found_at_address != 0) {
            z0_find_tower_globals(entrypoint, start);
        } else if (entrypoint.fn == after_V_COLOR && end_of_color_addr != 0) {
            entrypoint.found_at_address = end_of_color_addr;
            fprintf(stderr, "after_V_COLOR at address 0x%x\n", entrypoint.found_at_address);
        } else if (entrypoint.fn == TOWER_MODE && entrypoint.found_at_address != 0) {
            uint8_t dummy;
            start = find_globals_in_pattern({0xa0, WILDCARD, WILDCARD, WILDCARD, 0xa0, 0x05}, {&zg.TOWER_CHANGED, &dummy, &dummy}, entrypoint.found_at_address, 10);
            if (start == -1) {
                fprintf(stderr, "zg.TOWER_CHANGED not found!\n");
            }
        } else if (entrypoint.fn == DO_HINTS && entrypoint.found_at_address != 0) {
            start = find_hints_table(entrypoint.found_at_address);
            if (memory[entrypoint.found_at_address + 8] == 0x88) {
                store_byte(entrypoint.found_at_address + 8, kOpRtrue);
            }
        } else if (entrypoint.fn == DISPLAY_HINT && entrypoint.found_at_address != 0) {
            start = find_display_hint_globals(entrypoint.found_at_address);
            entrypoint.found_at_address = 0; // DISPLAY_HINT
        } else if ((entrypoint.fn == PBOZ_CLICK || entrypoint.fn == B_MOUSE_PEG_PICK || entrypoint.fn == B_MOUSE_WEIGHT_PICK)  &&
                   entrypoint.found_at_address != 0) {
            patch_ret_local(entrypoint.found_at_address, 2);
            // Remember that this revision has mouse support in the mini-game
            // (r242 has none), so that its clickable boxes get drawn.
            if (entrypoint.fn == PBOZ_CLICK)
                zr.PBOZ_CLICK = entrypoint.found_at_address;
            else if (entrypoint.fn == B_MOUSE_PEG_PICK)
                zr.B_MOUSE_PEG_PICK = entrypoint.found_at_address;
        } else if (entrypoint.fn == SN_CLICK && entrypoint.found_at_address != 0) {
            // As with the click routines above: return what SN_CLICK()
            // stored, or the game goes on to do its own hit test (against
            // picture sizes rounded to window pixels) and returns that.
            patch_ret_local(entrypoint.found_at_address, 2);
            zr.SN_CLICK = entrypoint.found_at_address;
        } else if (entrypoint.fn == V_DEFINE && entrypoint.found_at_address != 0) {
            start = find_function_key_tables(entrypoint.found_at_address);
        } else if (entrypoint.fn == V_REFRESH && entrypoint.found_at_address != 0) {
            zr.V_REFRESH = entrypoint.found_at_address - 1;
            start = find_globals_in_pattern({ 0x01, 0xc1, 0x8f, WILDCARD }, { &zg.CURRENT_SPLIT }, entrypoint.found_at_address, 200);
            if (start == -1) {
                start = find_globals_in_pattern({ WILDCARD, 0x01, 0x8c, 0x00, 0x05  }, { &zg.CURRENT_SPLIT }, entrypoint.found_at_address, 100);
                if (start == -1) {
                    fprintf(stderr, "Could not find zg.CURRENT_SPLIT!\n");
                }
            }
        } else if (entrypoint.fn == z0_UPDATE_STATUS_LINE && entrypoint.found_at_address != 0) {
            z0_find_status_line_globals(entrypoint, start);
        } else if (entrypoint.fn == DEFAULT_COLORS && entrypoint.found_at_address != 0) {
            // The DEFAULT-COLORS routine opens with two stores into globals --
            // FG-COLOR <- DEFAULT-FG and BG-COLOR <- DEFAULT-BG -- but the
            // signature { 2d ?? ?? 2d ?? ?? } is just "two adjacent STOREs" and
            // is not unique. The routine only exists from r366 on; in r296/r343
            // (which lack it) it false-matches an unrelated variable-swap idiom
            // whose store targets are *locals/stack* (e.g. r343 0x14423:
            // STORE L00,Gaa / STORE Gaa,G43). Accepting that wrote garbage into
            // fg_global_idx/bg_global_idx/DEFAULT_FG/DEFAULT_BG and -- since
            // DEFAULT_COLORS() is a live hook, not a stub -- corrupted two
            // globals every time the swap routine ran. Validate that all four
            // captured operands are real globals (a local/stack operand decodes
            // to an index >= 0xf0); if not, treat the routine as absent so the
            // hook never registers and the colour guards in z0_update_colors()
            // keep DEFAULT_FG/DEFAULT_BG at 0. (Pre-fix this was masked because
            // the cursor-poisoning bug skipped DEFAULT-COLORS entirely in r343.)
            uint8_t t_fg = 0, t_dfg = 0, t_bg = 0, t_dbg = 0;
            start = find_globals_in_pattern({ 0x2d, WILDCARD, WILDCARD, 0x2d, WILDCARD, WILDCARD }, { &t_fg, &t_dfg, &t_bg, &t_dbg }, entrypoint.found_at_address, 30);
            if (start != -1 && t_fg < 0xf0 && t_dfg < 0xf0 && t_bg < 0xf0 && t_dbg < 0xf0) {
                fg_global_idx = t_fg;
                zg.DEFAULT_FG = t_dfg;
                bg_global_idx = t_bg;
                zg.DEFAULT_BG = t_dbg;
                fprintf(stderr, "zg.DEFAULT_FG = 0x%x zg.DEFAULT_BG = 0x%x\n", zg.DEFAULT_FG, zg.DEFAULT_BG);
            } else {
                fprintf(stderr, "DEFAULT-COLORS false match at 0x%x rejected (no such routine in this revision)\n", entrypoint.found_at_address);
                entrypoint.found_at_address = 0;
            }
        } else if (entrypoint.fn == INIT_STATUS_LINE && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0xeb, 0x7f, 0x07, 0x88, WILDCARD, WILDCARD, WILDCARD }, { &zg.CURRENT_BORDER, &zg.CURRENT_BORDER, &zg.CURRENT_BORDER }, entrypoint.found_at_address, 300);
            if (start == -1) {
                fprintf(stderr, "Could not find zg.CURRENT_BORDER!\n");
            } else {
                fprintf(stderr, "zg.CURRENT_BORDER = 0x%x\n", zg.CURRENT_BORDER);
            }

            // CURRENT-SPLIT fallback. The V_REFRESH-anchored patterns above
            // don't match the middle releases (r296/r343/r366), leaving
            // zg.CURRENT_SPLIT == 0; z0_init_status_line then reads global 0,
            // fails its TEXT-WINDOW/F-SPLIT checks and returns *before* drawing
            // the border -- so no pillars, and the compass has no background
            // graphics window to draw into either (one missing global takes out
            // both). INIT-STATUS-LINE compares CURRENT-SPLIT against F-SPLIT
            // (#01a6) ~80 bytes in on every release (G1d on r343/r366, G1e on
            // r383/r393), so anchor here. Guarded so we don't override a good
            // detection from the V_REFRESH handler.
            if (zg.CURRENT_SPLIT == 0) {
                start = find_globals_in_pattern({ 0xc1, 0x8f, WILDCARD, 0x01, 0xa6 }, { &zg.CURRENT_SPLIT }, entrypoint.found_at_address, 200);
                if (start == -1) {
                    fprintf(stderr, "Could not find zg.CURRENT_SPLIT (INIT-STATUS-LINE fallback)!\n");
                } else {
                    fprintf(stderr, "zg.CURRENT_SPLIT = 0x%x (INIT-STATUS-LINE fallback)\n", zg.CURRENT_SPLIT);
                }
            }
        } else if (entrypoint.fn == FANUCCI && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({ 0x41, WILDCARD, 0x03, WILDCARD, 0xd4, 0x8f, WILDCARD, 0x03, 0xe8, WILDCARD }, { &zg.F_WIN_COUNT, &zg.YOUR_SCORE, &zg.YOUR_SCORE, &zg.YOUR_SCORE, &zg.YOUR_SCORE }, entrypoint.found_at_address, 300);
            if (start == -1) {
                fprintf(stderr, "Did not find zg.F_WIN_COUNT / zg.YOUR_SCORE!\n");
                start = entrypoint.found_at_address;
            } else if (memory[start + 1] == 0xcc && memory[start + 2] == 0x1f) {
                // The score bonus is followed by <FCLEAR ,BROOM ,TRYTAKEBIT>
                zo.BROOM = word(start + 3);
                zp.TRYTAKEBIT = memory[start + 5];
                fprintf(stderr, "zo.BROOM: 0x%x zp.TRYTAKEBIT: 0x%x\n", zo.BROOM, zp.TRYTAKEBIT);
            } else {
                fprintf(stderr, "zo.BROOM not found!\n");
            }
            start = find_globals_in_pattern({ 0xeb, 0x7f, 0x01, 0x8f, WILDCARD, WILDCARD, 0x95, WILDCARD }, { &zg.F_PLAYS, &zg.F_PLAYS, &zg.F_PLAYS }, start, 300);
            if (start == -1) {
                fprintf(stderr, "Did not find zg.F_PLAYS!\n");
            }
        } else if (entrypoint.fn == WINPROP && entrypoint.found_at_address != 0) {
            std::vector<uint8_t> patch = {0xaf, 0x01, 0x02, 0x00, 0xb8};
            start = entrypoint.found_at_address + 2;
            for (int i = 0; i < patch.size(); i++) {
                fprintf(stderr, "Patching 0x%x at address 0x%x (previous value 0x%x)\n", patch[i], start + i, memory[start + i]);
                store_byte(start + i, patch[i]);
            }
        } else if (entrypoint.fn == SETUP_PBOZ && entrypoint.found_at_address != 0) {
            z0_find_peggleboz_globals(entrypoint, start);
        } else if (entrypoint.fn == DRAW_PILE && entrypoint.found_at_address != 0) {
            start = find_16_bit_values_in_pattern({0xcf, 0x2f, WILDCARD, WILDCARD, 0x01, 0x02 }, {&zt.PILE_TABLE}, entrypoint.found_at_address, 10);
            if (start != -1) {
                fprintf(stderr, "zt.PILE_TABLE: 0x%x\n", zt.PILE_TABLE);
            } else {
                fprintf(stderr, "zt.PILE_TABLE not found!\n");
            }
        } else if (entrypoint.fn == SNARFEM && entrypoint.found_at_address != 0) {
            // Find the branch taken when the player removes the last pebble,
            // which SNARFEM() jumps to when the player asks to skip the game.
            // It always opens with STORE L03,#01 (the stop flag), but so does
            // the jester-wins branch further down, so match what follows too:
            //   r343 on:     CLEAR_ATTR FAN,TRYTAKEBIT (byte constants) ; NEW_LINE ; NEW_LINE
            //   r296, r66:   the same with FAN as a word constant
            //   r242:        INSERT_OBJ FAN,HERE ; CLEAR_ATTR FAN,TRYTAKEBIT
            static const std::vector<std::vector<uint8_t>> win_patterns = {
                {0x0d, 0x04, 0x01, 0x0c, WILDCARD, WILDCARD, 0xbb, 0xbb},
                {0x0d, 0x04, 0x01, 0xcc, 0x1f, WILDCARD, WILDCARD, WILDCARD, 0xbb, 0xbb},
                {0x0d, 0x04, 0x01, 0xce, 0x2f, WILDCARD, WILDCARD, WILDCARD, 0xcc, 0x1f},
            };
            for (const auto &pattern : win_patterns) {
                start = find_pattern_in_mem(pattern, entrypoint.found_at_address, 350);
                if (start != -1)
                    break;
            }
            if (start != -1) {
                zr.SNARFEM_WIN = start;
                fprintf(stderr, "zr.SNARFEM_WIN: 0x%x\n", zr.SNARFEM_WIN);
            } else {
                fprintf(stderr, "zr.SNARFEM_WIN not found!\n");
            }
        } else if (entrypoint.fn == PEG_GAME && entrypoint.found_at_address != 0) {
            // Find the code that follows a legal move, which PEG_GAME() jumps
            // to when the player asks to skip the game. The same in every
            // revision:
            //   STORE PEG-SELECTED,#00 ; SET_WINDOW #00 ; ERASE_WINDOW #00 ;
            //   CALL_1S PBOZ-WIN-CHECK -> -(SP) ; JZ (SP)+
            start = find_pattern_in_mem({0x0d, 0x02, 0x00, 0xeb, 0x7f, 0x00, 0xed, 0x7f, 0x00, 0x88, WILDCARD, WILDCARD, 0x00, 0xa0, 0x00}, entrypoint.found_at_address, 800);
            if (start != -1) {
                zr.PEG_GAME_WIN = start;
                fprintf(stderr, "zr.PEG_GAME_WIN: 0x%x\n", zr.PEG_GAME_WIN);
            } else {
                fprintf(stderr, "zr.PEG_GAME_WIN not found!\n");
            }
        } else if (entrypoint.fn == PBOZ_WIN_CHECK && entrypoint.found_at_address != 0) {
            // The counting loop tests each peg right after JE CNT,#16:
            //   r242, r296:  LOADW PEG-TABLE,CNT -> -(SP)  (cf 2f <table> 01 00)
            //   r66 on:      TEST_ATTR NOT-HERE-OBJECT,CNT (ca 2f <object> 01)
            const uint32_t a = entrypoint.found_at_address + 10;
            if (memory[a] == 0xcf && memory[a + 1] == 0x2f) {
                zt.PEG_TABLE = word(a + 2);
                fprintf(stderr, "zt.PEG_TABLE: 0x%x\n", zt.PEG_TABLE);
            } else if (zo.NOT_HERE_OBJECT == 0) {
                // Fallback for a revision whose SETUP-PBOZ was not found;
                // draw_cards() needs NOT-HERE-OBJECT too.
                if (memory[a] == 0xca && memory[a + 1] == 0x2f) {
                    zo.NOT_HERE_OBJECT = word(a + 2);
                    fprintf(stderr, "zo.NOT_HERE_OBJECT (PBOZ-WIN-CHECK fallback): 0x%x\n", zo.NOT_HERE_OBJECT);
                } else {
                    fprintf(stderr, "zo.NOT_HERE_OBJECT fallback not found!\n");
                }
            }
        } else if (entrypoint.fn == DRAW_NEW_HERE && entrypoint.found_at_address != 0) {
            uint8_t philhall = 0, mountain = 0, savannah = 0, highway = 0;
            start = find_values_in_pattern({0xa0, WILDCARD, 0x46, 0x41, WILDCARD, WILDCARD, 0x4c, 0x51, WILDCARD, WILDCARD, 0x01, 0xa0, 0x01 }, {&zg.NARROW, &philhall, &philhall, &zp.P_APPLE_DESC, &zp.P_APPLE_DESC}, entrypoint.found_at_address, 50);
            if (start != -1) {
                start = find_values_in_pattern({0xb0, 0xc1, 0x95, WILDCARD, WILDCARD, WILDCARD, WILDCARD, 0x67 }, {&mountain, &mountain, &savannah, &highway}, start, 20);
                if (start != -1) {
                    zg.NARROW -= 0x10;
                    zo.PHIL_HALL = philhall;
                    zo.G_U_HIGHWAY = highway;
                    zo.G_U_MOUNTAIN = mountain;
                    zo.G_U_SAVANNAH = savannah;
                    fprintf(stderr, "zg.NARROW: 0x%x zo.PHIL_HALL = 0x%x zo.G_U_HIGHWAY = 0x%x zo.G_U_MOUNTAIN = 0x%x zo.G_U_SAVANNAH = 0x%x zp.P_APPLE_DESC = 0x%x\n", zg.NARROW, zo.PHIL_HALL, zo.G_U_HIGHWAY, zo.G_U_MOUNTAIN, zo.G_U_SAVANNAH, zp.P_APPLE_DESC);
                }
            }
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == BLINK && entrypoint.found_at_address != 0) {
            start = find_globals_in_pattern({0xeb, 0x7f, 0x00, 0xe1, 0x9b, WILDCARD, 0x00, 0x05}, {&zg.BLINK_TBL}, entrypoint.found_at_address, 20);
            if (start != -1) {
                fprintf(stderr, "zg.BLINK_TBL: 0x%x\n", zg.BLINK_TBL);
            }
            entrypoint.found_at_address = 0;
        } else if (entrypoint.fn == V_MAP_LOOP && entrypoint.found_at_address != 0) {
            z0_find_map_globals(entrypoint, start);
        } else if (entrypoint.fn == DRAW_COMPASS_ROSE && entrypoint.found_at_address != 0) {
            // zr.DRAW_COMPASS_ROSE + zt.PICINF_TBL drive the *fallback* compass
            // path in z0_UPDATE_STATUS_LINE, used only by the pre-release
            // revisions (r242/r296) that lack a standalone DRAW-NEW-COMP. The
            // releases (incl. r343/r66) drive the compass through DRAW-NEW-COMP
            // (zr.DRAW_NEW_COMP != 0) and never read this fallback; their
            // DRAW-COMPASS-ROSE also reads SL-LOC-TBL rather than PICINF-TBL, so
            // the sub-search wouldn't find a table anyway. The DRAW-NEW-COMP row
            // precedes this one in the table (so zr.DRAW_NEW_COMP is already
            // resolved here), so skip the fallback wiring when it's set.
            // From r343 on the game records which directions are drawn as
            // flags on ROOMS, and DRAW-COMPASS-ROSE opens by testing one:
            // TEST_ATTR ROOMS <local>. The object number is taken from there
            // (ROOMS is not the same object in every release). The betas keep
            // no such flags, and zo.ROOMS stays 0.
            int32_t test_attr = find_pattern_in_mem({ 0xca, 0x2f, WILDCARD, WILDCARD }, entrypoint.found_at_address, 64);
            if (test_attr != -1) {
                zo.ROOMS = word(test_attr + 2);
                fprintf(stderr, "zo.ROOMS = 0x%x\n", zo.ROOMS);
            }
            if (zr.DRAW_NEW_COMP == 0) {
                zr.DRAW_COMPASS_ROSE = entrypoint.found_at_address;
                start = find_16_bit_values_in_pattern({0xcf, 0x1f, WILDCARD, WILDCARD, 0x00, 0x04 }, {&zt.PICINF_TBL}, entrypoint.found_at_address, 32);
                if (start == -1) {
                    fprintf(stderr, "zt.PICINF_TBL not found!\n");
                } else {
                    fprintf(stderr, "zr.DRAW_COMPASS_ROSE at address 0x%x zt.PICINF_TBL at address 0x%x\n", zr.DRAW_COMPASS_ROSE, zt.PICINF_TBL);
                }
            }
            entrypoint.found_at_address = 0;
        }
    }

    z0_find_peg_move_number_fallback();
    z0_find_color_globals_fallback();
}
