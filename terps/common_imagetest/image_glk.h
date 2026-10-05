// Force-included (-include image_glk.h) into every interpreter source built for
// an image probe (plus/Makefile.headless, taylor/Makefile.headless). It renames
// the Glk window, event and gestalt entry points so the unmodified interpreter
// talks to the fake window layer in image_glk.c (framebuffer-backed graphics
// window, canned key presses) while cheapglk keeps providing streams, files
// and styles.
//
// The rename happens before glk.h is read, so the prototypes move with it.

#ifndef IMAGE_GLK_H
#define IMAGE_GLK_H

#define glk_exit                        image_exit
#define glk_gestalt                     image_gestalt
#define glk_gestalt_ext                 image_gestalt_ext
#define glk_style_measure               image_style_measure

#define glk_set_window                  image_set_window
#define glk_window_open                 image_window_open
#define glk_window_close                image_window_close
#define glk_window_clear                image_window_clear
#define glk_window_iterate              image_window_iterate
#define glk_window_get_size             image_window_get_size
#define glk_window_get_parent           image_window_get_parent
#define glk_window_set_arrangement      image_window_set_arrangement
#define glk_window_get_stream           image_window_get_stream
#define glk_window_set_echo_stream      image_window_set_echo_stream
#define glk_window_move_cursor          image_window_move_cursor
#define glk_window_fill_rect            image_window_fill_rect
#define glk_window_set_background_color image_window_set_background_color

#define glk_select                      image_select
#define glk_request_char_event          image_request_char_event
#define glk_request_line_event          image_request_line_event
#define glk_cancel_char_event           image_cancel_char_event
#define glk_request_timer_events        image_request_timer_events

#endif
