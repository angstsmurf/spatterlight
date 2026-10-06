//
//  hulk.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-01-18.
//

#include "apple2draw.h"
#include "game_specific.h"
#include "saga.h"
#include "sagagraphics.h"
#include "scott.h"
#include "scott_display.h"

void HulkShowImageOnExamineUS(int noun)
{
    if (!Graphics)
        return;
    int image = -1;
    switch (noun) {
    case 55:
        if (Items[11].Location == MyLoc)
            // Dome
            image = 0;
        break;
    case 124: // Bio-Gem
    case 41:
        if (IsPresent(18))
            image = 1;
        break;
    case 108:
        if (IsPresent(17))
            // Natter energy egg
            image = 2;
        break;
    case 72:
        if (IsPresent(20))
            // Alien army ants
            image = 3;
        break;
    case 21: // Killer Bees
        if (Items[24].Location == MyLoc)
            image = 4;
        break;
    case 83: // Iron ring
        if (Items[33].Location == MyLoc)
            image = 5;
        break;
    case 121: // Cage
        if (Items[47].Location == MyLoc)
            image = 6;
        break;
    case 26: // BASE
    case 66: // HOLE
        if (MyLoc == 14)
            image = 7;
        break;
    default:
        break;
    }

    ShowUSCloseup(image, 90);
}

void HulkShowImageOnExamine(int noun)
{
    if (CurrentGame == HULK_US) {
        HulkShowImageOnExamineUS(noun);
        return;
    }
    int image = 0;
    switch (noun) {
    case 55:
        if (Items[11].Location == MyLoc)
            // Dome
            image = 28;
        break;
    case 108:
        if (IsPresent(17))
            // Natter energy egg
            image = 30;
        break;
    case 124: // Bio-Gem
    case 41:
        if (IsPresent(18))
            image = 29;
        break;
    case 21: // Killer Bees
        if (Items[24].Location == MyLoc)
            image = 31;
        break;
    case 83: // Iron ring
        if (Items[33].Location == MyLoc)
            image = 32;
        break;
    case 121: // Cage
        if (Items[47].Location == MyLoc)
            image = 33;
        break;
    default:
        break;
    }
    if (image)
        ShowCloseup(image);
}

void HulkLook(void)
{
    DrawImage(Rooms[MyLoc].Image);
    for (int ct = 0; ct <= GameHeader.NumItems; ct++) {
        int image = Items[ct].Image;
        if (Items[ct].Location == MyLoc && image != 255) {
            /* Don't draw bio gem in fuzzy area */
            if ((ct == 18 && MyLoc != 15) ||
                /* Don't draw Dr. Strange until outlet is plugged */
                (ct == 26 && Items[28].Location != MyLoc))
                continue;
            DrawImage(image);
        }
    }
}

void DrawHulkImage(int p)
{
    int image = 0;
    switch (p) {
    case 85:
        /* Hulk lifts dome */
        image = 34;
        break;
    case 86:
        /* Gas turns Hulk into Banner */
        image = 35;
        break;
    case 83:
        /* Chair breaks */
        image = 36;
        break;
    case 84:
        /* Banner becomes Hulk */
        image = 37;
        break;
    case 87:
        /* Victory: Chief Examiner and gems */
        image = 38;
        break;
    case 88:
        /* Hulk thinks about Nightmare */
        image = 39;
        break;
    case 89:
        /* Hulk digs */
        image = 40;
        break;
    case 82:
        /* Banner looks into mirror */
        image = 41;
        break;
    case 81:
        /* Hulk looks into mirror */
        image = 42;
        break;
    default:
        fprintf(stderr, "Unhandled image number %d!\n", p);
        break;
    }

    if (image != 0)
        ShowCloseup(image);
}
