/*
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* aslxglk-map.cc -- the Quest 5 grid map (game.gridmap): a graphics band
 * across the top of the screen, driven by the Glk frontend (aslxglk.cc)
 * through aslxglk-map.hh.
 *
 * All map layout runs engine-side in CoreGrid.aslx (bundled in aslx-core):
 * as the player explores it computes room rectangles in abstract grid units,
 * per player object (that dictionary is the fog of war, and it round-trips
 * through saves as ordinary game state), and sends bare paint commands to
 * the UI -- a paper.js canvas (grid.js) in the reference player, the
 * grid_draw host hook here (GridDraw, aslx-runtime.hh).  So this file knows
 * nothing about rooms: it retains the paint commands in a display list and
 * rasterises them into a Glk graphics window at the top of the screen (a
 * root-window split, so the band sits above the banner like the reference
 * player's gridPanel sits above everything).  Glk's only drawing primitive
 * is the filled rectangle, so the picture is rendered into an RGB surface
 * and flushed as run-length spans of the rows that changed since the last
 * blit -- by the same rasteriser as the scarier map pane
 * (common_utils/rgbsurface.h).
 *
 * grid.js behaviour kept:
 *  - one layer per floor (the z each command carries); the layer of the
 *    last box/label/player command is "current" (activateLayer), lines land
 *    on it, and every other layer with content shows at 20% opacity, so
 *    other floors linger faintly under the one the player is on;
 *  - the player marker is a single circle that later commands move, drawn
 *    on top, its radius in pixels (not grid units); a move glides the
 *    marker from the old room to the new one and only then pans the view
 *    to recentre it -- grid.js's onFrame, the same two chained linear
 *    slides -- ticked on the shared Glk timer (aslxglk.cc's
 *    timer_event_second); deterministic mode jumps instead;
 *  - the view keeps the player centred (like the scarier map's
 *    LockPlayerCentre);
 *  - colours are authored HTML names / #rrggbb against the game's own
 *    background (setBackground paints #gridPanel too), so the canvas
 *    follows SetBackgroundColour rather than the Glk theme -- White for
 *    almost every game, but a game on black draws white exit lines.
 * Not carried over: pan/drag and zoom gestures, GridSquareClick, and the
 * custom-layer vocabulary (Grid_DrawSquare/Svg/Image/Shape...), which the
 * engine ignores for now.
 */

#include "aslxglk-map.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "../../common_utils/rgbsurface.h"
#include "../questglk-common.hh"

/* Spatterlight's "slow draw / real-time delays" preference (glkimp): off in
 * deterministic mode, where the marker jumps instead of gliding. */
extern "C" int gli_sa_delays;

using aslx::GridDraw;

winid_t gmapwin = nullptr;

namespace {

std::vector<GridDraw> g_gm_ops;      /* boxes/labels/lines, arrival order */
GridDraw g_gm_player;                /* the one player marker */
bool g_gm_have_player = false;
double g_gm_scale = 30;              /* px per grid unit (Grid_SetScale) */
int g_gm_height = 0;                 /* pane height in px (JS.ShowGrid) */
int g_gm_layer = 0;                  /* current layer (last activateLayer) */
bool g_gm_dirty = false;             /* display list changed since last blit */
glui32 g_gm_canvas = 0xFFFFFF;       /* SetBackgroundColour (Core default White) */

/* The pixels currently on screen, so a redraw sends only the rows that
 * changed -- and nothing at all for the usual turn that leaves the map
 * alone.  g_gm_flush voids the comparison whenever the window may no longer
 * hold what we last blitted (resize, fresh window). */
struct GmSurface {
    int w = 0, h = 0;
    std::vector<glui32> px;
    rgbsurf_t view() { return rgbsurf_t{ w, h, px.data() }; }
};
GmSurface g_gm_screen;
bool g_gm_flush = true;

/* The "you are here" glide.  grid.js moves the marker by a tenth of the
 * remaining leg per 60fps frame (~170ms a phase); the same feel at Glk timer
 * rates is 6 steps of 33ms.  g_gm_disp* is the marker as drawn (grid units),
 * g_gm_view* the view centre (map px); outside a glide both track
 * g_gm_player exactly (grid_map_anim_snap). */
const int GM_ANIM_STEPS = 6;

bool g_gm_glide = false;                  /* marker slide in flight */
bool g_gm_pan = false;                    /* recentre pan in flight */
double g_gm_dispx = 0, g_gm_dispy = 0;    /* drawn marker, grid units */
double g_gm_stepx = 0, g_gm_stepy = 0;    /* marker step per tick */
double g_gm_viewx = 0, g_gm_viewy = 0;    /* view centre, map px */
double g_gm_vstepx = 0, g_gm_vstepy = 0;  /* pan step per tick */

}  // namespace

bool grid_map_animating()
{
    return g_gm_glide || g_gm_pan;
}

/* Land the marker and centre the view on it at once -- deterministic mode,
 * a first marker, a scale change, or a blocking hook that needs the shared
 * timer for itself (do_pause). */
void grid_map_anim_snap()
{
    g_gm_glide = g_gm_pan = false;
    if (g_gm_have_player) {
        g_gm_dispx = g_gm_player.x;
        g_gm_dispy = g_gm_player.y;
        g_gm_viewx = g_gm_dispx * g_gm_scale;
        g_gm_viewy = g_gm_dispy * g_gm_scale;
        g_gm_dirty = true;
    }
}

/* One fast tick: step the marker toward its room; once it lands, step the
 * view centre toward the marker.  grid.js's onFrame chains the two phases
 * the same way, each a constant vector walked until the remaining distance
 * is shorter than the step. */
void grid_map_anim_tick()
{
    if (g_gm_glide) {
        double rx = g_gm_player.x - g_gm_dispx;
        double ry = g_gm_player.y - g_gm_dispy;
        if (rx * rx + ry * ry >
            g_gm_stepx * g_gm_stepx + g_gm_stepy * g_gm_stepy) {
            g_gm_dispx += g_gm_stepx;
            g_gm_dispy += g_gm_stepy;
        } else {
            g_gm_dispx = g_gm_player.x;
            g_gm_dispy = g_gm_player.y;
            g_gm_glide = false;
            g_gm_vstepx = (g_gm_dispx * g_gm_scale - g_gm_viewx) / GM_ANIM_STEPS;
            g_gm_vstepy = (g_gm_dispy * g_gm_scale - g_gm_viewy) / GM_ANIM_STEPS;
            g_gm_pan = true;
        }
    } else if (g_gm_pan) {
        double rx = g_gm_dispx * g_gm_scale - g_gm_viewx;
        double ry = g_gm_dispy * g_gm_scale - g_gm_viewy;
        if (rx * rx + ry * ry >
            g_gm_vstepx * g_gm_vstepx + g_gm_vstepy * g_gm_vstepy) {
            g_gm_viewx += g_gm_vstepx;
            g_gm_viewy += g_gm_vstepy;
        } else {
            g_gm_viewx = g_gm_dispx * g_gm_scale;
            g_gm_viewy = g_gm_dispy * g_gm_scale;
            g_gm_pan = false;
        }
    }
    g_gm_dirty = true;
}

namespace {

/* --- colours: CSS, as canvas/paper.js accepts and Quest authors write them
 * ("White", "DarkRed", "#803020"); the parser is the text printer's too. */
glui32 gm_colour(const std::string &spec, glui32 fallback)
{
    glui32 c = questglk::css_colour(spec);
    return c == questglk::COLOUR_NONE ? fallback : c;
}

/* --- the RGB surface and its primitives ----------------------------------
 * All drawing is the shared rasteriser's (common_utils/rgbsurface.h, which
 * the scarier map pane draws with too); these wrappers fix this map's
 * choices -- canvas-style square pens, undashed -- and bind it to a
 * GmSurface. */

void gm_fill_rect(GmSurface &s, int x0, int y0, int x1, int y1,
                  glui32 rgb, int alpha)
{
    rgbsurf_t v = s.view();
    rgbsurf_fill_rect(&v, x0, y0, x1, y1, rgb, alpha);
}

void gm_line(GmSurface &s, int x0, int y0, int x1, int y1, int wd,
             glui32 rgb, int alpha)
{
    rgbsurf_t v = s.view();
    rgbsurf_line(&v, x0, y0, x1, y1, wd, RGBSURF_NIB_SQUARE, rgb, alpha, 0);
}

void gm_fill_circle(GmSurface &s, int cx, int cy, int r, glui32 rgb, int alpha)
{
    rgbsurf_t v = s.view();
    rgbsurf_fill_circle(&v, cx, cy, r, rgb, alpha);
}

/* Labels are drawn in the 8x8 face; a character it lacks shows as '?'. */
unsigned char gm_drawable(unsigned char c)
{
    return c < 0x20 || c > 0x7e ? '?' : c;
}

/* Proportional advance: the glyph's used columns plus a 1px gap. */
int gm_advance(unsigned char c)
{
    c = gm_drawable(c);
    if (c == ' ')
        return 4;
    int w = rgbsurf_glyph_cols(&rgbsurf_font8x8, c);
    return (w ? w : 3) + 1;
}

void gm_glyph(GmSurface &s, unsigned char c, int x, int y,
              glui32 rgb, int alpha)
{
    rgbsurf_t v = s.view();
    rgbsurf_draw_glyph(&v, &rgbsurf_font8x8, gm_drawable(c), x, y, rgb, alpha);
}

/* The label's UTF-8 collapsed to drawable ASCII, non-ASCII as '?'. */
std::string gm_ascii(const std::string &utf8)
{
    std::string out;
    for (size_t i = 0; i < utf8.size();) {
        unsigned char c = (unsigned char) utf8[i];
        if (c < 0x80) { out += (char) c; i++; }
        else {
            out += '?';
            i++;
            while (i < utf8.size() &&
                   ((unsigned char) utf8[i] & 0xc0) == 0x80)
                i++;
        }
    }
    return out;
}

/* Centre-justified text on a baseline, like grid.js's PointText. */
void gm_text_centred(GmSurface &s, const std::string &text, int cx,
                     int baseline, glui32 rgb, int alpha)
{
    std::string t = gm_ascii(text);
    int w = 0;
    for (unsigned char c : t)
        w += gm_advance(c);
    int x = cx - w / 2;
    int y = baseline - 7;   /* glyphs sit on row 7 */
    for (unsigned char c : t) {
        gm_glyph(s, c, x, y, rgb, alpha);
        x += gm_advance(c);
    }
}

/* --- rasterising the display list ---------------------------------------- */

void gm_render(GmSurface &s)
{
    if (g_gm_ops.empty() && !g_gm_have_player)
        return;

    /* Centre the view on the (possibly mid-pan) view centre; before any
     * player marker exists, on the first box (grid.js's firstBox centring). */
    double cx = 0, cy = 0;
    if (g_gm_have_player) {
        cx = g_gm_viewx;
        cy = g_gm_viewy;
    } else {
        for (const GridDraw &o : g_gm_ops)
            if (o.op == GridDraw::Op::Box) {
                cx = (o.x + o.w / 2) * g_gm_scale;
                cy = (o.y + o.h / 2) * g_gm_scale;
                break;
            }
    }
    double offx = s.w / 2.0 - cx, offy = s.h / 2.0 - cy;
    /* Clamped: (int)lround on an absurd authored coordinate/scale is UB (the
     * long wraps past int). +/-1e9 is still "far off the surface" to every
     * clipped primitive, and stays inside int range. */
    auto pclamp = [](double v) {
        if (!(v >= -1e9)) return (int) -1e9;   /* also catches NaN */
        if (v > 1e9) return (int) 1e9;
        return (int) lround(v);
    };
    auto px = [&](double gx) { return pclamp(gx * g_gm_scale + offx); };
    auto py = [&](double gy) { return pclamp(gy * g_gm_scale + offy); };

    /* The layers present, bottom-up; the current one full strength, the
     * rest at grid.js's 0.2 opacity. */
    std::vector<int> zs;
    for (const GridDraw &o : g_gm_ops)
        if (std::find(zs.begin(), zs.end(), o.z) == zs.end())
            zs.push_back(o.z);
    std::sort(zs.begin(), zs.end());

    for (int z : zs) {
        int alpha = z == g_gm_layer ? 255 : 51;
        for (const GridDraw &o : g_gm_ops) {
            if (o.z != z)
                continue;
            switch (o.op) {
            case GridDraw::Op::Box: {
                int x0 = px(o.x), y0 = py(o.y);
                int x1 = px(o.x + o.w), y1 = py(o.y + o.h);
                gm_fill_rect(s, x0, y0, x1, y1,
                             gm_colour(o.fill, 0xFFFFFF), alpha);
                if (o.borderwidth > 0) {
                    glui32 bc = gm_colour(o.border, 0);
                    /* sides is an NESW bitmask: which borders to draw. */
                    if (o.sides & 8)
                        gm_line(s, x0, y0, x1, y0, o.borderwidth, bc, alpha);
                    if (o.sides & 4)
                        gm_line(s, x1, y0, x1, y1, o.borderwidth, bc, alpha);
                    if (o.sides & 2)
                        gm_line(s, x0, y1, x1, y1, o.borderwidth, bc, alpha);
                    if (o.sides & 1)
                        gm_line(s, x0, y0, x0, y1, o.borderwidth, bc, alpha);
                }
                break;
            }
            case GridDraw::Op::Line:
                gm_line(s, px(o.x), py(o.y), px(o.x2), py(o.y2),
                        o.borderwidth, gm_colour(o.border, 0), alpha);
                break;
            case GridDraw::Op::Label:
                gm_text_centred(s, o.text, px(o.x), py(o.y),
                                gm_colour(o.fill, 0), alpha);
                break;
            default:
                break;
            }
        }
    }

    /* The player, on top (grid.js keeps the marker at the end of the active
     * layer), at its displayed -- possibly mid-glide -- position.  Radius is
     * in pixels; the stroke straddles it. */
    if (g_gm_have_player) {
        const GridDraw &p = g_gm_player;
        int r = (int) lround(p.w), bw = p.borderwidth;
        int mx = px(g_gm_dispx), my = py(g_gm_dispy);
        if (bw > 0)
            gm_fill_circle(s, mx, my, r + (bw + 1) / 2,
                           gm_colour(p.border, 0), 255);
        gm_fill_circle(s, mx, my, r - bw / 2,
                       gm_colour(p.fill, 0xFFFF00), 255);
    }
}

}  // namespace

/* --- the Glk window ------------------------------------------------------ */

void gm_screen_drop()
{
    g_gm_screen = GmSurface();
    g_gm_flush = true;
}

/* Rasterise and blit.  Cheap when nothing changed: the usual turn redraws
 * nothing at all, and a turn that moved the player flushes only the rows
 * the marker crossed. */
void redraw_grid_map()
{
    if (!gmapwin || (!g_gm_dirty && !g_gm_flush))
        return;
    glui32 w = 0, h = 0;
    glk_window_get_size(gmapwin, &w, &h);
    if (w == 0 || h == 0)
        return;

    GmSurface surf;
    surf.w = (int) w;
    surf.h = (int) h;
    surf.px.assign((size_t) w * h, g_gm_canvas);
    gm_render(surf);

    /* Only the rows that changed since the last blit, unless the window may
     * no longer hold it (a different size counts as that too). */
    rgbsurf_t now = surf.view(), shown = g_gm_screen.view();
    rgbsurf_send(&now, g_gm_flush ? nullptr : &shown,
                 [](void *, unsigned int rgb, int x, int y, int len) {
                     glk_window_fill_rect(gmapwin, rgb, (glsi32) x, (glsi32) y,
                                          (glui32) len, 1);
                 },
                 nullptr);

    g_gm_screen = std::move(surf);
    g_gm_flush = false;
    g_gm_dirty = false;
}

/* A window resize blanks a Glk graphics window, so Arrange/Redraw events
 * force the whole surface out again. */
void grid_map_arrange()
{
    g_gm_flush = true;
    redraw_grid_map();
}

/* Fraction of the window height the map pane occupies when shown.  The
 * engine's ShowGrid carries a pixel height, but the pane reads better as a
 * fixed proportion of the window (like the Scarier/ADRIFT map band). */
static const glui32 GM_HEIGHT_PERCENT = 30;

/* JS.ShowGrid: open the pane at 30% of the window height, or (<= 0) close
 * it.  The engine's `hpx` is kept only as the show/hide trigger; the display
 * list survives a hide, as in the reference player (ShowGrid(0) just
 * collapses the panel). */
static void grid_map_show(int hpx)
{
    g_gm_height = hpx;
    if (hpx > 0 && !gmapwin) {
        gmapwin = glk_window_open(glk_window_get_root(),
                                  winmethod_Above | winmethod_Proportional,
                                  GM_HEIGHT_PERCENT, wintype_Graphics, 0);
        if (gmapwin) {
            glk_window_set_background_color(gmapwin, g_gm_canvas);
            glk_window_clear(gmapwin);
        }
        gm_screen_drop();
    } else if (hpx <= 0 && gmapwin) {
        glk_window_close(gmapwin, nullptr);
        gmapwin = nullptr;
        grid_map_anim_snap();
        gm_screen_drop();
    } else if (hpx > 0 && gmapwin) {
        winid_t parent = glk_window_get_parent(gmapwin);
        if (parent)
            glk_window_set_arrangement(parent,
                                       winmethod_Above | winmethod_Proportional,
                                       GM_HEIGHT_PERCENT, gmapwin);
        gm_screen_drop();
    }
    g_gm_dirty = true;
}

/* The grid_draw host hook: fold one engine paint command into the display
 * list.  Box/Label/Player activate their layer (grid.js activateLayer);
 * a Line carries no z of its own and lands on the current one. */
void grid_map_command(const GridDraw &g)
{
    switch (g.op) {
    case GridDraw::Op::Show:
        grid_map_show((int) lround(g.h));
        return;
    case GridDraw::Op::Scale:
        if (g.w > 0) {
            g_gm_scale = g.w;
            grid_map_anim_snap();  /* the view centre is in map px */
        }
        break;
    case GridDraw::Op::Box:
    case GridDraw::Op::Label:
        g_gm_layer = g.z;
        g_gm_ops.push_back(g);
        break;
    case GridDraw::Op::Line: {
        GridDraw line = g;
        line.z = g_gm_layer;
        g_gm_ops.push_back(line);
        break;
    }
    case GridDraw::Op::Player: {
        bool moved = g_gm_have_player &&
                     (g.x != g_gm_player.x || g.y != g_gm_player.y);
        bool had = g_gm_have_player;
        g_gm_layer = g.z;
        g_gm_player = g;
        g_gm_have_player = true;
        if (moved && gli_sa_delays && gmapwin) {
            /* Glide from wherever the marker is drawn right now -- a move
             * mid-glide just retargets, like grid.js recomputing
             * playerVector from the current position.  The recentre pan is
             * armed when the marker lands (grid_map_anim_tick). */
            g_gm_glide = true;
            g_gm_pan = false;
            g_gm_stepx = (g.x - g_gm_dispx) / GM_ANIM_STEPS;
            g_gm_stepy = (g.y - g_gm_dispy) / GM_ANIM_STEPS;
        } else if (!had || moved) {
            grid_map_anim_snap();
        }
        break;
    }
    case GridDraw::Op::Clear:
        g_gm_ops.clear();
        g_gm_have_player = false;
        g_gm_glide = g_gm_pan = false;
        break;
    case GridDraw::Op::Canvas: {
        /* An unparseable colour keeps white, the Core default -- the paint
         * commands are authored against whatever this is, so guessing wrong
         * hides them (white-on-white exit lines). */
        glui32 c = gm_colour(g.fill, 0xFFFFFF);
        if (c == g_gm_canvas)
            return;
        g_gm_canvas = c;
        if (gmapwin) {
            glk_window_set_background_color(gmapwin, g_gm_canvas);
            glk_window_clear(gmapwin);
        }
        gm_screen_drop();   /* the window no longer holds what we blitted */
        break;
    }
    }
    g_gm_dirty = true;
}

/* Session teardown (RESTART/RESTORE boot a fresh world): nothing survives,
 * like the reference player reloading its page.  The new session's
 * InitInterface re-opens the pane and Grid_Redraw repaints every visited
 * room from the restored coordinate dictionaries. */
void grid_map_reset()
{
    if (gmapwin) {
        glk_window_close(gmapwin, nullptr);
        gmapwin = nullptr;
    }
    g_gm_ops.clear();
    g_gm_have_player = false;
    g_gm_scale = 30;
    g_gm_height = 0;
    g_gm_layer = 0;
    g_gm_canvas = 0xFFFFFF;
    g_gm_dirty = false;
    g_gm_glide = g_gm_pan = false;
    g_gm_dispx = g_gm_dispy = g_gm_viewx = g_gm_viewy = 0;
    gm_screen_drop();
}
