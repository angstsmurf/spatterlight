/*
 * Sound and music.
 */

#include "cocohtml.h"

CHtmlSysResource * CHtmlSysSoundOgg ::
create_ogg(const CHtmlUrl *url, const textchar_t *filename, unsigned long seekpos, unsigned long filesize, CHtmlSysWin *win)
{
    printf("create_ogg %s @ %d + %d\n", filename, seekpos, filesize);
    return 0;
}

CHtmlSysResource * CHtmlSysSoundWav ::
create_wav(const CHtmlUrl *url, const textchar_t *filename, unsigned long seekpos, unsigned long filesize, CHtmlSysWin *win)
{
    printf("create_wav %s @ %d + %d\n", filename, seekpos, filesize);
    return 0;
}

CHtmlSysResource * CHtmlSysSoundMidi ::
create_midi(const CHtmlUrl *url, const textchar_t *filename, unsigned long seekpos, unsigned long filesize, CHtmlSysWin *win)
{
    printf("create_midi %s @ %d + %d\n", filename, seekpos, filesize);
    return 0;
}

CHtmlSysResource * CHtmlSysSoundMpeg ::
create_mpeg(const CHtmlUrl *url, const textchar_t *filename, unsigned long seekpos, unsigned long filesize, CHtmlSysWin *win)
{
    printf("create_mpeg %s @ %d + %d\n", filename, seekpos, filesize);
    return 0;
}

