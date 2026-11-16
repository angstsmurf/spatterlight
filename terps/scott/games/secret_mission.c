/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  secret_mission.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  The animations and special opcodes of Secret Mission.
 */

#include "sagagraphics.h"
#include "game_specific.h"
#include "scott.h"
#include "scott_meta.h"
#include "scott_actions.h"
#include "scott_display.h"
#include "secret_mission.h"

/* Secret Mission */
enum {
    ROOM_AFTER_EXPLOSION = 1, /* The blank room the bomb leaves */
    ROOM_VISITORS_ROOM = 13, /* Large white visitors room */
};

enum {
    ITEM_SECURITY_ID = 1, /* Picture of me stamped -security- */
    ITEM_VISITOR_ID = 7,
    ITEM_MAINTENANCE_ID = 8,
    ITEM_PROJECTOR = 21, /* Movie projector with film cartridge */
    ITEM_SABOTEUR_ID = 41, /* ... stamped -window maintenance- */
};

void UpdateSecretAnimations(void)
{
    if (MyLoc != ROOM_VISITORS_ROOM || AnimationFlag == 0) {
        glk_request_timer_events(0);
        AnimationFlag = 0;
        return;
    }
    /* Roll down */
    if (AnimationFlag > 0 && AnimationFlag < 48) {
        RectFill(113, 0, 102, AnimationFlag, WhiteColour());
        AnimationFlag++;
    }
    /* Flicker */
    if (AnimationFlag >= 48 && AnimationFlag < 100) {
        if (Items[ITEM_PROJECTOR].Location == 0) {
            /* Image 30 is a white square */
            DrawImage(30);
            glk_request_timer_events(0);
            AnimationFlag = 0;
        } else if (AnimationFlag > 69) {
            if (AnimationFlag == 70)
                DrawImage(30);
            else
                DrawBlack();
            AnimationFlag = 70 + (AnimationFlag == 70);
        } else {
            AnimationFlag++;
        }
    }
    /* Roll up */
    if (AnimationFlag >= 100 && AnimationFlag < 200) {
        int height = AnimationFlag - 100;
        if (height)
            RectFill(112, 48 - height, 104, height, 0);
        AnimationFlag++;
    }
}

void SecretAction(int p)
{
    int oldimage = 0;
    switch (p) {
    case 1: /* Bomb explodes */
        if (split_screen) {
            CloseTopWindow();
            MyLoc = ROOM_AFTER_EXPLOSION;
        }
        Rooms[ROOM_AFTER_EXPLOSION].Image = 25; /* BOOM */
        DrawImage(25);
        Delay(2);
        DrawImage(26);
        Rooms[ROOM_AFTER_EXPLOSION].Image = 26; /* Mushroom cloud */
        break;
    case 2: /* ID picture */
        DrawImage(24);
        if (IsPresent(ITEM_SECURITY_ID)) {
            DrawImage(27); /* "Security" label */
        } else if (IsPresent(ITEM_VISITOR_ID) && !IsPresent(ITEM_MAINTENANCE_ID) && !IsPresent(ITEM_SABOTEUR_ID)) {
            DrawImage(28); /* "Visitor" label */
        }
        showing_closeup = 1;
        Output(sys[HIT_ENTER]);
        HitEnter();
        break;
    case 3: /* Screen rolls down */
    case 4:
        oldimage = Rooms[ROOM_VISITORS_ROOM].Image;
        /* Image 30 is a white square */
        Rooms[ROOM_VISITORS_ROOM].Image = 30;
        DrawBlack();
        AnimationFlag = 1;
        glk_request_timer_events(10);
        Output(sys[HIT_ENTER]);
        Output("\n");
        HitEnter();
        DrawImage(30);
        AnimationFlag = 100;
        glk_request_timer_events(5);
        event_t ev;
        do {
            glk_select(&ev);
            Updates(ev);
        } while (AnimationFlag < 200);
        Rooms[ROOM_VISITORS_ROOM].Image = oldimage;
        AnimationFlag = 0;
        break;
    default:
        fprintf(stderr, "Secret Mission: Unsupported action parameter %d!\n", p);
        return;
    }
}
