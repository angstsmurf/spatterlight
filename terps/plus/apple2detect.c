//
//  apple2detect.c
//  plus
//
//  Created by Administrator on 2022-08-03.
//

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "definitions.h"

#include "apple2detect.h"

size_t writeToFile(const char *name, uint8_t *data, size_t size);

int DetectApple2(uint8_t **sf, size_t *extent)
{
    const size_t dsk_image_size = 35 * 16 * 256;
    if (*extent > MAX_LENGTH || *extent < dsk_image_size)
        return 0;

    uint8_t *new = MemAlloc(dsk_image_size);
    size_t offset = dsk_image_size - 256;
    for (int i = 0; i < *extent && i < dsk_image_size; i += 256) {
        memcpy(new + offset, *sf + i, 256);
        offset -= 256;
    }

    int actionsize = new[125490] + (new[125491] << 8);
    debug_print("Actionsize: %d\n", actionsize);
    if (actionsize < 4000 || actionsize > 7000) {
        free(new);
        return 0;
    }

    writeToFile("/Users/administrator/Desktop/Apple2ReorderedData", new, dsk_image_size);

    size_t datasize = dsk_image_size - 125438;
    uint8_t *gamedata = MemAlloc(datasize);
    memcpy(gamedata, new + 125438, datasize);



    if (gamedata) {
        free(*sf);
        *sf = gamedata;
        *extent = datasize;
        CurrentSys = SYS_APPLE2;
        Images = MemAlloc(sizeof(imgrec));
        Images[0].filename = NULL;
        return 1;
    }

    return 0;
}

