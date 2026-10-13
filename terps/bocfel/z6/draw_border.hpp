//
//  draw_border.hpp
//  bocfel6
//
//  The borders and pillars around the play area of Arthur, Shogun and
//  Zork Zero, composited into the shared pixmap of draw_image.cpp.
//

#ifndef draw_border_hpp
#define draw_border_hpp

extern "C" {
#include "glk.h"
}

void draw_arthur_side_images(winid_t winid);
void extend_pillars(int top_cut, int foot_height, int total_height,
                    int pillar_height, int overlap, bool flip,
                    bool bw_hint_foot, int desired_height = 0);
void extend_underground_pillars(int top_cut, int foot_height, int total_height,
                                int pillar_height, int stone_height, int pillar_width);
void extend_jungle_pillars(int top_cut, int total_height,
                           int pillar_height, int overlap);
void extend_mac_bw_castle_pillars(void);
// Which flavor of border draw_border_common() is being asked to render.
// The three variants differ in whether the top covering rectangle is
// drawn and which color it uses.
enum class BorderKind {
    Hint,             // hint screen border (Zork Zero, Shogun)
    ShogunGame,       // Shogun gameplay border (always covers the top bar)
    ShogunStartMenu,  // Shogun start-menu border (no covering rect)
};

void draw_border_common(int border, int BL, int BR,
                        int border_height, int border_top, int pillar_top,
                        int left_margin, int bottom_offset,
                        BorderKind kind);

#endif /* draw_border_hpp */
