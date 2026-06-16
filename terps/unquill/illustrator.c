/* UnQuill: Disassemble and play games written with the "Quill" adventure game system
    Copyright (C) 1996,1999	John Elliott
    Copyright (C) 2005,		Tor Andersson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* The Illustrator location graphics: decoding and rendering of the vector
 * pictures that Gilsoft's "The Illustrator" attaches to Quill games, plus the
 * Glk graphics-window plumbing that displays them. Split out of unquill.c.
 * Shared globals/prototypes (zmem, zword, mainwin, statuswin, arch,
 * draw_location_graphic, pic_relayout) come from unquill.h; the ZX display-file
 * geometry and palette come from decompressz80.h. */

#include "unquill.h"
#include "decompressz80.h"

#ifdef SPATTERLIGHT
extern int gli_enable_graphics;
#endif

/* --- The Illustrator location graphics ---------------------------------- *
 * Quill games drawn with Gilsoft's "The Illustrator" carry a vector picture
 * per location: a list of drawing commands (plot, line, fill, ...) that the
 * runtime walks to paint the screen. We decode that command list (the same
 * format UnQuill's listgfx() prints) into an offscreen framebuffer and blit it
 * to a graphics window above the text. The command set and the 0xFAB8 pointer
 * block were confirmed against Bored of the Rings by disassembling its renderer
 * (an 8-way jump table at 0xfbef, low 3 bits of each command = opcode). */

#define ILL_W    256
#define ILL_H    176	/* ZX plot area; y is measured up from the bottom */
#define ILL_CX   (ILL_W / 8)	/* attribute cells across (32) */
#define ILL_CY   (ILL_H / 8)	/* attribute cells down (22) */
#define ILL_BASE 0xFAB8	/* Spectrum Illustrator graphics pointer block */

/* We render the picture into a genuine Spectrum display file - a 6912-byte
 * buffer laid out exactly as the hardware's screen memory: a 6144-byte 1bpp
 * bitmap in the non-linear display order, followed by a 768-byte grid of 8x8
 * attribute cells (ink, paper, bright). Drawing this way (rather than a private
 * colour-per-pixel buffer) lets the picture share the loading-screen blit and,
 * crucially, lets the fill be a faithful port of the ROM routine that walks the
 * same memory. The blit composes bitmap and attributes like the hardware: a set
 * bit shows ink, a clear bit shows paper. */
static winid_t picwin = NULL;
static uchar  *picscr = NULL;	/* ZX_SCREEN_SIZE display file (bitmap + attrs) */
static int     pic_scale = 1, pic_xoff = 0, pic_yoff = 0;

/* Split-screen detection. Fullscreen Illustrator games paint the whole 176-pixel
 * plot area; later "split screen" games (e.g. Bugsy) only draw into a fixed band
 * at the top and leave the rest for text, which would otherwise render as a black
 * lower half. We measure how deep each picture actually draws (ill_bot_sy, the
 * lowest touched screen row of the current picture) and keep the session's
 * running maximum, rounded up to a character cell, as the window height
 * (ill_draw_h). It starts at 0 ("unknown" -> treat as full height) and only ever
 * grows, so the graphics window settles at the game's true picture-area height
 * without flicking shorter for sparsely drawn rooms. */
static int     ill_bot_sy = -1;	/* deepest screen row drawn by current picture */
static int     ill_draw_h = 0;	/* picture-area height in pixels (0 = unknown) */

static void ill_track(int y)	/* y is a plot row (0 = bottom) */
{
    int sy = ILL_H - 1 - y;	/* convert to a top-down screen row */
    if (sy >= 0 && sy < ILL_H && sy > ill_bot_sy)
	ill_bot_sy = sy;
}

/* The Illustrator plots in a 256x176 area with y measured up from the bottom;
 * the Spectrum screen is addressed top-down. These map a plot coordinate to a
 * display-file bitmap byte/bit and to its attribute byte. */
static int ill_bmoff(int x, int y)	/* y is a top-down screen row 0..191 */
{
    return ZXBitmapOffset(y, x >> 3);
}
static int ill_getpix(int x, int y)	/* y is a plot row (0 = bottom) */
{
    int sy = ILL_H - 1 - y;
    return (picscr[ill_bmoff(x, sy)] >> (7 - (x & 7))) & 1;
}
static void ill_putpix(int x, int y, int set)
{
    int sy = ILL_H - 1 - y, off = ill_bmoff(x, sy), bit = 0x80 >> (x & 7);
    if (set) picscr[off] |= (uchar)bit;
    else     picscr[off] &= (uchar)~bit;
    if (set) ill_track(y);
}
static uchar *ill_attrp(int x, int y)
{
    int sy = ILL_H - 1 - y;
    return &picscr[ZX_BITMAP_SIZE + (sy >> 3) * ZX_SCREEN_COLS + (x >> 3)];
}

/* Pen / attribute state while drawing one picture. */
static int pen_x, pen_y, pen_ink, pen_paper, pen_bright;

/* Per-component "transparent" flags, mirroring the Spectrum's MASK_T: INK/PAPER/
 * BRIGHT 8 means "leave this component of the cell unchanged". When a flag is
 * set, drawing into a cell keeps that component of the cell's existing
 * attribute (e.g. the Fag End recess fill, drawn after PAPER 8, keeps the white
 * wall paper and only lays down black ink). */
static int pen_ink_t, pen_paper_t, pen_bright_t;


/* GOSUB scale factor (the command's sc field, 0 = unscaled). When non-zero the
 * relative deltas of the sub-picture's commands are multiplied by sc/8, exactly
 * as the Spectrum's coordinate routine (0xff5c -> 0xff7c, which multiplies by
 * sc then shifts right three times) does, so scaled-down repeated sub-pictures
 * (e.g. the trees) come out the right size. */
static int ill_sc = 0;
static int ill_scaled(int v) { return ill_sc ? (v * ill_sc) >> 3 : v; }

/* The Illustrator keeps the pen position as a byte pair and the screen-address
 * arithmetic wraps it: x is taken modulo 256 and y modulo the 176-row plot area
 * (ILL_H). Relative moves therefore wrap rather than running off the edge - e.g.
 * Bugsy's title draws five bullets by repeatedly GOSUBbing one sub-picture whose
 * first move is +160 in y; without the wrap each bullet climbs another ~161 rows
 * and only the first lands on screen, but mod-176 brings them back into a tidy
 * descending diagonal exactly as the Spectrum does. */
static int ill_wrapx(int x) { return x & (ILL_W - 1); }		/* ILL_W is 256 */
static int ill_wrapy(int y) { y %= ILL_H; if (y < 0) y += ILL_H; return y; }

/* Unit moves for RPLOT, indexed by bits 5-7 of the command byte (dx, dy with
 * dy measured up). The ROM's RPLOT handler (0xfd9f) reads a {ymove, dx} table
 * at 0xfddf in this order - direction 0 is straight up, then clockwise: N, NE,
 * E, SE, S, SW, W, NW. (An earlier table here started at E and ran the wrong
 * way round, which made every RPLOT step a step off, drifting figures by one
 * pixel and breaking flood-fill seals - e.g. Bugsy's "rough part of town".) */
static const int ill_rmoves[8][2] = {
    {0,1}, {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}
};

/* Stamp the current pen attribute onto the cell containing pixel (x,y), leaving
 * any component flagged transparent (INK/PAPER/BRIGHT 8) at its existing value -
 * exactly as the ROM's PO-ATTR combines ATTR_T through MASK_T. */
static void ill_setcell(int x, int y)
{
    uchar *a, v;
    x = ill_wrapx(x); y = ill_wrapy(y);
    a = ill_attrp(x, y);
    v = *a;
    if (!pen_ink_t)    v = (uchar)((v & ~0x07) | (pen_ink & 7));
    if (!pen_paper_t)  v = (uchar)((v & ~0x38) | ((pen_paper & 7) << 3));
    if (!pen_bright_t) v = (uchar)((v & ~0x40) | (pen_bright << 6));
    *a = v;
    ill_track(y);
}

/* Set (ink) or clear (paper) one pixel and stamp its cell's attribute. */
static void ill_plot(int x, int y, int set)
{
    x = ill_wrapx(x); y = ill_wrapy(y);
    ill_putpix(x, y, set);
    ill_setcell(x, y);
}

/* Draw a line exactly as the ZX ROM's DRAW-LINE routine (0x24BA) does, since
 * that is what The Illustrator calls: step the major axis every pixel, step the
 * minor axis when an error accumulator (initialised to major/2) overflows. This
 * reproduces the ROM's pixel choices so flood fills bound identically to the
 * original. The start point is assumed already plotted (as on the Spectrum).
 *
 * The two axes are symmetric, so rather than duplicate the loop we swap x<->y
 * for steep lines and run a single x-major loop, swapping back at the plot.
 * After the swap adx/ix/a are the major axis and ady/iy/b the minor; `steep`
 * keeps the tie-break (adx == ady -> not steep -> x-major) and makes the
 * adx == 0 test fire only on a true zero-length segment. */
static void ill_line(int x0, int y0, int x1, int y1)
{
    int adx = abs(x1 - x0), ady = abs(y1 - y0);
    int ix = (x1 > x0) ? 1 : -1, iy = (y1 > y0) ? 1 : -1;
    int t, err, k, a, b;

    int steep = ady > adx;		/* y is the major axis */
    if (steep) { t = x0; x0 = y0; y0 = t;  t = adx; adx = ady; ady = t;  t = ix; ix = iy; iy = t; }
    if (adx == 0)			/* adx == ady == 0: plot nothing */
	return;
    err = adx >> 1; a = x0; b = y0;
    for (k = 0; k < adx; k++)
    {
	err += ady;
	a += ix;
	if (err >= adx) { err -= adx; b += iy; }
	if (steep) ill_plot(b, a, 1);
	else       ill_plot(a, b, 1);
    }
}

/* Flood the connected region of clear pixels reachable from the seed, collecting
 * it into ill_region. Bounded only by set (ink) pixels - exactly as the ROM fill
 * (0xfc6c), which scans the bitmap and stops at set bits. Returns 0 (nothing to
 * do) if the seed itself is on a set pixel, as the ROM does. */
static uchar ill_region[ILL_W * ILL_H];
static int ill_flood(int x, int y)
{
    static int stack[ILL_W * ILL_H];
    int sp = 0;

    if (x < 0 || x >= ILL_W || y < 0 || y >= ILL_H)
	return 0;
    if (ill_getpix(x, y))		/* seed on a set pixel: nothing to do */
	return 0;

    memset(ill_region, 0, ILL_W * ILL_H);
    stack[sp++] = y * ILL_W + x;
    while (sp)
    {
	int p = stack[--sp];
	int px = p % ILL_W, py = p / ILL_W;
	if (ill_region[p] || ill_getpix(px, py))
	    continue;
	ill_region[p] = 1;
	if (px > 0         && !ill_region[p-1]     && !ill_getpix(px-1, py)   && sp < ILL_W*ILL_H-4) stack[sp++] = p-1;
	if (px < ILL_W - 1 && !ill_region[p+1]     && !ill_getpix(px+1, py)   && sp < ILL_W*ILL_H-4) stack[sp++] = p+1;
	if (py > 0         && !ill_region[p-ILL_W] && !ill_getpix(px,   py-1) && sp < ILL_W*ILL_H-4) stack[sp++] = p-ILL_W;
	if (py < ILL_H - 1 && !ill_region[p+ILL_W] && !ill_getpix(px,   py+1) && sp < ILL_W*ILL_H-4) stack[sp++] = p+ILL_W;
    }
    return 1;
}

/* FILL: flood the connected clear region and, exactly as the ROM does, set each
 * of its pixels (ink) while stamping the current attribute onto every cell it
 * touches. The pixel-level boundary gives smooth edges; the door interior stays
 * clear because its planks (set pixels drawn first) wall it off from the fill. */
static void ill_fill(int x, int y)
{
    int i;
    if (!ill_flood(x, y))
	return;
    for (i = 0; i < ILL_W * ILL_H; i++)
	if (ill_region[i])
	{
	    ill_setcell(i % ILL_W, i / ILL_W);
	    ill_putpix(i % ILL_W, i / ILL_W, 1);
	}
}

/* SHADE: flood the clear region, stamp the attribute, then set the pixels whose
 * dither bit is on. Each pixel picks bit ((x&1)<<2 | ((x>>1)&1)<<1 | (y&1)) of
 * the 8-bit pattern (the SHADE handler at 0xfe92), so set=ink shows through the
 * paper as a two-tone dither. */
static void ill_shade(int x, int y, int pattern)
{
    int i;
    if (!ill_flood(x, y))
	return;
    for (i = 0; i < ILL_W * ILL_H; i++)
	if (ill_region[i])
	{
	    int px = i % ILL_W, py = i / ILL_W;
	    int idx = ((px & 1) << 2) | (((px >> 1) & 1) << 1) | (py & 1);
	    ill_setcell(px, py);
	    if ((pattern >> idx) & 1)
		ill_putpix(px, py, 1);
	}
}

/* Decode and render one picture (a list of commands at gptr) into the bitmap.
 * `depth` guards against runaway GOSUB recursion. */
static void ill_render(ushort gfx_ptrs, ushort gptr, int depth)
{
    uchar gflag;
    int op, value, nargs, a0, a1;

    if (depth > 8)
	return;

    do
    {
	gflag = zmem(gptr);
	op    = gflag & 7;
	value = gflag >> 3;
	nargs = 0;

	switch (op)
	{
	case 0:	/* PLOT x,y  (over+inverse => AMOVE, absolute move only) */
	    nargs = 2;
	    pen_x = ill_wrapx(zmem(gptr + 1));
	    pen_y = ill_wrapy(zmem(gptr + 2));
	    if (!((gflag & 0x08) && (gflag & 0x10)))
		ill_plot(pen_x, pen_y, 1);
	    break;

	case 1:	/* LINE dx,dy  (over+inverse => MOVE only) */
	    nargs = 2;
	    a0 = ill_scaled(zmem(gptr + 1)) * ((gflag & 0x40) ? -1 : 1);
	    a1 = ill_scaled(zmem(gptr + 2)) * ((gflag & 0x80) ? -1 : 1);
	    if ((gflag & 0x08) && (gflag & 0x10))
	    {
		pen_x += a0; pen_y += a1;	/* MOVE */
	    }
	    else
	    {
		ill_line(pen_x, pen_y, pen_x + a0, pen_y + a1);
		pen_x += a0; pen_y += a1;
	    }
	    pen_x = ill_wrapx(pen_x); pen_y = ill_wrapy(pen_y);
	    break;

	case 2:	/* FILL / BLOCK / SHADE / BSHADE */
	    switch (gflag & 0x30)
	    {
	    case 0x00:	/* FILL: flood at pen + (dx,dy). Does NOT move the pen -
			 * the ROM computes COORDS +/- args but never stores it back. */
		nargs = 2;
		a0 = ill_scaled(zmem(gptr + 1)) * ((gflag & 0x40) ? -1 : 1);
		a1 = ill_scaled(zmem(gptr + 2)) * ((gflag & 0x80) ? -1 : 1);
		ill_fill(ill_wrapx(pen_x + a0), ill_wrapy(pen_y + a1));
		break;
	    case 0x10:	/* BLOCK: recolour a rectangle of attribute cells. The ROM's
			 * handler (0xfd1a) writes only the attribute file, never the
			 * bitmap, so ink already plotted survives - e.g. the death
			 * screen washes the picture red over the ringwraith, which
			 * stays black on the new red paper. */
	    {
		/* args: height, width, start col, start row (all in 8x8 cells,
		 * rows numbered top-down as on the Spectrum's attribute file). */
		int bh = zmem(gptr + 1), bw = zmem(gptr + 2);
		int bcol = zmem(gptr + 3), brow = zmem(gptr + 4);
		int row, col;
		nargs = 4;
		for (row = brow; row <= brow + bh; row++)
		    for (col = bcol; col <= bcol + bw; col++)
			ill_setcell(col * 8, 175 - row * 8);
		break;
	    }
	    default:	/* SHADE / BSHADE: dither at pen + (dx,dy), again without
			 * moving the pen (so a run of shades stays anchored to the
			 * preceding MOVE rather than drifting). */
		nargs = 3;
		a0 = ill_scaled(zmem(gptr + 1)) * ((gflag & 0x40) ? -1 : 1);
		a1 = ill_scaled(zmem(gptr + 2)) * ((gflag & 0x80) ? -1 : 1);
		ill_shade(ill_wrapx(pen_x + a0), ill_wrapy(pen_y + a1), zmem(gptr + 3));
		break;
	    }
	    break;

	case 3:	/* GOSUB: draw sub-picture at scale sc. The pen is NOT restored
		 * afterwards - as on the Spectrum, the sub-picture leaves it at
		 * its last point, and the parent's next MOVE is relative to that. */
	{
	    int sub = zmem(gptr + 1), old_sc = ill_sc;
	    nargs = 1;
	    ill_sc = value & 7;
	    ill_render(gfx_ptrs, zword(gfx_ptrs + 2 * sub), depth + 1);
	    ill_sc = old_sc;
	    break;
	}

	case 4:	/* RPLOT: plot one step in a direction */
	    nargs = 0;
	    pen_x = ill_wrapx(pen_x + ill_rmoves[(value >> 2) & 7][0]);
	    pen_y = ill_wrapy(pen_y + ill_rmoves[(value >> 2) & 7][1]);
	    if (!((gflag & 0x08) && (gflag & 0x10)))
		ill_plot(pen_x, pen_y, 1);
	    break;

	case 5:	/* PAPER / BRIGHT. The colour value is 0-7 to set, 8 = transparent
		 * (leave unchanged), 9 = contrast; the engine forms it the same way
		 * the ZX PRINT control code does, so honour 8 instead of masking it
		 * down to 0 (PAPER 8 is how a fill keeps the paper it inherited). */
	    nargs = 0;
	    {
		int v = value & 0x0F;
		if (gflag & 0x80) { pen_bright_t = (v == 8); if (v < 8) pen_bright = v & 1; }
		else              { pen_paper_t  = (v == 8); if (v < 8) pen_paper  = v;     }
	    }
	    break;

	case 6:	/* INK / FLASH (flash ignored); INK 8 is transparent, as above. */
	    nargs = 0;
	    if (!(gflag & 0x80))
	    {
		int v = value & 0x0F;
		pen_ink_t = (v == 8);
		if (v < 8) pen_ink = v;
	    }
	    break;

	case 7:	/* END */
	    nargs = 0;
	    break;
	}

	gptr += nargs + 1;
    }
    while (op != 7);
}

/* Recompute scaling/centring of the picture window, and crop the window's
 * height so the image fills it exactly (no empty vertical band above or below
 * the picture). */
static void pic_layout(void)
{
    glui32 w = 0, h = 0;
    int sx, sy, target_h;
    int dh = (ill_draw_h > 0) ? ill_draw_h : ILL_H;	/* drawn picture height */
    winid_t parent;

    glk_window_get_size(picwin, &w, &h);
    sx = (int)w / ILL_W;
    sy = (int)h / dh;
    pic_scale = (sx < sy) ? sx : sy;
    if (pic_scale < 1)
	pic_scale = 1;

    /* Resize the window's height to the picture's exact rendered height (which,
     * for a split-screen game, is only the top band it actually draws into). */
    target_h = dh * pic_scale;
    parent = glk_window_get_parent(picwin);
    if (parent && target_h > 0 && (int)h != target_h)
    {
	glk_window_set_arrangement(parent,
	    winmethod_Above | winmethod_Fixed, (glui32)target_h, picwin);
	glk_window_get_size(picwin, &w, &h);
	/* The arrangement may have been clamped; honour the actual size. */
	sx = (int)w / ILL_W;
	sy = (int)h / dh;
	pic_scale = (sx < sy) ? sx : sy;
	if (pic_scale < 1)
	    pic_scale = 1;
    }

    pic_xoff = ((int)w - ILL_W * pic_scale) / 2;
    pic_yoff = ((int)h - dh * pic_scale) / 2;
    if (pic_xoff < 0) pic_xoff = 0;
    if (pic_yoff < 0) pic_yoff = 0;
}

/* Repaint the picture after a window resize (evtype_Arrange) or a redraw
 * request (evtype_Redraw). The Fixed cropping we leave behind on pic_layout
 * makes the window stop growing when the parent does, so we first reset the
 * arrangement to the proportional default to let the layout pick up the
 * parent's new available space, then re-layout (which crops down) and re-blit
 * from the stored bitmap. Mirrors the Updates() / Look() flow used by the
 * Scott, Taylor and Plus interpreters. */
static void pic_blit(void);

void pic_relayout(void)
{
    winid_t parent;
    if (!picwin || !picscr)
	return;
    parent = glk_window_get_parent(picwin);
    if (parent)
	glk_window_set_arrangement(parent,
	    winmethod_Above | winmethod_Proportional, 55, picwin);
    pic_layout();
    pic_blit();
}

/* Resolve one pixel to a palette index by combining its bit with its cell's
 * attribute: a set bit shows ink, a clear bit shows paper, both shifted into
 * the bright half of the palette when the cell's bright flag is set. */
static int pic_pixel(int x, int y)
{
    uchar a = *ill_attrp(x, y);
    int bright = (a & 0x40) ? 8 : 0;
    return ill_getpix(x, y)
	 ? ((a & 7) + bright)		/* ink */
	 : (((a >> 3) & 7) + bright);	/* paper */
}

/* Blit the bitmap+attributes to the picture window (run-length per row; ZX y
 * is flipped). */
static void pic_blit(void)
{
    int sy, x;
    int dh = (ill_draw_h > 0) ? ill_draw_h : ILL_H;	/* drawn picture height */
    glui32 bg;
    if (!picwin || !picscr)
	return;
    /* The area outside the picture (margins and any band the image does not
     * fill) should match the text window's Normal background rather than a
     * fixed colour, as the Scott, Level9 and Magnetic interpreters do. Re-measure
     * on every blit so it tracks light/dark theme changes (a Redraw event comes
     * through pic_relayout -> pic_blit). */
    if (mainwin && glk_style_measure(mainwin, style_Normal, stylehint_BackColor, &bg))
	glk_window_set_background_color(picwin, bg);
    glk_window_clear(picwin);
    for (sy = 0; sy < dh; sy++)	/* only the band the picture draws into */
    {
	int y = ILL_H - 1 - sy;
	x = 0;
	while (x < ILL_W)
	{
	    int c = pic_pixel(x, y);
	    int start = x;
	    while (x < ILL_W && pic_pixel(x, y) == c)
		x++;
	    glk_window_fill_rect(picwin, ZXPalette[c],
		pic_xoff + start * pic_scale, pic_yoff + sy * pic_scale,
		(x - start) * pic_scale, pic_scale);
	}
    }
}

/* Draw the picture for location `loc`, if it has one, into a graphics window
 * above the text. Returns silently when graphics are unavailable or the game
 * has no Illustrator data. */
void draw_location_graphic(uchar loc)
{
    ushort gfx_ptrs, gfx_flags, gfx_count, gptr;
    uchar gflag;

    if (arch != ARCH_SPECTRUM)
	return;
#ifdef SPATTERLIGHT
    if (!gli_enable_graphics)
	return;
#endif
    if (!glk_gestalt(gestalt_Graphics, 0))
	return;

    gfx_ptrs  = zword(ILL_BASE);
    gfx_flags = zword(ILL_BASE + 2);
    gfx_count = zmem(ILL_BASE + 6);
    if (gfx_count == 0 || loc >= gfx_count)
	return;
    /* Sanity-check the pointer block before trusting it. */
    if (gfx_ptrs < 0x5C00 || gfx_flags < 0x5C00)
	return;

    gptr = zword(gfx_ptrs + 2 * loc);

    /* A location with no picture has a record that is just END; tear down any
     * window left from a previous location so the text reclaims that space and
     * a subsequent resize cannot redraw the stale bitmap. */
    if ((zmem(gptr) & 7) == 7)
    {
	if (picwin)
	{
	    glk_window_close(picwin, NULL);
	    picwin = NULL;
	}
	return;
    }

    if (!picscr)
    {
	picscr = malloc(ZX_SCREEN_SIZE);
	if (!picscr)
	    return;
    }
    if (!picwin)
    {
	/* Put the picture at the very top, above the description grid (Scott /
	 * TaylorMade style): split the status window's parent so the picture
	 * sits over the whole [description grid + text buffer] stack rather
	 * than between them. */
	winid_t split = (statuswin && glk_window_get_parent(statuswin))
		      ? glk_window_get_parent(statuswin) : mainwin;
	picwin = glk_window_open(split,
		winmethod_Above | winmethod_Proportional, 55,
		wintype_Graphics, 0);
	if (!picwin)
	    return;
    }

    /* The picture starts from the location's flag attribute: ink and paper come
     * from it, but not bright (pictures render without bright unless a BRIGHT
     * command turns it on). INK/PAPER/BRIGHT commands change these as it draws. */
    gflag = zmem(gfx_flags + loc);
    pen_ink    = gflag & 7;
    pen_paper  = (gflag >> 3) & 7;
    pen_bright = 0;
    pen_ink_t = pen_paper_t = pen_bright_t = 0;
    pen_x = 0; pen_y = 0;
    ill_sc = 0;

    /* Clear the bitmap and set every cell to the location's initial attribute,
     * so the picture starts as a blank field of its paper colour. */
    memset(picscr, 0, ZX_BITMAP_SIZE);
    memset(picscr + ZX_BITMAP_SIZE,
	   (uchar)(pen_ink | (pen_paper << 3) | (pen_bright << 6)),
	   ZX_SCREEN_SIZE - ZX_BITMAP_SIZE);

    ill_bot_sy = -1;			/* measure how deep this picture draws */
    ill_render(gfx_ptrs, gptr, 0);
    if (ill_bot_sy >= 0)
    {
	/* Grow the picture-area height to the deepest row drawn so far, rounded
	 * up to a character cell. Monotonic, so a split-screen game settles on
	 * its fixed top band and a fullscreen game expands to the full height. */
	int h = ((ill_bot_sy >> 3) + 1) << 3;
	if (h > ILL_H) h = ILL_H;
	if (h > ill_draw_h) ill_draw_h = h;
    }

    pic_layout();
    pic_blit();
}
