//
//  utility.c
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-03-10.
//

#include <stdlib.h>
#include <stdio.h>

#include "glk.h"
#include "taylor.h"

#include "common_file_utils.h"
#include "utility.h"

strid_t Transcript = NULL;

extern winid_t Bottom, Top;

GLK_ATTRIBUTE_NORETURN void CleanupAndExit(void)
{
    if (Transcript)
        glk_stream_close(Transcript, NULL);
    glk_exit();
}

uint8_t *SeekToPos(size_t offset)
{
    if (offset > FileImageLen || FileImage == NULL)
        return NULL;
    return FileImage + offset;
}

int rotate_left_with_carry(uint8_t *byte, int last_carry)
{
    int carry = ((*byte & 0x80) > 0);
    *byte = *byte << 1;
    if (last_carry)
        *byte = *byte | 0x01;
    return carry;
}

int rotate_right_with_carry(uint8_t *byte, int last_carry)
{
    int carry = ((*byte & 0x01) > 0);
    *byte = *byte >> 1;
    if (last_carry)
        *byte = *byte | 0x80;
    return carry;
}
