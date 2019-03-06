
/* savegame.h
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


#ifndef savegame_h_INCLUDED
#define savegame_h_INCLUDED

#include "../tools/types.h"

#define LAST_SAVEGAME_FILENAME_BYTE_LENGTH (MAXIMUM_SAVEGAME_NAME_LENGTH * MB_LEN_MAX + 1)

#ifndef savegame_c_INCLUDED
extern z_ucs last_savegame_filename[];
#endif /* savegame_c_INCLUDED */

int save_game_to_stream(uint16_t address, uint16_t length, z_file *save_file,
    bool evaluate_result);
void save_game(uint16_t address, uint16_t length, char *filename,
    bool skip_asking_for_filename, bool evaluate_result, char *directory);
int restore_game_from_stream(uint16_t address, uint16_t length,
    z_file *iff_file, bool evaluate_result);
int restore_game(uint16_t address, uint16_t length, char *filename, 
    bool skip_asking_for_filename, bool evaluate_result, char *directory);
#ifndef DISABLE_FILELIST
bool detect_saved_game(char *file_to_check, char **story_file_to_load);
#endif // DISABLE_FILELIST

void opcode_save_0op(void);
void opcode_save_ext(void);
void opcode_restore_0op(void);
void opcode_restore_ext(void);

#endif /* savegame_h_INCLUDED */

