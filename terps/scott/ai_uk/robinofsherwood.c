//
//  robinofsherwood.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-10.
//

#include <ctype.h>
#include <string.h>

#include "decompresstext.h"
#include "sagagraphics.h"
#include "irmak.h"
#include "scott.h"

extern Image *images;

/* Sub-image table for the 63 procedurally assembled forest locations
   (rooms 11–73). Each forest room is described by a variable-length
   sequence of image indices; see draw_sherwood(). */
static uint8_t *forest_images = NULL;

#define WATERFALL_ANIMATION_RATE 15

static void animate_lightning(int stage);

/* Handle game-specific scripted actions triggered by the action table.
   0 = lightning flash at the stone circle (palette-swap animation loop),
   1 = Herne the Hunter apparition (show close-up, move Herne to room 79),
   2 = climb a tree in the forest (save current location, move to room 93). */
void SherwoodAction(int p)
{
    event_t ev;

    switch (p) {
    case 0:
        /* Lightning flash: run 11 frames of palette-swap animation,
           blocking until the sequence completes */
        AnimationFlag = 1;
        glk_request_timer_events(15);

        while (AnimationFlag < 11) {
            glk_select(&ev);
            if (ev.type == evtype_Timer) {
                AnimationFlag++;
                animate_lightning(AnimationFlag);
            }
        }
        break;
    case 1:
        /* Herne the Hunter close-up, then place him at the waterfall cave */
        DrawImage(0);
        Display(Bottom, "\n%s\n", sys[HIT_ENTER]);
        showing_closeup = 1;
        HitEnter();
        Items[39].Location = 79;
        Look();
        break;
    case 2:
        /* Climb a tree: remember current forest location so we can
           return when the player climbs back down */
        SavedRoom = MyLoc;
        MyLoc = 93;
        Look();
        break;
    default:
        fprintf(stderr, "Unhandled SherwoodAction %d!\n", p);
        break;
    }
}

static int IsForestLocation(void) { return (MyLoc >= 11 && MyLoc <= 73); }

/* Forest scene composition types */
#define TREES 0  /* 10–11 sub-images: 8 tree columns + undergrowth + optional paths */
#define BUSHES 1 /* 5 sub-images placed side by side */

/* Assemble and draw a procedural forest image for the given location.
   The forest_images table stores a variable-length record per forest room.
   Bit 7 of the first byte distinguishes BUSHES (set) from TREES (clear).
   TREES records have 10 or 11 sub-image indices (trees only vs. trees with
   a path); BUSHES records always have 5. Sub-images are positioned on a
   fixed grid for TREES, or packed left-to-right for BUSHES. */
static void draw_sherwood(int loc)
{
    glk_window_clear(Graphics);
    int subimage_index = 0;

    /* Skip past preceding rooms' records to reach the one for loc */
    for (int i = 0; i < loc - 11; i++) {
        int skip = 5; /* BUSHES: 5 sub-images */
        if (forest_images[subimage_index] < 128)
            skip = 11; /* TREES: 10 or 11 (skip the maximum) */
        subimage_index += skip;
    }

    int forest_type = TREES;
    int subimages;

    if (forest_images[subimage_index] >= 128) {
        forest_type = BUSHES;
        subimages = 5;
    } else if (forest_images[subimage_index + 10] == 0xff) {
        subimages = 10; /* Trees only, no path */
    } else {
        subimages = 11; /* Trees with a forward path */
    }

    int xpos = 0, ypos = 0, image_number;

    for (int i = 0; i < subimages; i++) {
        if (forest_type == TREES) {
            if (i >= 8) {
                if (i == 8) {        /* Undergrowth strip */
                    ypos = 7;
                    xpos = 0;
                } else if (i == 9) { /* Bottom path */
                    ypos = 10;
                    xpos = 0;
                } else {             /* Forward path overlay */
                    ypos = 7;
                    xpos = 12;
                }
            } else {
                /* 8 tree columns, each 4 characters wide */
                ypos = 0;
                xpos = i * 4;
            }
        }

        image_number = forest_images[subimage_index++] & 127;

        DrawPictureAtPos(image_number, xpos, ypos, 0);

        if (forest_type == BUSHES) {
            /* BUSHES sub-images are variable-width, packed left to right */
            xpos += images[image_number].width;
        }
    }
}

/* Animate the waterfall in room 86. Scrolls the blue water pixels
   downward by 'stage' pixels (wrapping at 80), over a white background.
   Reads pixel data from the layout buffer (not imagebuffer) so the
   source pattern stays constant across frames. */
static void animate_waterfall(int stage)
{
    RectFill(88, 16, 48, 64, white_colour);
    for (int line = 2; line < 10; line++) {
        for (int col = 11; col < 17; col++) {
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    if (isNthBitSet(layout[col + line * IRMAK_IMGWIDTH][i], 7 - j)) {
                        int ypos = line * 8 + i + stage;
                        if (ypos > 79)
                            ypos = ypos - 64;
                        PutPixel(col * 8 + j, ypos, blue_colour);
                    }
                }
            }
        }
    }
}

/* Animate the waterfall visible from inside the cave (room 79).
   Same downward-scrolling technique as animate_waterfall, but only
   a single column wide (8 pixels) at the right edge of the image. */
static void animate_waterfall_cave(int stage)
{
    RectFill(248, 24, 8, 64, white_colour);
    for (int line = 3; line < 11; line++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                if (isNthBitSet(layout[31 + line * IRMAK_IMGWIDTH][i], 7 - j)) {
                    int ypos = line * 8 + i + stage;
                    if (ypos > 87)
                        ypos = ypos - 64;
                    PutPixel(248 + j, ypos, blue_colour);
                }
            }
        }
    }
}

/* Draw one frame of the flash animation at the stone circle. Each
   call swaps blue/yellow palette entries (platform-dependent colours)
   and redraws image 77, creating a strobe effect. Stage 3 holds a long
   pause (700 ms) for dramatic effect; stage 11 ends the sequence. */
void animate_lightning(int stage)
{
    if (palchosen == C64B) {
        SwitchPalettes(6, 7);
    } else {
        SwitchPalettes(1, 14);
        SwitchPalettes(9, 6);
    }
    DrawPictureNumber(77, 0);
    if (stage == 11) {
        glk_request_timer_events(0);
    } else if (stage == 3) {
        glk_request_timer_events(700);
    } else {
        glk_request_timer_events(40);
    }
}

/* Draw the graphics for the current room. Non-forest rooms use their
   stored image index plus item overlays. Forest rooms (11–73) are
   assembled procedurally via draw_sherwood, with additional overlays
   for NPCs and the campfire. Also handles the tree-climbing rooms
   (82/93) and starts waterfall animation timers for rooms 86 and 79. */
void RobinOfSherwoodLook(void)
{
    if (!IsForestLocation()) {
        if (Rooms[MyLoc].Image == 255) {
            CloseGraphicsWindow();
        } else {
            DrawImage(Rooms[MyLoc].Image);
            /* Draw images for items present in this room */
            for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
                if (Items[ct].Image) {
                    if ((Items[ct].Flag & 127) == MyLoc && Items[ct].Location == MyLoc) {
                        DrawImage(Items[ct].Image);
                    }
                }
            }
        }
    }

    /* Room 82 is a dummy exit target from "up a tree" — redirect to
       the forest room the player climbed from */
    if (MyLoc == 82)
        MyLoc = SavedRoom;
    /* Room 93 is "up a tree" — move any items here to the actual
       forest location so they appear when the player climbs down */
    if (MyLoc == 93)
        for (int i = 0; i < GameHeader.NumItems; i++)
            if (Items[i].Location == 93)
                Items[i].Location = SavedRoom;
    /* Open treasure chest overlay in the left bedroom */
    if (MyLoc == 7 && Items[62].Location == 7)
        DrawImage(70);
    if (IsForestLocation()) {
        OpenGraphicsWindow();
        draw_sherwood(MyLoc);

        if (Items[36].Location == MyLoc) {
            /* Gregory the tax collector with his horse and cart */
            DrawImage(15);
            DrawImage(3);
        }
        if (Items[60].Location == MyLoc || Items[77].Location == MyLoc) {
            /* A serf driving a horse and cart */
            DrawImage(15);
            DrawImage(12);
        }
        if (MyLoc == 73) {
            DrawImage(36); /* Campfire at the outlaws' camp */
        }
    }

    /* Start waterfall animation at the waterfall or inside the cave */
    if (MyLoc == 86 || MyLoc == 79) {
        glk_request_timer_events(WATERFALL_ANIMATION_RATE);
    }
}

/* Timer callback for Robin of Sherwood animations. AnimationFlag cycles
   0–63 as the vertical scroll offset for waterfalls. Room 84 (stone
   circle) keeps the timer alive without drawing — the lightning animation
   uses the same timer and would be cancelled if we stopped it here. */
void UpdateRobinOfSherwoodAnimations(void)
{
    AnimationFlag++;
    if (AnimationFlag > 63)
        AnimationFlag = 0;
    if (MyLoc == 86 || MyLoc == 79 || MyLoc == 84) {
        if (MyLoc == 86) {
            animate_waterfall(AnimationFlag);
        } else if (MyLoc == 79) {
            animate_waterfall_cave(AnimationFlag);
        }
    } else {
        glk_request_timer_events(0);
    }
}

/* Load extra data that Robin of Sherwood stores outside the standard
   Scott Adams database: per-room image indices, compressed room
   descriptions, system message mappings, and the forest sub-image
   table. All offsets are platform-dependent (C64 vs Spectrum). */
void LoadExtraSherwoodData(int c64)
{
    /* --- Room image indices --- */
    /* One byte per room. Forest rooms (11–73) are skipped because they
       use the procedural draw_sherwood() instead of a single image. */

    int offset = file_baseline_offset + ((c64 == 1) ? 0x1ffd : 0x3d99);

    uint8_t *ptr= SeekToPos(offset);

    if (ptr == NULL)
        return;

    int ct;
    Room *rp = Rooms;

    for (ct = 0; ct <= GameHeader.NumRooms; ct++) {
        rp->Image = *(ptr++);
        rp++;
        if (ct == 10) {
            /* Skip the 63 forest rooms (11–73) — no stored images */
            for (int i = 0; i < 63; i++) {
                rp++;
                ct++;
            }
        }
    }

    /* --- Room descriptions (5-bit compressed text) --- */
    /* Forest rooms all share the same hardcoded description. The first
       letter of each decompressed description is lowercased because
       the engine prepends "You are ". */

    ct = 0;
    rp = Rooms;

    offset = file_baseline_offset + ((c64 == 1) ? 0x402e : 0x5b7e);

    ptr = SeekToPos(offset);
    if (ptr == NULL)
        return;

    do {
        rp->Text = DecompressText(ptr, ct);
        *(rp->Text) = tolower(*(rp->Text));
        ct++;
        if (ct == 11) {
            /* Fill 61 forest rooms with the shared description
               (rooms 12–72; room 11 was decompressed above, room 73
               is handled by the next iteration) */
            for (int i = 0; i < 61; i++) {
                rp++;
                rp->Text = "in Sherwood Forest";
            }
        }
        rp++;
    } while (ct < 33);

    /* --- System message mapping --- */
    /* C64 and Spectrum versions store system messages in different orders;
       map each platform's message array to the canonical SysMessageType keys */

    if (c64 == 1) {
        SysMessageType messagekey[] = {
            NORTH,
            SOUTH,
            EAST,
            WEST,
            UP,
            DOWN,
            EXITS,
            YOU_SEE,
            YOU_ARE,
            HIT_ENTER,
            YOU_CANT_GO_THAT_WAY,
            OK,
            WHAT_NOW,
            HUH,
            YOU_HAVE_IT,
            TAKEN,
            DROPPED,
            YOU_HAVENT_GOT_IT,
            INVENTORY,
            YOU_DONT_SEE_IT,
            THATS_BEYOND_MY_POWER,
            DIRECTION,
            YOURE_CARRYING_TOO_MUCH,
            PLAY_AGAIN,
            RESUME_A_SAVED_GAME,
            YOU_CANT_DO_THAT_YET,
            I_DONT_UNDERSTAND,
            NOTHING
        };

        for (int i = 0; i < 26; i++) {
            sys[messagekey[i]] = system_messages[i];
        }

        sys[HIT_ENTER] = system_messages[30];
        sys[WHAT] = system_messages[13];

    } else {
        SysMessageType messagekey[] = {
            BAD_DATA,
            OK,
            OK,
            PLAY_AGAIN,
            I_DONT_UNDERSTAND,
            YOU_CANT_DO_THAT_YET,
            HUH,
            DIRECTION,
            YOU_HAVENT_GOT_IT,
            YOU_HAVE_IT,
            YOU_DONT_SEE_IT,
            THATS_BEYOND_MY_POWER,
            YOU_CANT_GO_THAT_WAY,
            YOU_ARE,
            YOU_SEE,
            YOU_SEE,
            EXITS,
            INVENTORY,
            NOTHING,
            WHAT_NOW,
            HIT_ENTER,
            YOURE_CARRYING_TOO_MUCH,
            RESUME_A_SAVED_GAME
        };

        for (int i = 0; i < 23; i++) {
            sys[messagekey[i]] = system_messages[i];
        }

        sys[WHAT] = sys[HUH];
    }
    sys[EXITS_DELIMITER] = " ";
    sys[MESSAGE_DELIMITER] = ". ";

    /* --- Forest sub-image table --- */
    /* 555 bytes of packed sub-image indices describing the composition
       of each forest room (rooms 11–73). See draw_sherwood(). */

    offset = file_baseline_offset + ((c64 == 1) ? 0x2300 : 0x3b6e);

    ptr = SeekToPos(offset);
    if (ptr == NULL)
        return;

    int cells = 555;
    forest_images = MemAlloc(cells);

    for (int i = 0; i < cells; i++)
        forest_images[i] = *(ptr++);

}
