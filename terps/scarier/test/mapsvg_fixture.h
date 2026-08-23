/* Hard-coded Main-page map fixture matching map.xml after REVEAL. */

#ifndef MAPSVG_FIXTURE_H
#define MAPSVG_FIXTURE_H

#include "../mapdraw.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fills *map and *view with static fixture data. Valid for the process lifetime.
   Player location key is "LocationHub". */
void mapsvg_fixture_build (map_t *map, map_view_t *view);

#ifdef __cplusplus
}
#endif

#endif
