/* aslxglk-map.inc -- the Quest 5 grid map (game.gridmap), unity-included by
 * aslxglk.cc inside its anonymous namespace.
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
 * is the filled rectangle, so -- as in the scarier map pane -- the picture
 * is rendered into an RGB surface and flushed as run-length spans of the
 * rows that changed since the last blit.
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
 *  - colours are authored HTML names / #rrggbb on a white canvas, so the
 *    pane keeps the reference player's white background in any Glk theme.
 * Not carried over: pan/drag and zoom gestures, GridSquareClick, and the
 * custom-layer vocabulary (Grid_DrawSquare/Svg/Image/Shape...), which the
 * engine ignores for now.
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

winid_t gmapwin = nullptr;

std::vector<GridDraw> g_gm_ops;      /* boxes/labels/lines, arrival order */
GridDraw g_gm_player;                /* the one player marker */
bool g_gm_have_player = false;
double g_gm_scale = 30;              /* px per grid unit (Grid_SetScale) */
int g_gm_height = 0;                 /* pane height in px (JS.ShowGrid) */
int g_gm_layer = 0;                  /* current layer (last activateLayer) */
bool g_gm_dirty = false;             /* display list changed since last blit */

/* The pixels currently on screen, so a redraw sends only the rows that
 * changed -- and nothing at all for the usual turn that leaves the map
 * alone.  g_gm_flush voids the comparison whenever the window may no longer
 * hold what we last blitted (resize, fresh window). */
struct GmSurface {
    int w = 0, h = 0;
    std::vector<glui32> px;
};
GmSurface g_gm_screen;
bool g_gm_flush = true;

/* The "you are here" glide.  grid.js moves the marker by a tenth of the
 * remaining leg per 60fps frame (~170ms a phase); the same feel at Glk timer
 * rates is 6 steps of 33ms.  g_gm_disp* is the marker as drawn (grid units),
 * g_gm_view* the view centre (map px); outside a glide both track
 * g_gm_player exactly (grid_map_anim_snap). */
const glui32 GM_ANIM_TICK_MS = 33;
const int GM_ANIM_STEPS = 6;

bool g_gm_glide = false;                  /* marker slide in flight */
bool g_gm_pan = false;                    /* recentre pan in flight */
double g_gm_dispx = 0, g_gm_dispy = 0;    /* drawn marker, grid units */
double g_gm_stepx = 0, g_gm_stepy = 0;    /* marker step per tick */
double g_gm_viewx = 0, g_gm_viewy = 0;    /* view centre, map px */
double g_gm_vstepx = 0, g_gm_vstepy = 0;  /* pan step per tick */

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

/* --- colours: the CSS3 extended keywords, as canvas/paper.js accepts and
 * Quest authors write them ("White", "DarkRed", "#803020"). */
const struct { const char *name; glui32 rgb; } kGmColours[] = {
    {"aliceblue", 0xF0F8FF}, {"antiquewhite", 0xFAEBD7}, {"aqua", 0x00FFFF},
    {"aquamarine", 0x7FFFD4}, {"azure", 0xF0FFFF}, {"beige", 0xF5F5DC},
    {"bisque", 0xFFE4C4}, {"black", 0x000000}, {"blanchedalmond", 0xFFEBCD},
    {"blue", 0x0000FF}, {"blueviolet", 0x8A2BE2}, {"brown", 0xA52A2A},
    {"burlywood", 0xDEB887}, {"cadetblue", 0x5F9EA0}, {"chartreuse", 0x7FFF00},
    {"chocolate", 0xD2691E}, {"coral", 0xFF7F50},
    {"cornflowerblue", 0x6495ED}, {"cornsilk", 0xFFF8DC},
    {"crimson", 0xDC143C}, {"cyan", 0x00FFFF}, {"darkblue", 0x00008B},
    {"darkcyan", 0x008B8B}, {"darkgoldenrod", 0xB8860B},
    {"darkgray", 0xA9A9A9}, {"darkgreen", 0x006400}, {"darkgrey", 0xA9A9A9},
    {"darkkhaki", 0xBDB76B}, {"darkmagenta", 0x8B008B},
    {"darkolivegreen", 0x556B2F}, {"darkorange", 0xFF8C00},
    {"darkorchid", 0x9932CC}, {"darkred", 0x8B0000},
    {"darksalmon", 0xE9967A}, {"darkseagreen", 0x8FBC8F},
    {"darkslateblue", 0x483D8B}, {"darkslategray", 0x2F4F4F},
    {"darkslategrey", 0x2F4F4F}, {"darkturquoise", 0x00CED1},
    {"darkviolet", 0x9400D3}, {"deeppink", 0xFF1493},
    {"deepskyblue", 0x00BFFF}, {"dimgray", 0x696969}, {"dimgrey", 0x696969},
    {"dodgerblue", 0x1E90FF}, {"firebrick", 0xB22222},
    {"floralwhite", 0xFFFAF0}, {"forestgreen", 0x228B22},
    {"fuchsia", 0xFF00FF}, {"gainsboro", 0xDCDCDC}, {"ghostwhite", 0xF8F8FF},
    {"gold", 0xFFD700}, {"goldenrod", 0xDAA520}, {"gray", 0x808080},
    {"green", 0x008000}, {"greenyellow", 0xADFF2F}, {"grey", 0x808080},
    {"honeydew", 0xF0FFF0}, {"hotpink", 0xFF69B4}, {"indianred", 0xCD5C5C},
    {"indigo", 0x4B0082}, {"ivory", 0xFFFFF0}, {"khaki", 0xF0E68C},
    {"lavender", 0xE6E6FA}, {"lavenderblush", 0xFFF0F5},
    {"lawngreen", 0x7CFC00}, {"lemonchiffon", 0xFFFACD},
    {"lightblue", 0xADD8E6}, {"lightcoral", 0xF08080},
    {"lightcyan", 0xE0FFFF}, {"lightgoldenrodyellow", 0xFAFAD2},
    {"lightgray", 0xD3D3D3}, {"lightgreen", 0x90EE90},
    {"lightgrey", 0xD3D3D3}, {"lightpink", 0xFFB6C1},
    {"lightsalmon", 0xFFA07A}, {"lightseagreen", 0x20B2AA},
    {"lightskyblue", 0x87CEFA}, {"lightslategray", 0x778899},
    {"lightslategrey", 0x778899}, {"lightsteelblue", 0xB0C4DE},
    {"lightyellow", 0xFFFFE0}, {"lime", 0x00FF00}, {"limegreen", 0x32CD32},
    {"linen", 0xFAF0E6}, {"magenta", 0xFF00FF}, {"maroon", 0x800000},
    {"mediumaquamarine", 0x66CDAA}, {"mediumblue", 0x0000CD},
    {"mediumorchid", 0xBA55D3}, {"mediumpurple", 0x9370DB},
    {"mediumseagreen", 0x3CB371}, {"mediumslateblue", 0x7B68EE},
    {"mediumspringgreen", 0x00FA9A}, {"mediumturquoise", 0x48D1CC},
    {"mediumvioletred", 0xC71585}, {"midnightblue", 0x191970},
    {"mintcream", 0xF5FFFA}, {"mistyrose", 0xFFE4E1}, {"moccasin", 0xFFE4B5},
    {"navajowhite", 0xFFDEAD}, {"navy", 0x000080}, {"oldlace", 0xFDF5E6},
    {"olive", 0x808000}, {"olivedrab", 0x6B8E23}, {"orange", 0xFFA500},
    {"orangered", 0xFF4500}, {"orchid", 0xDA70D6},
    {"palegoldenrod", 0xEEE8AA}, {"palegreen", 0x98FB98},
    {"paleturquoise", 0xAFEEEE}, {"palevioletred", 0xDB7093},
    {"papayawhip", 0xFFEFD5}, {"peachpuff", 0xFFDAB9}, {"peru", 0xCD853F},
    {"pink", 0xFFC0CB}, {"plum", 0xDDA0DD}, {"powderblue", 0xB0E0E6},
    {"purple", 0x800080}, {"rebeccapurple", 0x663399}, {"red", 0xFF0000},
    {"rosybrown", 0xBC8F8F}, {"royalblue", 0x4169E1},
    {"saddlebrown", 0x8B4513}, {"salmon", 0xFA8072},
    {"sandybrown", 0xF4A460}, {"seagreen", 0x2E8B57}, {"seashell", 0xFFF5EE},
    {"sienna", 0xA0522D}, {"silver", 0xC0C0C0}, {"skyblue", 0x87CEEB},
    {"slateblue", 0x6A5ACD}, {"slategray", 0x708090},
    {"slategrey", 0x708090}, {"snow", 0xFFFAFA}, {"springgreen", 0x00FF7F},
    {"steelblue", 0x4682B4}, {"tan", 0xD2B48C}, {"teal", 0x008080},
    {"thistle", 0xD8BFD8}, {"tomato", 0xFF6347}, {"turquoise", 0x40E0D0},
    {"violet", 0xEE82EE}, {"wheat", 0xF5DEB3}, {"white", 0xFFFFFF},
    {"whitesmoke", 0xF5F5F5}, {"yellow", 0xFFFF00},
    {"yellowgreen", 0x9ACD32},
};

glui32 gm_colour(const std::string &spec, glui32 fallback)
{
    std::string s;
    for (char c : spec)
        if (!isspace((unsigned char) c))
            s += (char) tolower((unsigned char) c);
    if (s.empty())
        return fallback;
    if (s[0] == '#') {
        glui32 v = 0;
        int digits = 0;
        for (size_t i = 1; i < s.size() && digits < 6; i++, digits++) {
            char c = s[i];
            int d = c >= '0' && c <= '9' ? c - '0'
                  : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            if (d < 0)
                return fallback;
            v = (v << 4) | (glui32) d;
        }
        if (digits == 3) {  /* #rgb -> #rrggbb */
            glui32 r = (v >> 8) & 0xf, g = (v >> 4) & 0xf, b = v & 0xf;
            return (r * 0x11 << 16) | (g * 0x11 << 8) | (b * 0x11);
        }
        return digits == 6 ? v : fallback;
    }
    for (const auto &e : kGmColours)
        if (s == e.name)
            return e.rgb;
    return fallback;
}

/* --- the RGB surface and its primitives ---------------------------------- */

void gm_blend(GmSurface &s, int x, int y, glui32 rgb, int alpha)
{
    if (x < 0 || y < 0 || x >= s.w || y >= s.h)
        return;
    glui32 &p = s.px[(size_t) y * s.w + x];
    if (alpha >= 255) {
        p = rgb;
        return;
    }
    glui32 r = ((rgb >> 16 & 0xff) * alpha + (p >> 16 & 0xff) * (255 - alpha)) / 255;
    glui32 g = ((rgb >> 8 & 0xff) * alpha + (p >> 8 & 0xff) * (255 - alpha)) / 255;
    glui32 b = ((rgb & 0xff) * alpha + (p & 0xff) * (255 - alpha)) / 255;
    p = (r << 16) | (g << 8) | b;
}

/* Fill [x0,x1) x [y0,y1). */
void gm_fill_rect(GmSurface &s, int x0, int y0, int x1, int y1,
                  glui32 rgb, int alpha)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            gm_blend(s, x, y, rgb, alpha);
}

/* A wd x wd pen stamp, centred like a canvas stroke straddles its path. */
void gm_nib(GmSurface &s, int x, int y, int wd, glui32 rgb, int alpha)
{
    if (wd <= 1) {
        gm_blend(s, x, y, rgb, alpha);
        return;
    }
    int o = wd / 2;
    gm_fill_rect(s, x - o, y - o, x - o + wd, y - o + wd, rgb, alpha);
}

void gm_line(GmSurface &s, int x0, int y0, int x1, int y1, int wd,
             glui32 rgb, int alpha)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gm_nib(s, x0, y0, wd, rgb, alpha);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gm_fill_circle(GmSurface &s, int cx, int cy, int r, glui32 rgb, int alpha)
{
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int) floor(sqrt((double) (r * r - dy * dy)) + 0.5);
        for (int x = cx - dx; x <= cx + dx; x++)
            gm_blend(s, x, cy + dy, rgb, alpha);
    }
}

/* --- 8x8 bitmap text (font8x8_basic, public domain: Marcel Sondaar; the
 * same face the scarier map pane carries).  Glk graphics windows cannot
 * render text, so room labels are drawn as glyphs.  LSB of each row byte is
 * the leftmost pixel. */
const unsigned char kGmFont[95][8] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* space */
  { 0x18, 0x3c, 0x3c, 0x18, 0x18, 0x00, 0x18, 0x00 },  /* '!' */
  { 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* '"' */
  { 0x36, 0x36, 0x7f, 0x36, 0x7f, 0x36, 0x36, 0x00 },  /* '#' */
  { 0x0c, 0x3e, 0x03, 0x1e, 0x30, 0x1f, 0x0c, 0x00 },  /* '$' */
  { 0x00, 0x63, 0x33, 0x18, 0x0c, 0x66, 0x63, 0x00 },  /* '%' */
  { 0x1c, 0x36, 0x1c, 0x6e, 0x3b, 0x33, 0x6e, 0x00 },  /* '&' */
  { 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* '\'' */
  { 0x18, 0x0c, 0x06, 0x06, 0x06, 0x0c, 0x18, 0x00 },  /* '(' */
  { 0x06, 0x0c, 0x18, 0x18, 0x18, 0x0c, 0x06, 0x00 },  /* ')' */
  { 0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00 },  /* '*' */
  { 0x00, 0x0c, 0x0c, 0x3f, 0x0c, 0x0c, 0x00, 0x00 },  /* '+' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x06 },  /* ',' */
  { 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00 },  /* '-' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x00 },  /* '.' */
  { 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x01, 0x00 },  /* '/' */
  { 0x3e, 0x63, 0x73, 0x7b, 0x6f, 0x67, 0x3e, 0x00 },  /* '0' */
  { 0x0c, 0x0e, 0x0c, 0x0c, 0x0c, 0x0c, 0x3f, 0x00 },  /* '1' */
  { 0x1e, 0x33, 0x30, 0x1c, 0x06, 0x33, 0x3f, 0x00 },  /* '2' */
  { 0x1e, 0x33, 0x30, 0x1c, 0x30, 0x33, 0x1e, 0x00 },  /* '3' */
  { 0x38, 0x3c, 0x36, 0x33, 0x7f, 0x30, 0x78, 0x00 },  /* '4' */
  { 0x3f, 0x03, 0x1f, 0x30, 0x30, 0x33, 0x1e, 0x00 },  /* '5' */
  { 0x1c, 0x06, 0x03, 0x1f, 0x33, 0x33, 0x1e, 0x00 },  /* '6' */
  { 0x3f, 0x33, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x00 },  /* '7' */
  { 0x1e, 0x33, 0x33, 0x1e, 0x33, 0x33, 0x1e, 0x00 },  /* '8' */
  { 0x1e, 0x33, 0x33, 0x3e, 0x30, 0x18, 0x0e, 0x00 },  /* '9' */
  { 0x00, 0x0c, 0x0c, 0x00, 0x00, 0x0c, 0x0c, 0x00 },  /* ':' */
  { 0x00, 0x0c, 0x0c, 0x00, 0x00, 0x0c, 0x0c, 0x06 },  /* ';' */
  { 0x18, 0x0c, 0x06, 0x03, 0x06, 0x0c, 0x18, 0x00 },  /* '<' */
  { 0x00, 0x00, 0x3f, 0x00, 0x00, 0x3f, 0x00, 0x00 },  /* '=' */
  { 0x06, 0x0c, 0x18, 0x30, 0x18, 0x0c, 0x06, 0x00 },  /* '>' */
  { 0x1e, 0x33, 0x30, 0x18, 0x0c, 0x00, 0x0c, 0x00 },  /* '?' */
  { 0x3e, 0x63, 0x7b, 0x7b, 0x7b, 0x03, 0x1e, 0x00 },  /* '@' */
  { 0x0c, 0x1e, 0x33, 0x33, 0x3f, 0x33, 0x33, 0x00 },  /* 'A' */
  { 0x3f, 0x66, 0x66, 0x3e, 0x66, 0x66, 0x3f, 0x00 },  /* 'B' */
  { 0x3c, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3c, 0x00 },  /* 'C' */
  { 0x1f, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1f, 0x00 },  /* 'D' */
  { 0x7f, 0x46, 0x16, 0x1e, 0x16, 0x46, 0x7f, 0x00 },  /* 'E' */
  { 0x7f, 0x46, 0x16, 0x1e, 0x16, 0x06, 0x0f, 0x00 },  /* 'F' */
  { 0x3c, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7c, 0x00 },  /* 'G' */
  { 0x33, 0x33, 0x33, 0x3f, 0x33, 0x33, 0x33, 0x00 },  /* 'H' */
  { 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'I' */
  { 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1e, 0x00 },  /* 'J' */
  { 0x67, 0x66, 0x36, 0x1e, 0x36, 0x66, 0x67, 0x00 },  /* 'K' */
  { 0x0f, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7f, 0x00 },  /* 'L' */
  { 0x63, 0x77, 0x7f, 0x7f, 0x6b, 0x63, 0x63, 0x00 },  /* 'M' */
  { 0x63, 0x67, 0x6f, 0x7b, 0x73, 0x63, 0x63, 0x00 },  /* 'N' */
  { 0x1c, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1c, 0x00 },  /* 'O' */
  { 0x3f, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x0f, 0x00 },  /* 'P' */
  { 0x1e, 0x33, 0x33, 0x33, 0x3b, 0x1e, 0x38, 0x00 },  /* 'Q' */
  { 0x3f, 0x66, 0x66, 0x3e, 0x36, 0x66, 0x67, 0x00 },  /* 'R' */
  { 0x1e, 0x33, 0x07, 0x0e, 0x38, 0x33, 0x1e, 0x00 },  /* 'S' */
  { 0x3f, 0x2d, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'T' */
  { 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3f, 0x00 },  /* 'U' */
  { 0x33, 0x33, 0x33, 0x33, 0x33, 0x1e, 0x0c, 0x00 },  /* 'V' */
  { 0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00 },  /* 'W' */
  { 0x63, 0x63, 0x36, 0x1c, 0x1c, 0x36, 0x63, 0x00 },  /* 'X' */
  { 0x33, 0x33, 0x33, 0x1e, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'Y' */
  { 0x7f, 0x63, 0x31, 0x18, 0x4c, 0x66, 0x7f, 0x00 },  /* 'Z' */
  { 0x1e, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1e, 0x00 },  /* '[' */
  { 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x40, 0x00 },  /* '\\' */
  { 0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1e, 0x00 },  /* ']' */
  { 0x08, 0x1c, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00 },  /* '^' */
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff },  /* '_' */
  { 0x0c, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00 },  /* '`' */
  { 0x00, 0x00, 0x1e, 0x30, 0x3e, 0x33, 0x6e, 0x00 },  /* 'a' */
  { 0x07, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x3b, 0x00 },  /* 'b' */
  { 0x00, 0x00, 0x1e, 0x33, 0x03, 0x33, 0x1e, 0x00 },  /* 'c' */
  { 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6e, 0x00 },  /* 'd' */
  { 0x00, 0x00, 0x1e, 0x33, 0x3f, 0x03, 0x1e, 0x00 },  /* 'e' */
  { 0x1c, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0f, 0x00 },  /* 'f' */
  { 0x00, 0x00, 0x6e, 0x33, 0x33, 0x3e, 0x30, 0x1f },  /* 'g' */
  { 0x07, 0x06, 0x36, 0x6e, 0x66, 0x66, 0x67, 0x00 },  /* 'h' */
  { 0x0c, 0x00, 0x0e, 0x0c, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'i' */
  { 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1e },  /* 'j' */
  { 0x07, 0x06, 0x66, 0x36, 0x1e, 0x36, 0x67, 0x00 },  /* 'k' */
  { 0x0e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x00 },  /* 'l' */
  { 0x00, 0x00, 0x33, 0x7f, 0x7f, 0x6b, 0x63, 0x00 },  /* 'm' */
  { 0x00, 0x00, 0x1f, 0x33, 0x33, 0x33, 0x33, 0x00 },  /* 'n' */
  { 0x00, 0x00, 0x1e, 0x33, 0x33, 0x33, 0x1e, 0x00 },  /* 'o' */
  { 0x00, 0x00, 0x3b, 0x66, 0x66, 0x3e, 0x06, 0x0f },  /* 'p' */
  { 0x00, 0x00, 0x6e, 0x33, 0x33, 0x3e, 0x30, 0x78 },  /* 'q' */
  { 0x00, 0x00, 0x3b, 0x6e, 0x66, 0x06, 0x0f, 0x00 },  /* 'r' */
  { 0x00, 0x00, 0x3e, 0x03, 0x1e, 0x30, 0x1f, 0x00 },  /* 's' */
  { 0x08, 0x0c, 0x3e, 0x0c, 0x0c, 0x2c, 0x18, 0x00 },  /* 't' */
  { 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6e, 0x00 },  /* 'u' */
  { 0x00, 0x00, 0x33, 0x33, 0x33, 0x1e, 0x0c, 0x00 },  /* 'v' */
  { 0x00, 0x00, 0x63, 0x6b, 0x7f, 0x7f, 0x36, 0x00 },  /* 'w' */
  { 0x00, 0x00, 0x63, 0x36, 0x1c, 0x36, 0x63, 0x00 },  /* 'x' */
  { 0x00, 0x00, 0x33, 0x33, 0x33, 0x3e, 0x30, 0x1f },  /* 'y' */
  { 0x00, 0x00, 0x3f, 0x19, 0x0c, 0x26, 0x3f, 0x00 },  /* 'z' */
  { 0x38, 0x0c, 0x0c, 0x07, 0x0c, 0x0c, 0x38, 0x00 },  /* '{' */
  { 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00 },  /* '|' */
  { 0x07, 0x0c, 0x0c, 0x38, 0x0c, 0x0c, 0x07, 0x00 },  /* '}' */
  { 0x6e, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }   /* '~' */
};

/* Proportional advance: the glyph's used columns plus a 1px gap. */
int gm_advance(unsigned char c)
{
    if (c < 0x20 || c > 0x7e)
        c = '?';
    if (c == ' ')
        return 4;
    unsigned bits = 0;
    for (int r = 0; r < 8; r++)
        bits |= kGmFont[c - 0x20][r];
    int w = 0;
    while (bits) { w++; bits >>= 1; }
    return (w ? w : 3) + 1;
}

void gm_glyph(GmSurface &s, unsigned char c, int x, int y,
              glui32 rgb, int alpha)
{
    if (c < 0x20 || c > 0x7e)
        c = '?';
    const unsigned char *g = kGmFont[c - 0x20];
    for (int r = 0; r < 8; r++)
        for (int b = 0; b < 8; b++)
            if (g[r] & (1u << b))
                gm_blend(s, x + b, y + r, rgb, alpha);
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
    auto px = [&](double gx) { return (int) lround(gx * g_gm_scale + offx); };
    auto py = [&](double gy) { return (int) lround(gy * g_gm_scale + offy); };

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
    surf.px.assign((size_t) w * h, 0xFFFFFF);
    gm_render(surf);

    if (g_gm_screen.w != surf.w || g_gm_screen.h != surf.h)
        g_gm_flush = true;

    for (int y = 0; y < surf.h; y++) {
        const glui32 *row = &surf.px[(size_t) y * surf.w];
        if (!g_gm_flush &&
            memcmp(row, &g_gm_screen.px[(size_t) y * surf.w],
                   (size_t) surf.w * sizeof row[0]) == 0)
            continue;
        int x = 0;
        while (x < surf.w) {
            glui32 c = row[x];
            int x0 = x;
            do
                x++;
            while (x < surf.w && row[x] == c);
            glk_window_fill_rect(gmapwin, c, (glsi32) x0, (glsi32) y,
                                 (glui32) (x - x0), 1);
        }
    }

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
void grid_map_show(int hpx)
{
    g_gm_height = hpx;
    if (hpx > 0 && !gmapwin) {
        gmapwin = glk_window_open(glk_window_get_root(),
                                  winmethod_Above | winmethod_Proportional,
                                  GM_HEIGHT_PERCENT, wintype_Graphics, 0);
        if (gmapwin) {
            glk_window_set_background_color(gmapwin, 0xFFFFFF);
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
    g_gm_dirty = false;
    g_gm_glide = g_gm_pan = false;
    g_gm_dispx = g_gm_dispy = g_gm_viewx = g_gm_viewy = 0;
    gm_screen_drop();
}
