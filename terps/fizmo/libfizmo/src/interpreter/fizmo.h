
/* fizmo.h
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2009-2017 Christoph Ender.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef fizmo_h_INCLUDED 
#define fizmo_h_INCLUDED

#include <stdlib.h>
#include <stdarg.h>

#include "../screen_interface/screen_interface.h"
#include "../sound_interface/sound_interface.h"
#include "../blorb_interface/blorb_interface.h"
#include "blockbuf.h"

#define LIBFIZMO_VERSION "0.7.16"

#define FIZMO_INTERPRETER_NUMBER 6
/*
From: http://www.logicalshift.demon.co.uk/unix/zoom/manual/configXwin.html
 1 - Infocom's internal debugging interpreter
 2 - Applie IIe interpreter
 3 - Macintosh
 4 - Amiga (colour behaviour can change if you specify this)
 5 - Atari ST
 6 - IBM PC
 7 - Commodore 128
 8 - Commodore 64
 9 - Apple IIc
10 - Apple IIgs
11 - Tandy Color
*/

#define FIZMO_INTERPRETER_REVISION 'b';

#define SCORE_MODE_UNKNOWN 0
#define SCORE_MODE_SCORE_AND_TURN 1
#define SCORE_MODE_TIME 2

#define OBEYS_SPEC_MAJOR_REVISION_NUMER 1
#define OBEYS_SPEC_MINOR_REVISION_NUMER 0


int fizmo_register_screen_interface(
    struct z_screen_interface *screen_interface);
void fizmo_register_sound_interface(
    struct z_sound_interface *sound_interface);
void fizmo_register_blorb_interface(
    struct z_blorb_interface *blorb_interface);
#ifndef DISABLE_OUTPUT_HISTORY
void fizmo_register_paragraph_attribute_function(
    void (*new_paragraph_attribute_function)(int *parameter1, int *parameter2));
void fizmo_register_paragraph_removal_function(
    void (*new_paragraph_removal_function)(int parameter1, int parameter2));
#endif // DISABLE_OUTPUT_HISTORY

void fizmo_start(z_file* story_stream, z_file *blorb_stream,
    z_file *restore_on_start_file);
void fizmo_new_screen_size(uint16_t width, uint16_t height);

void write_interpreter_info_into_header();
int close_interface(z_ucs *error_message);
void *fizmo_malloc(size_t size);
void *fizmo_realloc(void *ptr, size_t size);
char *fizmo_strdup(char *s1);
int ensure_mem_size(char **ptr, int *current_size, int size);
void ensure_dot_fizmo_dir_exists();
char *quote_special_chars(char *s);
char *unquote_special_chars(char *s);
#ifndef DISABLE_CONFIGFILES
char *get_fizmo_config_dir_name();
int parse_fizmo_config_files();
#endif // DISABLE_CONFIGFILES

#ifndef fizmo_c_INCLUDED 
extern struct commandline_parameter *interpreter_commandline_parameters[];
extern struct z_screen_interface *active_interface;
extern struct z_sound_interface *active_sound_interface;
extern struct z_story *active_z_story;
extern uint8_t ver;
extern uint8_t *header_extension_table;
extern uint8_t header_extension_table_size;

#ifndef DISABLE_BLOCKBUFFER
extern BLOCKBUF *upper_window_buffer;
#endif /* DISABLE_BLOCKBUFFER */

#ifndef DISABLE_OUTPUT_HISTORY
extern void (*paragraph_attribute_function)(int *parameter1, int *parameter2);
extern void (*paragraph_removal_function)(int parameter1, int parameter2);
#endif // DISABLE_OUTPUT_HISTORY

#endif /* fizmo_c_INCLUDED */

#endif /* fizmo_h_INCLUDED */

