// zorkzero_internal.hpp
//
// Declarations shared by zorkzero.cpp and zorkzero_minigames.cpp only.

#ifndef zorkzero_internal_hpp
#define zorkzero_internal_hpp

#include <cstdint>

// Window 3 holds the encyclopedia caption text
#define Z0_ENCYCLOPEDIA_WINDOW windows[3]

constexpr int F_SPLIT = 422;
constexpr int F_BOTTOM = 423;

constexpr int PBOZ_SPLIT = 402;
constexpr int PBOZ_BOTTOM = 424;

constexpr int EXIT_BOX = 463;
constexpr int SHOW_MOVES_BOX = 464;
constexpr int RESTART_BOX = 466;
constexpr int DIM_SHOW_MOVES_BOX = 467;
constexpr int DIM_RESTART_BOX = 469;

// In zorkzero.cpp
void z0_hide_right_status(void);
void z0_close_encyclopedia_window(void);
void z0_set_bordered_status_colors(void);
void ADJUST_TEXT_WINDOW(int id);
void update_blink_coordinates(uint16_t x, uint16_t y);

// In zorkzero_minigames.cpp
void z0_draw_peggleboz_box_image(uint16_t pic);
bool redraw_minigame_on_resize(uint16_t current_split);
void restore_fanucci_command_grid(void);
void restore_selected_peg(void);

#endif /* zorkzero_internal_hpp */
