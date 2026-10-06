// Force-included (-include image_glk.h) into every interpreter source built for
// an image probe (plus/Makefile.headless, taylor/Makefile.headless). It renames
// the Glk window, event and gestalt entry points so the unmodified interpreter
// talks to the fake window layer in fake_glk_window.c (framebuffer-backed
// graphics window) and the canned key presses of image_glk.c, while cheapglk
// keeps providing streams, files and styles.
//
// The rename happens before glk.h is read, so the prototypes move with it.

#ifndef IMAGE_GLK_H
#define IMAGE_GLK_H

#define glk_exit                        image_exit
#define glk_gestalt                     fakeglk_gestalt
#define glk_gestalt_ext                 fakeglk_gestalt_ext
#define glk_style_measure               fakeglk_style_measure

#define glk_set_window                  fakeglk_set_window
#define glk_window_open                 fakeglk_window_open
#define glk_window_close                fakeglk_window_close
#define glk_window_clear                fakeglk_window_clear
#define glk_window_iterate              fakeglk_window_iterate
#define glk_window_get_size             fakeglk_window_get_size
#define glk_window_get_parent           fakeglk_window_get_parent
#define glk_window_set_arrangement      fakeglk_window_set_arrangement
#define glk_window_get_stream           fakeglk_window_get_stream
#define glk_window_set_echo_stream      fakeglk_window_set_echo_stream
#define glk_window_move_cursor          fakeglk_window_move_cursor
#define glk_window_fill_rect            fakeglk_window_fill_rect
#define glk_window_set_background_color fakeglk_window_set_background_color

#define glk_select                      image_select
#define glk_request_char_event          fakeglk_request_char_event
#define glk_request_line_event          image_request_line_event
#define glk_cancel_char_event           fakeglk_cancel_char_event
#define glk_request_timer_events        fakeglk_request_timer_events

#endif
