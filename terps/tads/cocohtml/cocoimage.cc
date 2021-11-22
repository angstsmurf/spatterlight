/*
 * Images
 */

#include "cocohtml.h"

CHtmlSysResource * CHtmlSysImageJpeg :: 
create_jpeg(const CHtmlUrl *url, const textchar_t *filename, unsigned long seekpos, unsigned long filesize, CHtmlSysWin *win)
{
    printf("create_jpeg %s @ %d + %d\n", filename, seekpos, filesize);
    return 0;
}

CHtmlSysResource * CHtmlSysImagePng ::
create_png(const CHtmlUrl *url, const textchar_t *filename, unsigned long seekpos, unsigned long filesize, CHtmlSysWin *win)
{
    printf("create_png %s @ %d + %d\n", filename, seekpos, filesize);
    return 0;
}

CHtmlSysResource * CHtmlSysImageMng ::
create_mng(const CHtmlUrl *url, const textchar_t *filename, unsigned long seekpos, unsigned long filesize, CHtmlSysWin *win)
{
    printf("create_mng %s @ %d + %d\n", filename, seekpos, filesize);
    return 0;
}
