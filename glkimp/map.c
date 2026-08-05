/* Map document extension (gestalt_Map). SVG/image present + overlays. */

#include "glkimp.h"
#include "protocol.h"

#include <stdlib.h>
#include <string.h>

/* One-shot map event request; cleared on deliver / cancel / close. */
int gli_map_event_request = FALSE;

#define MAP_MAX_OVERLAYS 64
#define MAP_MAX_HYPERLINKS 64
#define MAP_MAX_POINTS_PER_LINK 32

typedef struct map_overlay_slot_struct {
    int used;
    overlayid_t id;
} map_overlay_slot_t;

static map_overlay_slot_t map_overlays[MAP_MAX_OVERLAYS];
static overlayid_t map_next_overlay_id = 1;

static int svg_looks_valid(const unsigned char *data, glui32 len)
{
    glui32 i;
    if (data == NULL || len < 4)
        return FALSE;
    for (i = 0; i + 4 <= len; i++) {
        if (data[i] == '<' &&
            (data[i + 1] == 's' || data[i + 1] == 'S') &&
            (data[i + 2] == 'v' || data[i + 2] == 'V') &&
            (data[i + 3] == 'g' || data[i + 3] == 'G'))
            return TRUE;
    }
    return FALSE;
}

static int png_looks_valid(const unsigned char *data, glui32 len)
{
    /* 89 50 4E 47 0D 0A 1A 0A */
    if (data == NULL || len < 8)
        return FALSE;
    return data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G'
        && data[4] == 0x0d && data[5] == 0x0a && data[6] == 0x1a && data[7] == 0x0a;
}

static int jpeg_looks_valid(const unsigned char *data, glui32 len)
{
    /* SOI + marker: FF D8 FF */
    if (data == NULL || len < 3)
        return FALSE;
    return data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff;
}

/* Wire blob for MAPPRESENT:
     u32 format           (mapimage_*)
     u32 bgcolor          (mapcolor_Default = use SVG/window default)
     u32 data_len
     u8  data[data_len]   (UTF-8 SVG, or PNG/JPEG bytes)
     u32 nhyperlinks
     repeated:
       u32 id
       u32 npoints          (>= 3)
       i32 x,y * npoints
       u32 label_len
       u8  label[label_len]

   MAPPRESENTIMAGE buffer:
     u32 bgcolor
     u32 image_id
     u32 nhyperlinks
     ... same hyperlink packing ...

   MAPSETHYPERLINKS buffer:
     u32 nhyperlinks
     ... same hyperlink packing ...

   MAPOVERLAY buffer:
     u32 height
     u32 zindex
     u32 link_id          (0 = not a hotspot)
     u32 linklabel_len
     u8  linklabel[linklabel_len] (UTF-8)
 */
static int map_pack_write_u32(unsigned char *dst, size_t cap, size_t *off, glui32 v)
{
    if (*off + 4 > cap)
        return FALSE;
    dst[*off + 0] = (unsigned char)(v & 0xff);
    dst[*off + 1] = (unsigned char)((v >> 8) & 0xff);
    dst[*off + 2] = (unsigned char)((v >> 16) & 0xff);
    dst[*off + 3] = (unsigned char)((v >> 24) & 0xff);
    *off += 4;
    return TRUE;
}

static int map_pack_write_i32(unsigned char *dst, size_t cap, size_t *off, glsi32 v)
{
    return map_pack_write_u32(dst, cap, off, (glui32)v);
}

static int map_pack_write_bytes(unsigned char *dst, size_t cap, size_t *off,
                                const void *src, size_t n)
{
    if (*off + n > cap)
        return FALSE;
    if (n)
        memcpy(dst + *off, src, n);
    *off += n;
    return TRUE;
}

static int map_hyperlink_is_valid(const glk_maphyperlink_t *h)
{
    if (h == NULL)
        return FALSE;
    if (h->id == 0)
        return FALSE;
    if (h->npoints < 3 || h->npoints > MAP_MAX_POINTS_PER_LINK)
        return FALSE;
    if (h->points == NULL)
        return FALSE;
    return TRUE;
}

/* Copy valid hyperlinks into dst[0..*nvalid); truncates at MAP_MAX_HYPERLINKS. */
static void map_filter_hyperlinks(const glk_maphyperlink_t *hyperlinks,
                                  glui32 nhyperlinks,
                                  glk_maphyperlink_t *dst,
                                  glui32 *nvalid)
{
    glui32 i;
    *nvalid = 0;
    if (hyperlinks == NULL)
        return;
    if (nhyperlinks > MAP_MAX_HYPERLINKS)
        nhyperlinks = MAP_MAX_HYPERLINKS;
    for (i = 0; i < nhyperlinks; i++) {
        if (map_hyperlink_is_valid(&hyperlinks[i]))
            dst[(*nvalid)++] = hyperlinks[i];
    }
}

static int map_pack_one_hyperlink(unsigned char *dst, size_t cap, size_t *off,
                                  const glk_maphyperlink_t *h)
{
    glui32 labellen = 0;
    glui32 p;

    if (h->label != NULL)
        labellen = (glui32)strlen(h->label);

    if (!map_pack_write_u32(dst, cap, off, h->id))
        return FALSE;
    if (!map_pack_write_u32(dst, cap, off, h->npoints))
        return FALSE;
    for (p = 0; p < h->npoints; p++) {
        if (!map_pack_write_i32(dst, cap, off, h->points[p].x))
            return FALSE;
        if (!map_pack_write_i32(dst, cap, off, h->points[p].y))
            return FALSE;
    }
    if (!map_pack_write_u32(dst, cap, off, labellen))
        return FALSE;
    if (!map_pack_write_bytes(dst, cap, off, h->label, labellen))
        return FALSE;
    return TRUE;
}

static int map_pack_hyperlinks(unsigned char *dst, size_t cap, size_t *off,
                               const glk_maphyperlink_t *links, glui32 nlinks)
{
    glui32 i;
    if (!map_pack_write_u32(dst, cap, off, nlinks))
        return FALSE;
    for (i = 0; i < nlinks; i++) {
        if (!map_pack_one_hyperlink(dst, cap, off, &links[i]))
            return FALSE;
    }
    return TRUE;
}

static void map_clear_overlays_local(void)
{
    glui32 i;
    for (i = 0; i < MAP_MAX_OVERLAYS; i++) {
        map_overlays[i].used = FALSE;
        map_overlays[i].id = 0;
    }
}

static void map_notify_clear_overlays(void)
{
    map_clear_overlays_local();
    win_flush();
    sendmsg_glk(MAPOVERLAYCLEARALL, 0, 0, 0, 0, 0, 0, NULL);
}

void glk_map_set_hyperlinks(const glk_maphyperlink_t *hyperlinks,
                            glui32 nhyperlinks)
{
    static unsigned char pack[GLKBUFSIZE];
    static glk_maphyperlink_t valid[MAP_MAX_HYPERLINKS];
    size_t off = 0;
    glui32 nvalid = 0;

    map_filter_hyperlinks(hyperlinks, nhyperlinks, valid, &nvalid);

    if (!map_pack_hyperlinks(pack, sizeof pack, &off,
                             nvalid ? valid : NULL, nvalid))
        return;

    win_flush();
    sendmsg_glk(MAPSETHYPERLINKS, 0, 0, 0, 0, 0, off, (char *)pack);
}

glui32 glk_map_present(glui32 format, const unsigned char *data, glui32 len,
    glui32 flags, glui32 bgcolor,
    glsi32 focusleft, glsi32 focustop,
    glui32 focuswidth, glui32 focusheight,
    const glk_maphyperlink_t *hyperlinks, glui32 nhyperlinks)
{
    static unsigned char pack[GLKBUFSIZE];
    static glk_maphyperlink_t valid[MAP_MAX_HYPERLINKS];
    size_t off = 0;
    glui32 nvalid = 0;

    if (format == mapimage_SVG) {
        if (!svg_looks_valid(data, len))
            return 0;
    } else if (format == mapimage_PNG) {
        if (!png_looks_valid(data, len))
            return 0;
    } else if (format == mapimage_JPEG) {
        if (!jpeg_looks_valid(data, len))
            return 0;
    } else {
        return 0;
    }

    map_filter_hyperlinks(hyperlinks, nhyperlinks, valid, &nvalid);

    if (!map_pack_write_u32(pack, sizeof pack, &off, format))
        return 0;
    if (!map_pack_write_u32(pack, sizeof pack, &off, bgcolor))
        return 0;
    if (!map_pack_write_u32(pack, sizeof pack, &off, len))
        return 0;
    if (!map_pack_write_bytes(pack, sizeof pack, &off, data, len))
        return 0;
    if (!map_pack_hyperlinks(pack, sizeof pack, &off,
                             nvalid ? valid : NULL, nvalid))
        return 0;

    map_notify_clear_overlays();

    win_flush();
    sendmsg_glk(MAPPRESENT, (int)flags, (int)focusleft, (int)focustop,
                (int)focuswidth, (int)focusheight, off, (char *)pack);
    return 1;
}

glui32 glk_map_present_image(glui32 image, glui32 flags, glui32 bgcolor,
    glsi32 focusleft, glsi32 focustop, glui32 focuswidth, glui32 focusheight,
    const glk_maphyperlink_t *hyperlinks, glui32 nhyperlinks)
{
    static unsigned char pack[GLKBUFSIZE];
    static glk_maphyperlink_t valid[MAP_MAX_HYPERLINKS];
    size_t off = 0;
    glui32 nvalid = 0;

    if (image == 0)
        return 0;
    if (!gli_loadimage((int)image))
        return 0;

    map_filter_hyperlinks(hyperlinks, nhyperlinks, valid, &nvalid);

    if (!map_pack_write_u32(pack, sizeof pack, &off, bgcolor))
        return 0;
    if (!map_pack_write_u32(pack, sizeof pack, &off, image))
        return 0;
    if (!map_pack_hyperlinks(pack, sizeof pack, &off,
                             nvalid ? valid : NULL, nvalid))
        return 0;

    map_notify_clear_overlays();

    win_flush();
    sendmsg_glk(MAPPRESENTIMAGE, (int)flags, (int)focusleft, (int)focustop,
                (int)focuswidth, (int)focusheight, off, (char *)pack);
    return 1;
}

overlayid_t glk_map_overlay(glui32 image, glsi32 left, glsi32 top,
                            glui32 width, glui32 height, glui32 zindex,
                            glui32 link_id, const char *linklabel)
{
    static unsigned char pack[GLKBUFSIZE];
    size_t off = 0;
    glui32 i;
    glui32 linklabellen = 0;
    overlayid_t id;
    int slot = -1;

    if (image == 0)
        return 0;
    if (!gli_loadimage((int)image))
        return 0;
    if (linklabel != NULL)
        linklabellen = (glui32)strlen(linklabel);

    if (!map_pack_write_u32(pack, sizeof pack, &off, height))
        return 0;
    if (!map_pack_write_u32(pack, sizeof pack, &off, zindex))
        return 0;
    if (!map_pack_write_u32(pack, sizeof pack, &off, link_id))
        return 0;
    if (!map_pack_write_u32(pack, sizeof pack, &off, linklabellen))
        return 0;
    if (linklabellen && !map_pack_write_bytes(pack, sizeof pack, &off, linklabel, linklabellen))
        return 0;

    for (i = 0; i < MAP_MAX_OVERLAYS; i++) {
        if (!map_overlays[i].used) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0)
        return 0;

    id = map_next_overlay_id++;
    if (id == 0)
        id = map_next_overlay_id++;

    map_overlays[slot].used = TRUE;
    map_overlays[slot].id = id;

    win_flush();
    sendmsg_glk(MAPOVERLAY, (int)id, (int)image, (int)left, (int)top, (int)width,
                off, (char *)pack);
    return id;
}

overlayid_t glk_map_fill_rect(glui32 color, glsi32 left, glsi32 top,
                              glui32 width, glui32 height, glui32 zindex)
{
    static unsigned char pack[8];
    size_t off = 0;
    glui32 i;
    overlayid_t id;
    int slot = -1;

    if (width == 0 || height == 0)
        return 0;

    if (!map_pack_write_u32(pack, sizeof pack, &off, height))
        return 0;
    if (!map_pack_write_u32(pack, sizeof pack, &off, zindex))
        return 0;

    for (i = 0; i < MAP_MAX_OVERLAYS; i++) {
        if (!map_overlays[i].used) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0)
        return 0;

    id = map_next_overlay_id++;
    if (id == 0)
        id = map_next_overlay_id++;

    map_overlays[slot].used = TRUE;
    map_overlays[slot].id = id;

    win_flush();
    /* a1=id a2=color a3=left a4=top a5=width; pack=height,zindex */
    sendmsg_glk(MAPFILLRECT, (int)id, (int)color, (int)left, (int)top, (int)width,
                off, (char *)pack);
    return id;
}

glui32 glk_map_overlay_move(overlayid_t overlay, glsi32 left, glsi32 top,
                            glui32 width, glui32 height, glui32 zindex)
{
    static unsigned char pack[4];
    size_t off = 0;
    glui32 i;
    int found = FALSE;

    if (overlay == 0)
        return 0;
    for (i = 0; i < MAP_MAX_OVERLAYS; i++) {
        if (map_overlays[i].used && map_overlays[i].id == overlay) {
            found = TRUE;
            break;
        }
    }
    if (!found)
        return 0;

    if (!map_pack_write_u32(pack, sizeof pack, &off, zindex))
        return 0;

    win_flush();
    sendmsg_glk(MAPOVERLAYMOVE, (int)overlay, (int)left, (int)top,
                (int)width, (int)height, off, (char *)pack);
    return 1;
}

glui32 glk_map_overlay_clear(overlayid_t overlay)
{
    glui32 i;
    int found = FALSE;

    if (overlay == 0)
        return 0;
    for (i = 0; i < MAP_MAX_OVERLAYS; i++) {
        if (map_overlays[i].used && map_overlays[i].id == overlay) {
            map_overlays[i].used = FALSE;
            map_overlays[i].id = 0;
            found = TRUE;
            break;
        }
    }
    if (!found)
        return 0;

    win_flush();
    sendmsg_glk(MAPOVERLAYCLEAR, (int)overlay, 0, 0, 0, 0, 0, NULL);
    return 1;
}

glui32 glk_map_overlay_clear_all(void)
{
    map_notify_clear_overlays();
    return 1;
}

void glk_map_close(void)
{
    gli_map_event_request = FALSE;
    map_clear_overlays_local();
    win_flush();
    sendmsg_glk(MAPCLOSE, 0, 0, 0, 0, 0, 0, NULL);
}

void glk_map_set_focus(glsi32 focusleft, glsi32 focustop,
                       glui32 focuswidth, glui32 focusheight)
{
    win_flush();
    sendmsg_glk(MAPFOCUS, (int)focusleft, (int)focustop,
                (int)focuswidth, (int)focusheight, 0, 0, NULL);
}

void glk_map_clear_focus(void)
{
    win_flush();
    sendmsg_glk(MAPCLEARFOCUS, 0, 0, 0, 0, 0, 0, NULL);
}

void glk_request_map_event(void)
{
    gli_map_event_request = TRUE;
    win_flush();
    sendmsg_glk(INITMAPEVENT, 0, 0, 0, 0, 0, 0, NULL);
}

void glk_cancel_map_event(void)
{
    gli_map_event_request = FALSE;
    win_flush();
    sendmsg_glk(CANCELMAPEVENT, 0, 0, 0, 0, 0, 0, NULL);
}
