// v6_panels.cpp
//
// The windows of V6 games other than Infocom's four (Moments Out of Time, the
// V6Lib port of Adventure, Wyrmward): panels laid out from the geometry the
// game gives its windows, and their autosave.

#include <algorithm>
#include <array>
#include <vector>

#include "memory.h"
#include "options.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"

#include "v6_shared.hpp"
#include "v6_panels.hpp"

// Windows 2-7 are normally redirected to the main window, but that
// jumbles the output of games which really lay out their windows:
// Moments Out of Time keeps a menu bar in a one-line window at the
// bottom of the screen, and the V6Lib port of Adventure can show the
// room description and the inventory in windows above the story text.
// So a window which the game has both sized and moved, and which is
// neither the window commands are read in nor covered by a window
// placed later, becomes a panel: a Glk window of its own above or below
// the main window. As Glk windows can't overlap, the panels are laid
// out in horizontal bands, with panels that are side by side sharing
// a band.
static int v6_story_window = -1;
static std::vector<int> v6_layout_signature;
static bool v6_layout_dirty;

void v6_mark_layout_dirty()
{
    v6_layout_dirty = true;
}

void v6_forget_layout()
{
    v6_story_window = -1;
    v6_layout_signature.clear();
    v6_layout_dirty = false;
}

bool is_v6_panel(const Window *win)
{
    return v6_is_generic() &&
        win >= &windows[2] && win <= &windows[7] &&
        win->id != nullptr && win->id != mainwin->id;
}

bool is_v6_grid_panel(const Window *win)
{
    return is_v6_panel(win) && win->id->type == wintype_TextGrid;
}

struct V6Rect {
    int y0, x0, y1, x1;
};

static bool v6_window_rect(int window, V6Rect &rect)
{
    const auto &geometry = v6_geometry[window];

    if (!geometry.sized || !geometry.moved || as_signed(geometry.height) < 1 || as_signed(geometry.width) < 1) {
        return false;
    }

    rect.y0 = std::max<int>(as_signed(geometry.y), 1);
    rect.x0 = std::max<int>(as_signed(geometry.x), 1);
    rect.y1 = rect.y0 + geometry.height - 1;
    rect.x1 = rect.x0 + geometry.width - 1;

    return true;
}

// The story window is the one the game reads commands in. Until it has
// done so, take a window more than a few lines high which reaches the
// bottom of the screen for one.
bool v6_is_story_window(int window)
{
    V6Rect rect;

    if (v6_story_window >= 0) {
        return window == v6_story_window;
    }

    return v6_geometry[window].height > 3 && v6_window_rect(window, rect) && rect.y1 >= word(0x24);
}

static bool v6_window_covered(int window)
{
    V6Rect rect, other;

    if (!v6_window_rect(window, rect)) {
        return false;
    }

    for (int i = 0; i < 8; i++) {
        if (i != window && v6_geometry[i].stamp > v6_geometry[window].stamp && v6_window_rect(i, other) &&
            other.y0 <= rect.y1 && other.y1 >= rect.y0 && other.x0 <= rect.x1 && other.x1 >= rect.x0) {

            return true;
        }
    }

    return false;
}

static bool v6_wants_panel(int window)
{
    V6Rect rect;

    return window >= 2 && v6_window_rect(window, rect) && !v6_is_story_window(window) && !v6_window_covered(window);
}

struct V6Band {
    enum class Position { Top, Middle, Bottom } position;
    int y0, y1;
    std::vector<int> panels;
};

static std::vector<V6Band> v6_layout_bands()
{
    std::vector<int> panels;
    std::vector<V6Band> bands;
    V6Rect rect;

    for (int i = 2; i < 8; i++) {
        if (v6_wants_panel(i)) {
            panels.push_back(i);
        }
    }

    auto origin = [](int window) {
        V6Rect r;
        v6_window_rect(window, r);
        return std::make_pair(r.y0, r.x0);
    };

    std::sort(panels.begin(), panels.end(), [&origin](int a, int b) {
        return origin(a) < origin(b);
    });

    for (int panel : panels) {
        v6_window_rect(panel, rect);
        if (!bands.empty() && rect.y0 <= bands.back().y1) {
            bands.back().y1 = std::max(bands.back().y1, rect.y1);
            bands.back().panels.push_back(panel);
        } else {
            bands.push_back({V6Band::Position::Middle, rect.y0, rect.y1, {panel}});
        }
    }

    // A band above the upper window goes at the top of the screen, a
    // band in the lower half of the story window (or of the screen, if
    // there is no story window) goes below the main window, and all
    // others go between the upper window and the main window.
    V6Rect upper;
    bool have_upper = upperwin->id != nullptr && upper_window_height > 0 && v6_window_rect(1, upper);
    int centre = 1 + word(0x24);

    for (int i = 0; i < 8; i++) {
        if (v6_is_story_window(i) && v6_window_rect(i, rect)) {
            centre = rect.y0 + rect.y1;
            break;
        }
    }

    for (auto &band : bands) {
        std::sort(band.panels.begin(), band.panels.end(), [&origin](int a, int b) {
            return origin(a).second < origin(b).second;
        });

        if (have_upper && band.y1 < upper.y0) {
            band.position = V6Band::Position::Top;
        } else if (band.y0 + band.y1 > centre) {
            band.position = V6Band::Position::Bottom;
        }
    }

    return bands;
}

// A panel at most three lines high is a bar, which is always a grid.
// A larger one is a text buffer if the game has turned wrapping on.
static glui32 v6_panel_type(const V6Band &band, int window)
{
    return band.y1 - band.y0 + 1 > 3 && (windows[window].attribute & 1) ? wintype_TextBuffer : wintype_TextGrid;
}

static void v6_open_band(const V6Band &band)
{
    winid_t split = mainwin->id;
    glui32 method = winmethod_Above;
    winid_t previous = nullptr;
    int remaining = 0;

    if (band.position == V6Band::Position::Top) {
        split = glk_window_get_parent(upperwin->id);
    } else if (band.position == V6Band::Position::Bottom) {
        method = winmethod_Below;
    }

    for (int panel : band.panels) {
        remaining += v6_geometry[panel].width;
    }

    for (int panel : band.panels) {
        glui32 type = v6_panel_type(band, panel);
        int width = v6_geometry[panel].width;
        winid_t id;

        if (previous == nullptr) {
            if (split == nullptr) {
                return;
            }
            id = glk_window_open(split, method | winmethod_Fixed, band.y1 - band.y0 + 1, type, 0);
        } else {
            id = glk_window_open(previous, winmethod_Right | winmethod_Proportional, 100 * remaining / (remaining + width), type, 0);
        }

        if (id == nullptr) {
            return;
        }

        windows[panel].id = id;
        windows[panel].x = windows[panel].y = 0;
        previous = id;
        remaining -= width;
    }
}

static void v6_close_panels()
{
    for (int i = 2; i < 8; i++) {
        if (is_v6_panel(&windows[i])) {
            glk_window_close(windows[i].id, nullptr);
            windows[i].id = mainwin->id;
            windows[i].x = windows[i].y = 0;
        }
    }
}

// Describe a layout, to tell whether it differs from the one on screen.
static std::vector<int> v6_signature(const std::vector<V6Band> &bands, std::array<bool, 8> &wanted)
{
    std::vector<int> signature;

    for (const auto &band : bands) {
        signature.push_back(-1);
        signature.push_back(static_cast<int>(band.position));
        signature.push_back(band.y1 - band.y0 + 1);
        for (int panel : band.panels) {
            signature.push_back(panel);
            signature.push_back(v6_panel_type(band, panel));
            signature.push_back(v6_geometry[panel].width);
            wanted[panel] = true;
        }
    }

    return signature;
}

static void v6_apply_layout()
{
    v6_layout_dirty = false;

    if (!v6_is_generic() || !options.redirect_v6_windows || mainwin->id == nullptr) {
        return;
    }

    auto bands = v6_layout_bands();
    std::array<bool, 8> wanted{};
    auto signature = v6_signature(bands, wanted);

    bool same = signature == v6_layout_signature;
    for (int i = 2; i < 8 && same; i++) {
        same = is_v6_panel(&windows[i]) == wanted[i];
    }

    if (same) {
        return;
    }

    // Glk can't move windows around, so close all panels and open them
    // again in their new places.
    v6_close_panels();

    // Every new band is opened next to the upper window or the main
    // window, pushing the earlier ones outwards.
    for (const auto &band : bands) {
        if (band.position != V6Band::Position::Bottom) {
            v6_open_band(band);
        }
    }

    for (auto band = bands.rbegin(); band != bands.rend(); ++band) {
        if (band->position == V6Band::Position::Bottom) {
            v6_open_band(*band);
        }
    }

    v6_layout_signature = signature;

    glk_set_window(curwin->id);
    set_current_style();
}

// The layout is not updated as soon as the game moves or resizes a
// window, because games do that one window at a time, and the stages
// in between make no sense. It is updated when the game is about to
// read input, and when it draws in a window which might be a panel.
void v6_touch_window(Window *win)
{
    int window = win - &windows[0];

    if (!v6_is_generic() || window < 2 || window > 7 || v6_is_story_window(window)) {
        return;
    }

    // Drawing in a window which is covered by a later one brings it
    // back. Moments Out of Time doesn't move its menu bar into place
    // again after undoing the command which turned it off.
    if (!is_v6_panel(win) && v6_window_covered(window)) {
        v6_geometry[window].stamp = ++v6_geometry_stamp;
        v6_layout_dirty = true;
    }

    if (v6_layout_dirty) {
        v6_apply_layout();
    }
}

void v6_layout_before_input(bool line_input)
{
    if (!v6_is_generic()) {
        return;
    }

    int window = curwin - &windows[0];

    if (line_input && window != v6_story_window && !is_v6_panel(curwin)) {
        v6_story_window = window;
        v6_layout_dirty = true;
    }

    if (v6_layout_dirty) {
        v6_apply_layout();
    }
}

// The panels of the non-Infocom V6 games are autosaved along with the
// other Glk windows, but they only make sense together with the window
// geometry they were laid out from. Without it the panels would be
// closed at the first input after an autorestore. Windows which are
// redirected to the main window share its Glk window, so the tags
// can't tell which of them is the current or the main one either.
void v6_stash_layout(library_state_data *dat)
{
    dat->v6_layout_valid = v6_is_generic();
    if (!dat->v6_layout_valid) {
        return;
    }

    int *value = dat->v6_layout;

    *value++ = v6_story_window;
    *value++ = curwin - &windows[0];
    *value++ = mainwin - &windows[0];
    *value++ = upperwin - &windows[0];

    for (const auto &geometry : v6_geometry) {
        *value++ = geometry.y;
        *value++ = geometry.x;
        *value++ = geometry.height;
        *value++ = geometry.width;
        *value++ = (geometry.sized ? 1 : 0) | (geometry.moved ? 2 : 0);
        *value++ = static_cast<int>(geometry.stamp);
    }
}

void v6_recover_layout(const library_state_data *dat)
{
    if (!v6_is_generic() || !dat->v6_layout_valid) {
        return;
    }

    const int *value = dat->v6_layout;

    auto window_at = [](int index, Window *fallback) {
        return index >= 0 && index < 8 && windows[index].id != nullptr ? &windows[index] : fallback;
    };

    int story = *value++;
    v6_story_window = story >= 0 && story < 8 ? story : -1;
    curwin = window_at(*value++, curwin);
    mainwin = window_at(*value++, mainwin);
    mainwin->has_echo = true;
    upperwin = window_at(*value++, upperwin);

    v6_geometry_stamp = 0;
    for (auto &geometry : v6_geometry) {
        geometry.y = *value++;
        geometry.x = *value++;
        geometry.height = *value++;
        geometry.width = *value++;
        int flags = *value++;
        geometry.sized = (flags & 1) != 0;
        geometry.moved = (flags & 2) != 0;
        geometry.stamp = *value++;
        v6_geometry_stamp = std::max(v6_geometry_stamp, geometry.stamp);
    }

    // The panels on screen are those of this layout, unless something
    // is amiss, in which case they are laid out again.
    std::array<bool, 8> wanted{};
    v6_layout_signature = v6_signature(v6_layout_bands(), wanted);
    v6_layout_dirty = false;
    for (int i = 2; i < 8; i++) {
        if (is_v6_panel(&windows[i]) != wanted[i]) {
            v6_layout_signature.clear();
            v6_layout_dirty = true;
        }
    }
}
