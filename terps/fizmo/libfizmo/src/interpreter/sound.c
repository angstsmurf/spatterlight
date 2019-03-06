
/* sound.c
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


#include "../tools/i18n.h"
#include "../tools/tracelog.h"
#include "sound.h"
#include "fizmo.h"
#include "zpu.h"


void opcode_sound_effect(void)
{
  int effect_number=1, effect=2, volume=8, repeats=255;
  uint16_t routine = 0;

  // VAR:245 15 5/3 sound_effect number effect volume routine
  TRACE_LOG("Opcode: SOUND_EFFECT\n");

  if (active_sound_interface != NULL)
  {
    if (number_of_operands >= 1)
    {
      effect_number = (int16_t)op[0];
      if (effect_number < 1)
        effect_number = 1;

      if (number_of_operands >= 2)
      {
        effect = (int16_t)op[1];
        if ( (effect < 1) || (effect > 4) )
          return;

        if (number_of_operands >= 3)
        {
          TRACE_LOG("op[2]: %d.\n", op[2]);

          volume = op[2] & 0xff;
          repeats = op[2] >> 8;

          if ( (volume < 1) || (volume > 8) )
            volume = 8;

          if (ver >= 5)
          {
            if (repeats < 1)
              // 255 loop forever.
              repeats = 255;
            else if (repeats > 255)
              repeats = 254;
          }
          else
            // -1 means take from file (repeat is also stored there).
            repeats = -1;

          if (number_of_operands >= 4)
            routine = op[3];
        }
      }
    }

    TRACE_LOG("Nr: %d, Effect: %d, Volume: %d, Repeats: %d, Routine: %d.\n",
        effect_number, effect, volume, repeats, routine);

    if (effect == 1)
      active_sound_interface->prepare_sound(effect_number, volume, repeats);
    else if (effect == 2)
      active_sound_interface->play_sound(effect_number,volume,repeats,routine);
    else if (effect == 3)
      active_sound_interface->stop_sound(effect_number);
    else if (effect == 4)
      active_sound_interface->finish_sound(effect_number);
  }
}

