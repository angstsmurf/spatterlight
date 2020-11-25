/*-
 * Copyright 2017 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2 or 3, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "patches.h"
#include "memory.h"
#include "zterp.h"

struct patch
{
  char *title;
  char serial[sizeof header.serial];
  uint16_t release;
  uint16_t checksum;
  uint32_t addr;
  size_t n;
  uint8_t *expected;
  uint8_t *replacement;
};

#define B(...)	(uint8_t[]){__VA_ARGS__}

static const struct patch patches[] =
{
  /* Beyond Zork tries to treat a dictionary word as an object in two
   * places. This affects all releases and so needs to be patched in two
   * places each release, resulting in several patch entries.
   *
   * The code looks something like:
   *
   * [ KillFilm;
   *   @clear_attr circlet 3
   *   @call_vn ReplaceSyn "circlet" "film" "zzzp"
   *   @call_vn ReplaceAdj "circlet" "swirling" "zzzp"
   *   @rfalse;
   * ];
   *
   * For the calls, "circlet" is the dictionary word, not the object. In
   * both ReplaceSyn and ReplaceAdj, the first call is @get_prop_addr
   * with "circlet" as the object, which is invalid. According to
   * http://ifarchive.org/if-archive/infocom/interpreters/zip/zip_bugs.txt,
   * interpreters can return 0 in this particular case. Conveniently,
   * both ReplaceSyn and ReplaceAdj immediately return false if
   * @get_prop_addr returns 0, so it’s fine to avoid calling them
   * altogether. Since the two calls to ReplaceSyn and ReplaceAdj are
   * superfluous, and KillFilm always returns false, the first byte of
   * the first @call_vn is replaced with @rfalse. This leaves junk
   * instructions afterward, but they’ll never be reached, so it doesn't
   * matter.
   */
  {
    .title = "Beyond Zork", .serial = "870915", .release = 47, .checksum = 0x3ff4,
    .addr = 0x2f8e2, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },
  {
    .title = "Beyond Zork", .serial = "870915", .release = 47, .checksum = 0x3ff4,
    .addr = 0x2f8fe, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },
  {
    .title = "Beyond Zork", .serial = "870917", .release = 49, .checksum = 0x24d6,
    .addr = 0x2f8b2, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },
  {
    .title = "Beyond Zork", .serial = "870917", .release = 49, .checksum = 0x24d6,
    .addr = 0x2f8ce, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },
  {
    .title = "Beyond Zork", .serial = "870923", .release = 51, .checksum = 0x0cbe,
    .addr = 0x2f75e, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },
  {
    .title = "Beyond Zork", .serial = "870923", .release = 51, .checksum = 0x0cbe,
    .addr = 0x2f77a, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },
  {
    .title = "Beyond Zork", .serial = "871221", .release = 57, .checksum = 0xc5ad,
    .addr = 0x2fc6e, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },
  {
    .title = "Beyond Zork", .serial = "871221", .release = 57, .checksum = 0xc5ad,
    .addr = 0x2fc8a, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },
  {
    .title = "Beyond Zork", .serial = "880610", .release = 60, .checksum = 0xa49d,
    .addr = 0x2fbfa, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },
  {
    .title = "Beyond Zork", .serial = "880610", .release = 60, .checksum = 0xa49d,
    .addr = 0x2fc16, .n = 1,
    .expected = B(0xf9),
    .replacement = B(0xb1),
  },

  /* This is in a routine which iterates over all attributes of an
   * object, but due to an off-by-one error, attribute 48 (0x30) is
   * included, which is not valid, as the last attribute is 47 (0x2f);
   * there are 48 attributes but the count starts at 0.
   */
  {
    .title = "Sherlock", .serial = "871214", .release = 21, .checksum = 0x79b2,
    .addr = 0x223ac, .n = 1, .expected = B(0x30), .replacement = B(0x2f),
  },
  {
    .title = "Sherlock", .serial = "880112", .release = 22, .checksum = 0xcb96,
    .addr = 0x225a4, .n = 1, .expected = B(0x30), .replacement = B(0x2f),
  },
  {
    .title = "Sherlock", .serial = "880127", .release = 26, .checksum = 0x26ba,
    .addr = 0x22818, .n = 1, .expected = B(0x30), .replacement = B(0x2f),
  },
  {
    .title = "Sherlock", .serial = "880324", .release = 4, .checksum = 0x7086,
    .addr = 0x22498, .n = 1, .expected = B(0x30), .replacement = B(0x2f),
  },

  /* The operands to @get_prop here are swapped, so swap them back. */
  {
    .title = "Stationfall", .serial = "870326", .release = 87, .checksum = 0x71ae,
    .addr = 0xd3d4, .n = 3, .expected = B(0x31, 0x0c, 0x73), .replacement = B(0x51, 0x73, 0x0c),
  },
  {
    .title = "Stationfall", .serial = "870430", .release = 107, .checksum = 0x2871,
    .addr = 0xe3fe, .n = 3, .expected = B(0x31, 0x0c, 0x77), .replacement = B(0x51, 0x77, 0x0c),
  },

  /* The Solid Gold (V5) version of Wishbringer calls @show_status, but
   * that is an illegal instruction outside of V3. Convert it to @nop.
   */
  {
    .title = "Wishbringer", .serial = "880706", .release = 23, .checksum = 0x4222,
    .addr = 0x1f910, .n = 1, .expected = B(0xbc), .replacement = B(0xb4),
  },

  /* Robot Finds Kitten attempts to sleep with the following:
   *
   * [ Func junk;
   *   @aread junk 0 10 PauseFunc -> junk;
   * ];
   *
   * However, since “junk” is a local variable with value 0 instead of a
   * text buffer, this is asking to read from/write to address 0. This
   * works in some interpreters, but Bocfel is more strict, and aborts
   * the program. Rewrite this instead to:
   *
   * @read_char 1 10 PauseFunc -> junk;
   * @nop; ! This is for padding.
   */
  {
    .title = "Robot Finds Kitten", .serial = "130320", .release = 7, .checksum = 0x4a18,
    .addr = 0x4912, .n = 8,
    .expected = B(0xe4, 0x94, 0x05, 0x00, 0x0a, 0x12, 0x5a, 0x05),
    .replacement = B(0xf6, 0x53, 0x01, 0x0a, 0x12, 0x5a, 0x05, 0xb4),
  },

  { .replacement = NULL },
};

void apply_patches(void)
{
  for(const struct patch *patch = patches; patch->replacement != NULL; patch++)
  {
    if(memcmp(patch->serial, header.serial, sizeof header.serial) == 0 &&
       patch->release == header.release &&
       patch->checksum == header.checksum &&
       patch->addr >= header.static_start &&
       patch->addr + patch->n < memory_size &&
       memcmp(&memory[patch->addr], patch->expected, patch->n) == 0)
    {
      memcpy(&memory[patch->addr], patch->replacement, patch->n);
    }
  }
}
