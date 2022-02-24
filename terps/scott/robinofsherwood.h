//
//  robinofsherwood.h
//  scott
//
//  Created by Administrator on 2022-01-10.
//

#ifndef robinofsherwood_h
#define robinofsherwood_h

#include "definitions.h"
#include <stdio.h>

void update_robin_of_sherwood_animations(void);
void robin_of_sherwood_look(void);
GameIDType LoadExtraSherwoodData(void);
GameIDType LoadExtraSherwoodData64(void);
int is_forest_location(void);
void SherwoodAction(int p);

#endif /* robinofsherwood_h */
