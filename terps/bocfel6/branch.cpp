// Copyright 2010-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

#include "branch.h"
#include "memory.h"
#include "process.h"
#include "stack.h"
#include "types.h"
#include "util.h"

void branch_if(bool do_branch)
{
    uint8_t branch;
    uint16_t offset;

    branch = byte(pc++);

    if (!do_branch) {
        branch ^= 0x80;
    }

    offset = branch & 0x3f;

    if ((branch & 0x40) == 0) {
        offset = (offset << 8) | byte(pc++);

        // Get the sign right.
        if ((offset & 0x2000) == 0x2000) {
            offset |= 0xc000;
        }
    }

    if ((branch & 0x80) == 0x80) {
        if (offset > 1) {
            pc += as_signed(offset) - 2;
            ZASSERT(pc < memory_size, "branch to invalid address 0x%lx", static_cast<unsigned long>(pc));
        } else {
            do_return(offset);
        }
    }
}

void zjump()
{
    // -= 2 because pc has been advanced past the jump instruction.
    pc += as_signed(zargs[0]);
    pc -= 2;

    ZASSERT(pc < memory_size, "@jump to invalid address 0x%lx", static_cast<unsigned long>(pc));
}

void zjz()
{
    fprintf(stderr, "z_jz %d (%d)\n", zargs[0], (short) zargs[0] == 0);
    branch_if(zargs[0] == 0);
}

void zje()
{
    if (znargs == 1) {
        fprintf(stderr, "zje %d\n", zargs[0]);
        branch_if(false);
    } else if (znargs == 2) {
        fprintf(stderr, "zje %d %d\n", zargs[0], zargs[1]);
        branch_if(zargs[0] == zargs[1]);
    } else if (znargs == 3) {
        fprintf(stderr, "zje %d %d %d\n", zargs[0], zargs[1], zargs[2]);
        branch_if(zargs[0] == zargs[1] || zargs[0] == zargs[2]);
    } else {
        fprintf(stderr, "zje %d %d %d %d\n", zargs[0], zargs[1], zargs[2], zargs[3]);
        branch_if(zargs[0] == zargs[1] || zargs[0] == zargs[2] || zargs[0] == zargs[3]);
    }
}

void zjl()
{
    fprintf(stderr, "zjl: branch if %d < %d\n", as_signed(zargs[0]), as_signed(zargs[1]));
    branch_if(as_signed(zargs[0]) < as_signed(zargs[1]));
}

void zjg()
{
    fprintf(stderr, "zjg: branch if %d > %d\n", as_signed(zargs[0]), as_signed(zargs[1]));
    branch_if(as_signed(zargs[0]) > as_signed(zargs[1]));
}
