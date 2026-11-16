/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  us_display.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Room and inventory pictures of the US (SAGA) games: picking the room
 *  image, drawing the object images on top, and the game-specific overlays.
 */


#include "apple2draw.h"
#include "apple2_vector_draw.h"
#include "atari_8bit_vector_draw.h"
#include "c64a8scott.h"
#include "c64_small.h"
#include "pcdraw.h"
#include "randomness.h"
#include "rtpi_graphics.h"
#include "sagagraphics.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"

#include "us_display.h"

/* Draw an Apple II format image. Clears the screen buffer first for room
   images to avoid compositing artifacts from the previous scene. */
static int DrawApple2Image(USImage *image)
{
    if (image->usage == IMG_ROOM)
        ClearApple2ScreenMem();
    DrawApple2ImageFromData(image->imagedata, image->datasize, CurrentGame == COUNT_US ? APPLE2_COUNT : APPLE2_STANDARD, NULL);
    debug_print("Drawing image with index %d, usage %d\n", image->index, image->usage);
    return 1;
}

/* Draw a DOS/PC (CGA) format image. */
static int DrawDOSImage(USImage *image) {
    if (image->imagedata == NULL) {
        fprintf(stderr, "DrawDOSImage: ptr == NULL\n");
        return 0;
    }

    debug_print("DrawDOSImage: usage:%d index:%d\n", image->usage, image->index);

    StartPixelRuns();
    int result = DrawDOSImageFromData(image->imagedata, image->datasize);
    FlushPixelRuns();
    return result;
}

/* Dispatch image drawing to the appropriate platform-specific renderer
   based on the image's system type (DOS, C64, Apple II, Atari 8-bit,
   TI-99/4A). */
static int DrawUSImage(USImage *image)
{
    last_image_index = image->index;
    switch (image->systype) {
        case SYS_MSDOS:
            return DrawDOSImage(image);
        case SYS_C64_TINY:
            return DrawMiniC64(image);
        case SYS_APPLE2:
            return DrawApple2Image(image);
        case SYS_APPLE2_LINES:
            return DrawApple2VectorImage(image);
        case SYS_C64:
        case SYS_ATARI8:
            return DrawC64A8Image(image);
        case SYS_ATARI8_LINES:
            DrawAtari8BitVectorImage(image);
            return 1;
        case SYS_TI994A:
            DrawRTPIImage(image);
            return 1;
        default:
            return 0;
    }
}

/* Draw images for all items the player is currently carrying.
   Iterates the image list in forward order (low index to high). */
static void DrawInventoryImages(void)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if ((image->usage == IMG_INV_OBJ || image->usage == IMG_INV_AND_ROOM_OBJ) &&
                image->index <= GameHeader.NumItems &&
                Items[image->index].Location == CARRIED) {
                DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
}

static void DrawRoomObjectImages(void)
{
    /* We must draw the object images like this, from highest index
       to lowest, to match the original and not get too many
       weird results where objects are partially covered by
       stuff which ought to be behind them.
       (While the images on the inventory screen are drawn in the
       opposite order.) */
    for (int i = GameHeader.NumItems; i >= 0; i--) {
        if (Items[i].Location == MyLoc) {
            DrawUSRoomObject(i);
        }
    }
}

/* Find and draw the room image matching the given room number.
   Returns 1 on success, 0 if no matching image was found. */
int DrawUSRoom(int room)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM && image->index == room) {
                return DrawUSImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
    return 0;
}

/* Rooms and items of the US games that get overlays */
enum {
    HULK_ITEM_LARGE_HOLE = 14, /* Two items, 14 and 15 */
    HULK_ITEM_BIO_GEM = 18,
    HULK_ITEM_WAX = 21,

    COUNT_ROOM_FLOWER_BOX = 5,
    COUNT_ROOM_BATHROOM = 8,
    COUNT_ROOM_OUTSIDE_CASTLE = 9,
    COUNT_ROOM_PIT = 18,
    COUNT_ITEM_COAT_OF_ARMS = 5,
    COUNT_ITEM_MIRROR = 17,
    COUNT_ITEM_SHEET_END = 35, /* The other end of the sheet */
    COUNT_ITEM_DAISIES = 61,

    VOODOO_ROOM_CHIMNEY = 14,
    VOODOO_ITEM_BOARDS = 45,

    RTPI_ROOM_SEA = 11,
    RTPI_ROOM_UNDERSEA = 12,
    RTPI_ROOM_UNDER_DOCK = 22, /* On beam under dock */
    RTPI_ITEM_DOCK_EAST = 26,
    RTPI_ITEM_DOCK_UNDERSIDE = 27,
    RTPI_ITEM_BOAT_WEST = 28,
    RTPI_ITEM_DOCK = 32, /* DESTROYED at night */
};

/* Draw a "fuzzy" (blurred/distorted) version of the room image for
   Return to Pirate's Isle. Used when the player's vision is impaired
   (not wearing corrective lenses). Suppressed when it's truly dark
   (at sea at night with no floodlights). */
int DrawFuzzyRoom(int room)
{
    /* If at sea at night, and floodlights are off or battery is out, don't
       draw fuzzy image (because it really is dark) */
    if (Items[RTPI_ITEM_DOCK].Location == DESTROYED && (!TestBitFlag(29) || CurrentCounter <= 0))
        return 0;
    /* Bitflag 2 is set while we cannot see clearly: the glasses are off or
       lost, the face mask is off or full of water, or the silt is stirred
       up. The game then says everything is "too FUZZY to see". If the flag
       is clear, it really is dark. */
    if (!TestBitFlag(2))
        return 0;

    USImage *image = USImages;
    if (image != NULL) {
        do {
            if (image->usage == IMG_ROOM && image->index == room) {
                return DrawFuzzyRTPIImage(image);
            }
            image = image->next;
        } while (image != NULL);
    }
    return 0;
}

/* Find and draw the object image for a given item index as it appears
   in the current room. Searches for images tagged as room objects. */
void DrawUSRoomObject(int item)
{
    USImage *image = USImages;
    if (image != NULL) {
        do {
            if ((image->usage == IMG_ROOM_OBJ ||
                image->usage == IMG_INV_AND_ROOM_OBJ) &&
                image->index == item) {
                DrawUSImage(image);
                return;
            }
            image = image->next;
        } while (image != NULL);
    }
}

/* The room image to show for the current location. The US disk releases
   (except Apple II) of The Incredible Hulk reuse images for several
   locations. */
static int USRoomImage(void)
{
    static const struct {
        uint8_t loc;
        uint8_t image;
    } hulk_aliases[] = {
        { 5, 3 }, { 6, 3 },     /* Tunnel going outside */
        { 7, 4 }, { 8, 4 },     /* Field */
        { 10, 9 }, { 11, 9 },   /* Hole */
        { 13, 2 }, { 14, 2 },   /* Dome */
        { 17, 16 }, { 18, 16 }, /* warp */
    };

    if (CurrentGame == HULK_US && USImages && USImages->systype != SYS_APPLE2) {
        for (size_t i = 0; i < sizeof hulk_aliases / sizeof hulk_aliases[0]; i++)
            if (hulk_aliases[i].loc == MyLoc)
                return hulk_aliases[i].image;
    }
    return MyLoc;
}

/* Game-specific composite image overlays.
   Some objects are drawn as extra layers on top of the room image,
   but only when they are present in specific rooms. These couldn't
   be handled by the generic DrawRoomObjectImages() because they
   use pseudo-item indices or conditional room checks. */
static void DrawGameSpecificOverlays(void)
{
    switch (CurrentGame) {
        case HULK_US:
            if (Items[HULK_ITEM_BIO_GEM].Location == MyLoc && MyLoc == Items[HULK_ITEM_BIO_GEM].InitialLoc)
                DrawUSRoomObject(70);
            if (Items[HULK_ITEM_WAX].Location == MyLoc && MyLoc == Items[HULK_ITEM_WAX].InitialLoc)
                DrawUSRoomObject(72);
            if (Items[HULK_ITEM_LARGE_HOLE].Location == MyLoc || Items[HULK_ITEM_LARGE_HOLE + 1].Location == MyLoc)
                DrawUSRoomObject(13);
            break;
        case COUNT_US:
            if (Items[COUNT_ITEM_MIRROR].Location == MyLoc && MyLoc == COUNT_ROOM_BATHROOM)
                DrawUSRoomObject(80);
            if (Items[COUNT_ITEM_SHEET_END].Location == MyLoc && MyLoc == COUNT_ROOM_PIT)
                DrawUSRoomObject(81);
            if (Items[COUNT_ITEM_COAT_OF_ARMS].Location == MyLoc && MyLoc == COUNT_ROOM_OUTSIDE_CASTLE)
                DrawUSRoomObject(82);
            if (Items[COUNT_ITEM_DAISIES].Location == MyLoc && MyLoc == COUNT_ROOM_FLOWER_BOX) /* Apple II only */
                DrawUSRoomObject(83);
            break;
        case VOODOO_CASTLE_US:
            if (Items[VOODOO_ITEM_BOARDS].Location == MyLoc && MyLoc == VOODOO_ROOM_CHIMNEY)
                DrawUSRoomObject(80);
            break;
        case RETURN_TO_PIRATES_ISLE:
            /* Dock scene uses layered compositing: underside of dock,
               then beam overlays, and nighttime sea/boat variants. */
            if (MyLoc == RTPI_ROOM_UNDER_DOCK) {
                DrawUSRoomObject(27); /* Draw underside of dock when under the dock */
                DrawUSRoom(31);       /* Draw low beam (on top of underside of dock) */
            } else if (MyLoc == RTPI_ROOM_SEA) {
                if (erkyrath_random() % 2 == 1) {
                    /* If a random chance of 1 in 2 succeeds, randomly draw one
                       of two fish images (but not if it would end up on top of
                       the boat or dock) */
                    int fish = 33 + erkyrath_random() % 2;
                    if (!(fish == 34 && Items[RTPI_ITEM_BOAT_WEST].Location == MyLoc) &&
                        !(fish == 33 && Items[RTPI_ITEM_DOCK_EAST].Location == MyLoc)) {
                        DrawUSRoom(fish);
                    }
                }
                if (Items[RTPI_ITEM_DOCK].Location == DESTROYED) {
                    DrawUSRoom(29);       /* Draw sea at night */
                    if (Items[RTPI_ITEM_BOAT_WEST].Location == MyLoc) {
                        DrawUSRoom(32);   /* Boat to the west at night */
                    }
                } else if (Items[RTPI_ITEM_DOCK_UNDERSIDE].Location == MyLoc) {
                    /* Draw high beam */
                    DrawUSRoom(30);
                }
            } else if (MyLoc == RTPI_ROOM_UNDERSEA) {
                /* If a random chance of 1 in 2 succeeds, randomly draw one of
                   three fish images */
                if (erkyrath_random() % 2 == 1) {
                    DrawUSRoom(35 + erkyrath_random() % 3);
                }
            }

            /* If it is night and we are out of battery, just make the image
               black by clearing the VDP */
            if (Items[RTPI_ITEM_DOCK].Location == DESTROYED && (!TestBitFlag(29) || CurrentCounter <= 0)) {
                ClearVDP();
            }

            break;
        default:
            break;
    }
}

/* Main drawing routine for US SAGA games. Redraws the graphics window
   with the current room image and any visible object overlays.

   Called whenever the display needs refreshing: after movement, taking/
   dropping items, restore/undo, window resize, preference changes, or
   lighting changes.

   Contains per-game special cases for composite images (e.g. The Hulk
   reuses room images for multiple locations; The Count has context-dependent
   object overlays; Return to Pirate's Isle composites dock/beam/boat
   layers). */
void LookUS(void)
{
    if (!Graphics || !(should_look_in_transcript || should_draw_image))
        return;

    if (Top != Bottom)
        glk_window_clear(Top);

    if (should_draw_image) {
        glk_window_clear(Graphics);
        if (!DrawUSRoom(USRoomImage())) {
            return;
        }

        DrawRoomObjectImages();
        DrawGameSpecificOverlays();
        DrawImageOrVector();
    }
    should_draw_image = 0;
}

/* Display the inventory screen. Room image 98 is used as the inventory
   background. If no such image exists, falls back to redrawing
   the normal room view. Carried item images are composited on top. */
void InventoryUS(void)
{
    if (!Graphics || !HasGraphics())
        return;
    glk_window_clear(Graphics);
    if (!DrawUSRoom(INVENTORY_IMAGE)) {
        should_draw_image = 1;
        LookUS();
        return;
    }
    DrawInventoryImages();
    DrawImageOrVector();
    if (!showing_inventory) {
        showing_inventory = 1;
        Output(sys[HIT_ENTER]);
        HitEnter();
    }
}
