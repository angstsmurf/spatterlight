//
//  game_specific.c
//  scott
//
//  Created by Administrator on 2022-01-27.
//

#include "game_specific.h"

#include "scott.h"

void secret_action(int p) {
    switch (p) {
        case 1:
            dead = 1;
            if (split_screen) {
                glk_window_close(Top, NULL);
                Top = NULL;
                MyLoc = 1;
                Rooms[1].Image = 25;
                DrawImage(25);
                Output(sys[HIT_ENTER]);
                HitEnter();
                DrawImage(26);
                Rooms[1].Image = 26;
            }
            break;
        case 2:
            DrawImage(24);
            if (Items[1].Location == CARRIED || Items[1].Location == MyLoc) {
                DrawImage(27);
            } else if ((Items[7].Location == CARRIED || Items[7].Location == MyLoc) &&
                       Items[8].Location != CARRIED && Items[8].Location != MyLoc &&
                       Items[41].Location != CARRIED && Items[41].Location != MyLoc){
                DrawImage(28);
            }
            Output(sys[HIT_ENTER]);
            HitEnter();
            break;
        case 3:
        case 4:
            DrawBlack();
            DrawImage(30);
            Output(sys[HIT_ENTER]);
            Output("\n");
            HitEnter();
            break;
        default:
            fprintf(stderr, "Secret Mission: Unsupported action parameter %d!\n", p);
            return;
    }
}

void adventureland_darkness(void) {
    if ((Rooms[MyLoc].Image & 128) == 128)
        BitFlags|=1<<DARKBIT;
    else
        BitFlags&=~(1<<DARKBIT);
}

void adventureland_action(int p) {
    int image = 0;
    switch (p) {
        case 1:
            image = 36;
            break;
        case 2:
            image = 34;
            break;
        case 3:
            image = 33;
            break;
        case 4:
            image = 35;
            break;
        default:
            fprintf(stderr, "Adventureland: Unsupported action parameter %d!\n", p);
            return;
    }
    DrawImage(image);
    Output("\n");
    Output(sys[HIT_ENTER]);
    HitEnter();
    return;
}
