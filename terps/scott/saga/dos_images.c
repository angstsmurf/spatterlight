/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  dos_images.c
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Loads the picture files of the MS-DOS version of Questprobe featuring
 *  The Incredible Hulk.
 */

#include <stdlib.h>
#include <string.h>

#include "sagagraphics.h"
#include "scott.h"

#include "dos_images.h"

/* DOS image filenames for Questprobe featuring The Incredible Hulk.
   (The only SAGA graphical game released for MS-DOS)
   Naming convention:
     "R01xx"  — Room image, where xx is the room number.
     "B01xxR" — Object image associated with a room, where xx is the object
     index.
     "B01xxI" — Object image for the inventory screen.
*/
/* clang-format off */

static const char *DOSFilenames[] =
  { "B01024R", "B01044I", "R0109",   "R0190",
    "B01001I", "B01025R", "B01045R", "R0112",  "R0191",
    "B01002I", "B01026R", "B01046I", "R0115",  "R0192",
    "B01003I", "B01029I", "B01048I", "R0116",  "R0193",
    "B01004I", "B01030I", "B01050I", "R0119",  "R0194",
    "B01012I", "B01031I", "B01051I", "R0120",  "R0195",
    "B01013R", "B01032I", "B01053R", "R0181",  "R0196",
    "B01016I", "B01033R", "B01070R", "R0182",  "R0197",
    "B01017R", "B01034R", "B01072R", "R0183",  "R0198",
    "B01018I", "B01035I", "R0184",   "R0199",  "B01019I",
    "B01036R", "R0100",   "R0185",   "B01020R","B01037I",
    "R0101",   "R0186",   "B01021I", "B01039R","R0102",
    "R0187",   "B01022R", "B01040R", "R0103",  "R0188",
    "B01023I", "B01042I", "R0104",   "R0189",  NULL };

/* clang-format on */

/* Load all DOS-format image files into a linked list of USImage nodes.
   Parses each filename to determine image type (room, room object,
   inventory object) and index number from the naming convention. */
int LoadDOSImages(void)
{
    USImages = NewImage();

    USImage *image = USImages;
    size_t datasize;
    int found = 0;

    for (int i = 0; DOSFilenames[i] != NULL; i++) {
        const char *shortname = DOSFilenames[i];
        uint8_t *data = FindImageFile(shortname, &datasize);
        if (data) {
            found++;
            image->datasize = datasize;
            image->imagedata = MemAlloc(datasize);
            memcpy(image->imagedata, data, datasize);
            free(data);
            image->systype = SYS_MSDOS;
            /* "R01xx" filenames are room images; extract 2-digit room index
               from positions 3-4 */
            if (shortname[0] == 'R') {
                image->usage = IMG_ROOM;
                image->index = (shortname[3] - '0') * 10 + shortname[4] - '0';
            } else {
                /* "B01xxR" or "B01xxI" — extract 2-digit object index from
                   positions 4-5 */
                image->index = (shortname[4] - '0') * 10 + shortname[5] - '0';
                if (shortname[6] == 'R') {
                    image->usage = IMG_ROOM_OBJ;
                } else {
                    image->usage = IMG_INV_OBJ;
                }
            }
            image = AppendImage(image);
        }
    }
    if (!found) {
        free(USImages);
        USImages = NULL;
        return 0;
    }
    return 1;
}
