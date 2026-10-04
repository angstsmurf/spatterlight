// Force-included (-include scene_glk.h) into every interpreter source built for
// build/scenetest. It renames the Glk window, event and gestalt entry points so
// the unmodified interpreter talks to the fake window layer in scenetest.c
// (framebuffer-backed graphics window, scripted events) while cheapglk keeps
// providing streams, files, styles and unicode.
//
// The rename happens before glk.h is read, so the prototypes move with it.

#ifndef SCENE_GLK_H
#define SCENE_GLK_H

#define glk_exit                        scene_exit
#define glk_gestalt                     scene_gestalt
#define glk_gestalt_ext                 scene_gestalt_ext
#define glk_style_measure               scene_style_measure

#define glk_set_window                  scene_set_window
#define glk_window_open                 scene_window_open
#define glk_window_close                scene_window_close
#define glk_window_clear                scene_window_clear
#define glk_window_get_size             scene_window_get_size
#define glk_window_get_parent           scene_window_get_parent
#define glk_window_set_arrangement      scene_window_set_arrangement
#define glk_window_get_stream           scene_window_get_stream
#define glk_window_set_echo_stream      scene_window_set_echo_stream
#define glk_window_move_cursor          scene_window_move_cursor
#define glk_window_fill_rect            scene_window_fill_rect
#define glk_window_set_background_color scene_window_set_background_color

#define glk_select                      scene_select
#define glk_request_char_event          scene_request_char_event
#define glk_request_char_event_uni      scene_request_char_event_uni
#define glk_request_line_event_uni      scene_request_line_event_uni
#define glk_cancel_char_event           scene_cancel_char_event
#define glk_request_timer_events        scene_request_timer_events

#endif
