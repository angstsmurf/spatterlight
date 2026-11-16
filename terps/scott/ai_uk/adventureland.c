/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  adventureland.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The special opcodes of Adventureland.
 */

#include "closeups.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"
#include "scott_display.h"
#include "adventureland.h"

void AdventurelandDarkness(void)
{
    if ((Rooms[MyLoc].Image & 128) == 128)
        SetDark();
    else
        SetLight();
}

void AdventurelandAction(int p)
{
    int image = 0;
    switch (p) {
    case 1:
        /* BOOM */
        image = 36;
        break;
    case 2:
        /* Genie */
        image = 34;
        break;
    case 3:
        /* Dragon */
        image = 33;
        break;
    case 4:
        /* White Ox */
        image = 35;
        break;
    default:
        fprintf(stderr, "Adventureland: Unsupported action parameter %d!\n", p);
        return;
    }
    /* The original shows nothing, and does not wait for a key, when
       pictures are switched off. */
    if (!GraphicsEnabled() && !closeups_without_graphics)
        return;
    DrawImage(image);
    Output("\n");
    Output(sys[HIT_ENTER]);
    showing_closeup = 1;
    HitEnter();
    return;
}
