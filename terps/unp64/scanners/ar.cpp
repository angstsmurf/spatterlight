/* This is a cut-down version of UNP64 with only the bare minimum
 * needed to decompress a number of Commodore 64 adventure games.
 * It is distributed under the zlib License by kind permission of
 * the original authors Magnus Lind and iAN CooG.
 */

/*
 UNP64 - generic Commodore 64 prg unpacker
 (C) 2008-2022 iAN CooG/HVSC Crew^C64Intros
 original source and idea: testrun.c, taken from exo20b7

 Follows original disclaimer
 */

/*
 * Copyright (c) 2002 - 2023 Magnus Lind.
 *
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

/*
 C++ version based on code adapted to ScummVM by Avijeet Maurya
 */

#include "exo_util.h"
#include "types.h"
#include "unp64.h"

namespace Unp64 {

/* Action Replay and similar freeze-cartridge dumps.
 *
 * Ported from the original UNP64 Scn_AR. Only the single-file branches are
 * carried over here: the AR3/AR4/ARU freezer format, the snapshot freezer
 * ("SSNAP"), and the single-file Freeze Machine format. The two-file freeze
 * variants from upstream (which read a companion file off disk) are omitted,
 * as this cut-down build unpacks a single in-memory PRG. */
void scnAR(UnpStr *unp) {
    unsigned char *mem;
    int q, p;
    if (unp->_idFlag)
        return;
    mem = unp->_mem;
    if (unp->_depAdr == 0) {
        if (u32eq(mem + 0x80f, 0xDD0D8D7F) &&
            u32eq(mem + 0x927, 0x4CE7D0CA)) {
            for (q = 0xa00, p = 0; p < 0x100; p++) {
                if (mem[q + p] == 0x99 &&
                    mem[q + p + 1] == mem[0x092b] &&
                    mem[q + p + 2] == mem[0x092c] &&
                    mem[q + p + 0x11] == 0x40) { /* Action Replay v4 */
                    unp->_depAdr = 0x100 + p;
                    break;
                }
                if (mem[q + p] == 0x99 &&
                    mem[q + p + 1] == mem[0x092b] &&
                    mem[q + p + 2] == mem[0x092c] &&
                    mem[q + p + 0x10] == 0x40) { /* Action Replay v3.x */
                    unp->_depAdr = 0x100 + p;
                    break;
                }
            }
            if (!unp->_depAdr) { /* Action Replay, unknown variant */
                unp->_depAdr = mem[0x092b] | mem[0x092c] << 8;
            }
            unp->_rtiFrc = 1;
            if (unp->_info->_run == -1)
                unp->_forced = 0x080d;
        } else if (u32eq(mem + 0x812, 0xDD0D8DDC)) { /* Super Snapshot */
            for (p = 0x900; p < 0xcfff; p++) {
                if (u32eq(mem + p, 0xA9DC0E8D) &&
                    u32eq(mem + p + 0x0a, 0xA9DD0E8D) &&
                    mem[p + 0x18] == 0x4c) {
                    unp->_depAdr = mem[p + 0x19] | mem[p + 0x1a] << 8;
                    unp->_rtiFrc = 1;
                    if (unp->_info->_run == -1)
                        unp->_forced = 0x080d;
                    break;
                }
            }
        } else if (u32eq(mem + 0x80f, 0xDD0D8D7F) &&
                   u32eq(mem + 0x8ef, 0x0330BD01) &&
                   u32eq(mem + 0x8fa, 0x7E4C00A0) &&
                   u32eq(mem + 0xbc8, 0x4C01C6DF)) { /* Freeze Machine */
            unp->_depAdr = mem[0x0bcc] | mem[0x0bcd] << 8;
            unp->_strMem = 2;
            unp->_endAdr = 0x10000;
            unp->_rtiFrc = 1;
            if (unp->_info->_run == -1)
                unp->_forced = 0x080d;
        }
    }
    if (unp->_depAdr != 0) {
        unp->_idFlag = 1;
        return;
    }
}

} // End of namespace Unp64
