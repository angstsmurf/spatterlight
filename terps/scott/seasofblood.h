//
//  seasofblood.h
//  Spatterlight
//
//  Created by Administrator on 2022-01-08.
//

#ifndef seasofblood_h
#define seasofblood_h

#include <stdio.h>

int LoadExtraSeasOfBloodData(void);
void seas_of_blood_room_image(void);
void adventure_sheet(void);
void blood_action(int p);

extern uint8_t *blood_image_data;

#endif /* seasofblood_h */
