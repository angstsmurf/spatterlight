/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  closeups.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The table of close-up pictures, shown when the player examines something.
 */

#include "sagagraphics.h"
#include "us_display.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_display.h"
#include "closeups.h"

/* Draw a close-up image and wait for a key press. The room image is
   redrawn after that. */
void ShowCloseup(int image)
{
    DrawImage(image);
    showing_closeup = 1;
    Output(sys[HIT_ENTER]);
    HitEnter();
}

void ShowUSCloseup(int image, int offset) {
    if (image >= 0) {
        if (Graphics)
            glk_window_clear(Graphics);
        if (DrawUSRoom(offset + image)) {
            showing_closeup = 1;
            DrawImageOrVector();
            Output(sys[HIT_ENTER]);
            HitEnter();
        }
    }
}

/* The original interpreters have a table of close-up pictures, which the
   loaders that know where it is hand over: looking at the noun of an entry
   shows its picture if its item is carried or in the room. */
/* The sixteen of an original table and a few of our own. */
#define MAX_CLOSEUPS 24

static struct {
    int item;
    int noun;
    int image;
} closeups[MAX_CLOSEUPS];

static int num_closeups = 0;
static int closeup_verb = -1;
static int closeups_are_us_pictures = 0;

/* Starts a new table. us_pictures is whether the pictures are the room
   pictures of a US release rather than the numbered ones of a UK one. */
void SetCloseupVerb(int verb, int us_pictures)
{
    closeup_verb = verb;
    closeups_are_us_pictures = us_pictures;
    num_closeups = 0;
}

void AddCloseup(int item, int noun, int image)
{
    if (num_closeups == MAX_CLOSEUPS || item < 0 || item > GameHeader.NumItems)
        return;
    closeups[num_closeups].item = item;
    closeups[num_closeups].noun = noun;
    closeups[num_closeups].image = image;
    num_closeups++;
}

/* Gives the picture of the entry for item and noun to another item and
   noun as well. Does nothing if the table has no such entry, or already
   has the other one. */
static void AddCloseupAlias(int item, int noun, int alias_item, int alias_noun)
{
    for (int i = 0; i < num_closeups; i++) {
        if (closeups[i].item == alias_item && closeups[i].noun == alias_noun)
            return;
    }
    for (int i = 0; i < num_closeups; i++) {
        if (closeups[i].item == item && closeups[i].noun == noun) {
            AddCloseup(alias_item, alias_noun, closeups[i].image);
            return;
        }
    }
}

/* Close-ups that the originals do not show but that are nice to have: an
   item that turns into another one loses its picture in the original, and
   so does an obvious other word for it. To be called once the table of
   the game has been read. */
void AddExtraCloseups(GameIDType game)
{
    switch (game) {
    case ADVENTURELAND_US:
        AddCloseupAlias(24, 23, 26, 23); /* Bees in a bottle */
        break;
    case STRANGE_ODYSSEY_US:
        AddCloseupAlias(10, 22, 11, 22); /* The other phaser */
        AddCloseupAlias(30, 8, 31, 8); /* The other hound */
        break;
    case VOODOO_CASTLE_US:
        AddCloseupAlias(40, 42, 38, 42); /* Chemicals before they are mixed */
        /* Count Cristo is LOOK COFFIN on the Apple II and LOOK MAN on the
           Atari. */
        AddCloseupAlias(27, 8, 27, 53);
        AddCloseupAlias(27, 53, 27, 8);
        break;
    case HULK_US:
    case HULK_US_PREL:
        AddCloseupAlias(27, 66, 27, 26); /* LOOK BASE for the gas outlet */
        /* Fallthrough */
    case HULK:
    case HULK_C64:
        /* LOOK GEM for the bio-gem, which only some US releases have */
        AddCloseupAlias(18, 124, 18, 41);
        break;
    default:
        break;
    }
}

int HasCloseupTable(void)
{
    return closeup_verb != -1;
}

/* For the tests. Returns 0 when there is no such entry. */
int GetCloseup(int index, int *verb, int *item, int *noun, int *image)
{
    if (index < 0 || index >= num_closeups)
        return 0;
    *verb = closeup_verb;
    *item = closeups[index].item;
    *noun = closeups[index].noun;
    *image = closeups[index].image;
    return 1;
}

/* The original searches from the end, and when a noun has several entries
   it shows the last one whose item is there. */
/* For the headless tests only, which have no graphics but still want the
   table looked up: go through with a close-up (and its key press) anyway. */
int closeups_without_graphics = 0;

void ShowCloseupOnExamine(int verb, int noun)
{
    if (verb != closeup_verb || noun == 0)
        return;
    /* Like the original, show nothing (and do not wait for a key) when
       pictures are switched off. */
    if (!GraphicsEnabled() && !closeups_without_graphics)
        return;
    for (int i = num_closeups - 1; i >= 0; i--) {
        if (closeups[i].noun != noun || !IsPresent(closeups[i].item))
            continue;
        if (closeups_are_us_pictures)
            ShowUSCloseup(closeups[i].image, 0);
        else
            ShowCloseup(closeups[i].image);
        return;
    }
}
