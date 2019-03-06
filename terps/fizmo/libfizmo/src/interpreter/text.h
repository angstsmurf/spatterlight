
/* text.h
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


#ifndef text_h_INCLUDED
#define text_h_INCLUDED

#include "../tools/types.h"

#define Z_UCS_OUTPUT_BUFFER_SIZE 128

// According to Z-Spec 1.0, abbreviations may not continue further
// abbreviations.
#define MAX_ABBREVIATION_DEPTH 1

#ifndef text_c_INCLUDED
extern uint8_t alphabet_table_v1[];
extern uint8_t alphabet_table_after_v1[];
extern z_ucs z_ucs_newline_string[];
#endif /* text_c_INCLUDED */

z_ucs zscii_input_char_to_z_ucs(zscii zscii_input);
z_ucs zscii_output_char_to_z_ucs(zscii zscii_output);
zscii unicode_char_to_zscii_input_char(z_ucs unicode_char);
void opcode_print_paddr(void);
void opcode_read(void);
void opcode_print(void);
void opcode_print_num(void);
void opcode_print_char(void);
void opcode_new_line(void);
void opcode_print_obj(void);
void opcode_print_ret(void);
void opcode_print_addr(void);
void opcode_show_status(void);
void opcode_read_char(void);
void opcode_tokenise(void);
void opcode_print_table(void);
void opcode_encode_text(void);
void opcode_print_unicode(void);
void opcode_check_unicode(void);

// This is used by the interface code outside the module:
/*@-exportlocal@*/
void display_status_line(void);
/*@+exportlocal@*/

#endif /* text_h_INCLUDED */

