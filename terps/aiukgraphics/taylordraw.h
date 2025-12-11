//
//  taylordraw.h
//  Spatterlight
//
//  Created by Administrator on 2025-12-10.
//

#ifndef taylordraw_h
#define taylordraw_h

typedef void(*draw_obj_fn)(uint8_t, uint8_t);

void InitTaylor(uint8_t *data, uint8_t *end, uint8_t *objloc, int older, int rebel, draw_obj_fn obj_draw);
int DrawTaylor(int loc, int current_location);

#endif // !taylordraw_h
