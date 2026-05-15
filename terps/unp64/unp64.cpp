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


#include <cstring>
#include <cstdio>
#include <memory>

#include "types.h"
#include "globals.h"
#include "6502_emu.h"
#include "exo_util.h"
#include "unp64.h"

#include "unp64_interface.h"

namespace Unp64 {

/* Reset the unpacker state to defaults. Called at startup and again
 * between iterations when a payload is nested inside another packer
 * (the -c switch / _recurs path). Defaults assume a BASIC-style PRG
 * loaded at $0801 with a SYS line transferring control around $0800. */
void reinitUnp(void) {
	_G(_unp)._idFlag = 0;
	_G(_unp)._forced = 0;
	_G(_unp)._strMem = 0x800;
	_G(_unp)._retAdr = 0x800;
	_G(_unp)._depAdr = 0;
	_G(_unp)._endAdr = 0x10000;
	_G(_unp)._rtAFrc = 0;
	_G(_unp)._wrMemF = 0;
	_G(_unp)._lfMemF = 0;
	_G(_unp)._exoFnd = 0;
	_G(_unp)._ecaFlg = 0;
	_G(_unp)._fEndBf = 0;
	_G(_unp)._fEndAf = 0;
	_G(_unp)._fStrAf = 0;
	_G(_unp)._fStrBf = 0;
	_G(_unp)._mon1st = 0;
}

/* True if pc is one of the BASIC ROM entry points that signal
 * "return to BASIC and RUN the program" — i.e. the unpacker has
 * finished and is handing control back to a fresh SYS/RUN. */
int isBasicRun1(int pc) {
	if (pc == 0xa7ae || pc == 0xa7ea || pc == 0xa7b1 || pc == 0xa474 || pc == 0xa533 || pc == 0xa871 || pc == 0xa888 || pc == 0xa8bc)
		return 1;
	else
		return 0;
}

/* Broader match for "we're now executing in BASIC's interpreter
 * dispatch loop", which also indicates the unpacker has finished. */
int isBasicRun2(int pc) {
	if (isBasicRun1(pc) || ((pc >= 0xA57C) && (pc <= 0xA659)) || pc == 0xa660 || pc == 0xa68e)
		return 1;
	else
		return 0;
}

/* Parse a UNP64 switch string (e.g. "-d8000 -t9fff -c") into up to four
 * argv-style tokens. Returns the number of tokens written into settings[].
 * strtok_r keeps the function reentrant. */
static int parseSwitchString(const char *switches, char settings[4][64]) {
	if (switches == NULL)
		return 0;
	size_t string_length = std::strlen(switches);
	if (string_length == 0 || string_length >= 100)
		return 0;

	char string[100];
	snprintf(string, sizeof string, "%s", switches);

	int numSettings = 0;
	char *saveptr = NULL;
	char *setting = strtok_r(string, " ", &saveptr);
	while (setting != NULL && numSettings < 4) {
		snprintf(settings[numSettings], sizeof settings[numSettings], "%s", setting);
		numSettings++;
		setting = strtok_r(NULL, " ", &saveptr);
	}
	return numSettings;
}

/* Interpret parsed switches and mutate the unpacker state accordingly.
 * Starts from `startIdx` so the caller can pre-consume the leading -f. */
static void applySwitches(char settings[4][64], int numSettings, int startIdx,
                          LoadInfo *info, int *iterMax) {
	int p = startIdx;
	while (p < numSettings) {
		if (settings[p][0] == '-') {
			switch (settings[p][1]) {
			case '-':
				/* "--" terminates option parsing. */
				p = numSettings;
				break;
			case 'e':
				/* -e<addr>: force entry point (override BASIC SYS). */
				strToInt(settings[p] + 2, &_G(_unp)._forced);
				_G(_unp)._forced &= 0xffff;
				if (_G(_unp)._forced < 0x1)
					_G(_unp)._forced = 0;
				break;
			case 'a':
				/* -a: dump the full 64K, ignoring any detected
				 * start/end hints from scanners. */
				_G(_unp)._strMem = 2;
				_G(_unp)._endAdr = 0x10001;
				_G(_unp)._fEndAf = 0;
				_G(_unp)._fStrAf = 0;
				_G(_unp)._strAdC = 0;
				_G(_unp)._endAdC = 0;
				_G(_unp)._monEnd = 0;
				_G(_unp)._monStr = 0;
				break;
			case 'r':
				/* -r<addr>: emulation halts when pc >= addr. */
				strToInt(settings[p] + 2, &_G(_unp)._retAdr);
				_G(_unp)._retAdr &= 0xffff;
				break;
			case 'R':
				/* -R<addr>: like -r but require an exact pc match. */
				strToInt(settings[p] + 2, &_G(_unp)._retAdr);
				_G(_unp)._retAdr &= 0xffff;
				_G(_unp)._rtAFrc = 1;
				break;
			case 'd':
				/* -d<addr>: depack-done address; first loop exits when pc == addr. */
				strToInt(settings[p] + 2, &_G(_unp)._depAdr);
				_G(_unp)._depAdr &= 0xffff;
				break;
			case 't':
				/* -t<addr>: explicit end-of-data address for output. */
				strToInt(settings[p] + 2, &_G(_unp)._endAdr);
				_G(_unp)._endAdr &= 0xffff;
				if (_G(_unp)._endAdr >= 0x100)
					_G(_unp)._endAdr++;
				break;
			case 'u':
				/* -u: zero out bytes that the unpacker did not modify. */
				_G(_unp)._wrMemF = 1;
				break;
			case 'l':
				/* -l: trim a trailing tail of unchanged bytes from the dump. */
				_G(_unp)._lfMemF = info->_end;
				break;
			case 's':
				/* -s: don't pre-seed the stack page with the SYS snapshot. */
				_G(_unp)._fStack = 0;
				break;
			case 'x':
				break;
			case 'B':
				//copyRoms[0][1] = 1;
				break;
			case 'K':
				//copyRoms[1][1] = 1;
				break;
			case 'c':
				/* -c: re-run unpacker on the output (handles nested packers). */
				_G(_unp)._recurs++;
				break;
			case 'm': // keep undocumented for now
				/* -m<n>: override the per-loop instruction cap. */
				strToInt(settings[p] + 2, iterMax);
			}
		}
		p++;
	}
}

/* After both emulation phases complete, fold any pending end/start address
 * adjustments (constants and register-derived offsets) into _endAdr/_strMem. */
static void applyEndStartOffsets(const CpuCtx *r) {
	if (_G(_unp)._endAdC & 0xffff) {
		_G(_unp)._endAdr += (_G(_unp)._endAdC & 0xffff);
		_G(_unp)._endAdr &= 0xffff;
	}

	if (_G(_unp)._endAdC & EA_USE_A) {
		_G(_unp)._endAdr += r->_a;
		_G(_unp)._endAdr &= 0xffff;
	}

	if (_G(_unp)._endAdC & EA_USE_X) {
		_G(_unp)._endAdr += r->_x;
		_G(_unp)._endAdr &= 0xffff;
	}

	if (_G(_unp)._endAdC & EA_USE_Y) {
		_G(_unp)._endAdr += r->_y;
		_G(_unp)._endAdr &= 0xffff;
	}

	if (_G(_unp)._strAdC & 0xffff) {
		_G(_unp)._strMem += (_G(_unp)._strAdC & 0xffff);
		_G(_unp)._strMem &= 0xffff;
		/* only if ea_addff, no reg involved */
		if (((_G(_unp)._strAdC & 0xffff0000) == EA_ADDFF) && ((_G(_unp)._strMem & 0xff) == 0)) {
			_G(_unp)._strMem += 0x100;
			_G(_unp)._strMem &= 0xffff;
		}
	}

	if (_G(_unp)._strAdC & EA_USE_A) {
		_G(_unp)._strMem += r->_a;
		_G(_unp)._strMem &= 0xffff;
		if (_G(_unp)._strAdC & EA_ADDFF) {
			if ((_G(_unp)._strMem & 0xff) == 0xff)
				_G(_unp)._strMem++;

			if (r->_a == 0) {
				_G(_unp)._strMem += 0x100;
				_G(_unp)._strMem &= 0xffff;
			}
		}
	}

	if (_G(_unp)._strAdC & EA_USE_X) {
		_G(_unp)._strMem += r->_x;
		_G(_unp)._strMem &= 0xffff;

		if (_G(_unp)._strAdC & EA_ADDFF) {
			if ((_G(_unp)._strMem & 0xff) == 0xff)
				_G(_unp)._strMem++;

			if (r->_x == 0) {
				_G(_unp)._strMem += 0x100;
				_G(_unp)._strMem &= 0xffff;
			}
		}
	}

	if (_G(_unp)._strAdC & EA_USE_Y) {
		_G(_unp)._strMem += r->_y;
		_G(_unp)._strMem &= 0xffff;

		if (_G(_unp)._strAdC & EA_ADDFF) {
			if ((_G(_unp)._strMem & 0xff) == 0xff)
				_G(_unp)._strMem++;

			if (r->_y == 0) {
				_G(_unp)._strMem += 0x100;
				_G(_unp)._strMem &= 0xffff;
			}
		}
	}
}

/* Write the standard PRG two-byte load-address header just before the
 * unpacked data, then copy [header .. end) to the caller's buffer.
 * Returns false if the result wouldn't fit. */
static bool writeUnpackedOutput(uint8_t *mem, int strMem, int endAdr,
                                uint8_t *destinationBuffer,
                                size_t destBufferCapacity,
                                size_t *finalLength) {
	size_t outSize = (size_t)(endAdr - strMem + 2);
	if (outSize > destBufferCapacity)
		return false;
	mem[strMem - 2] = strMem & 0xff;
	mem[strMem - 1] = strMem >> 8;
	memcpy(destinationBuffer, mem + (strMem - 2), outSize);
	*finalLength = outSize;
	return true;
}

/* Core unpacker entry point.
 *
 *   compressed           - the raw PRG (load address + payload) to unpack
 *   length               - size of the compressed buffer in bytes
 *   destinationBuffer    - caller-owned buffer that receives the unpacked PRG
 *   destBufferCapacity   - size in bytes of destinationBuffer; the unpacker
 *                          will fail rather than overrun if the result is
 *                          larger (max plausible output is 65538 bytes)
 *   finalLength          - out: number of bytes written to destinationBuffer
 *   switches             - optional UNP64 command-line style flags, e.g.
 *                          "-e<addr>" force entry, "-d<addr>" depack address,
 *                          "-t<addr>" end address, "-r<addr>" return address,
 *                          "-c" recurse on the unpacked result, etc.
 *
 * Returns true on success, false on a recoverable failure (unknown packer
 * or emulation aborted without producing output). The strategy is to
 * emulate the 6502 CPU running the packed program inside a 64 KiB RAM
 * image until execution reaches a known "depack done" address, then
 * copy the decompressed bytes from emulator RAM to destinationBuffer. */
bool unp64cpp(const uint8_t *compressed, size_t length, uint8_t *destinationBuffer, size_t destBufferCapacity, size_t *finalLength, const char *switches) {

	char settings[4][64];
	int numSettings = parseSwitchString(switches, settings);

	CpuCtx r[1];          /* 6502 CPU register/flag context */
	LoadInfo info[1];     /* parsed metadata about the loaded PRG */

	/* Common failure exit: invalidate the global _info pointer (which
	 * aliases the local `info` and would otherwise dangle) and report
	 * failure to the caller. */
	auto fail = [&]() {
		_G(_unp)._info = nullptr;
		return false;
	};

	/* Emulated C64 RAM and a pre-run snapshot used by -u/-l. Heap-allocated
	 * to keep the 128 KiB out of the stack frame. */
	auto memBuf    = std::unique_ptr<uint8_t[]>(new uint8_t[65536]());
	auto oldmemBuf = std::unique_ptr<uint8_t[]>(new uint8_t[65536]());
	uint8_t *mem    = memBuf.get();
	uint8_t *oldmem = oldmemBuf.get();

	/* Snapshot of the IRQ/NMI/BRK/IO vectors at $0314-$0333 as they
	 * appear on a freshly-reset C64. Some packers decrypt themselves
	 * using these bytes, so we must seed them rather than leave zeros. */
	static const uint8_t kInitialVector[0x20] = {0x31, 0xEA, 0x66, 0xFE, 0x47, 0xFE, 0x4A, 0xF3,
						 0x91, 0xF2, 0x0E, 0xF2, 0x50, 0xF2, 0x33, 0xF3,
						 0x57, 0xF1, 0xCA, 0xF1, 0xED, 0xF6, 0x3E, 0xF1,
						 0x2F, 0xF3, 0x66, 0xFE, 0xA5, 0xF4, 0xED, 0xF5};

	/* Working copy — the _debugP path mutates the vector mid-run. */
	uint8_t vector[0x20];
	memcpy(vector, kInitialVector, sizeof(vector));

	/* Snapshot of page $0100 (the 6502 stack page) immediately after a
	 * BASIC `SYS` from the READY prompt — some packers expect leftover
	 * bytes from BASIC's tokenized SYS line to still be on the stack. */
	static const uint8_t stack[0x100] = {0x33, 0x38, 0x39, 0x31, 0x31, 0x00, 0x30, 0x30, 0x30, 0x30,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
						 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7D,
						 0xEA, 0x00, 0x00, 0x82, 0x22, 0x0E, 0xBC, 0x81, 0x64, 0xB8,
						 0x0C, 0xBD, 0xBA, 0xB7, 0xBC, 0x03, 0x00, 0x46, 0xE1, 0xE9,
						 0xA7, 0xA7, 0x79, 0xA6, 0x9C, 0xE3};

	int iterMax = ITERMAX; /* hard cap on emulated instructions per phase */
	int p;

	memset(&_G(_unp), 0, sizeof(_G(_unp)));
	reinitUnp();
	_G(_unp)._fStack = 1;
	_G(_unp)._mem = mem;
	_G(_unp)._r = r;
	_G(_unp)._name = NULL;
	_G(_unp)._info = info;

	p = 0;

	/* The -f<value> switch pre-fills the upper part of RAM with a byte
	 * pattern; some packers leave the unfilled region untouched and a
	 * known pattern makes "what was modified" detection (-u) reliable. */
	if (numSettings != 0) {
		if (settings[0][0] == '-' && _G(_parsePar) && settings[0][1] == 'f') {
			strToInt(settings[p] + 2, (int *)&_G(_unp)._filler);
			if (_G(_unp)._filler) {
				memset(mem + (_G(_unp)._filler >> 16), _G(_unp)._filler & 0xff, 0x10000 - (_G(_unp)._filler >> 16));
			}
			p++;
		}
	}

	/* Outer loop handles the -c "recurse on output" case; the explicit
	 * `continue` at the bottom replaces the original goto. */
	while (true) {
	info->_basicTxtStart = 0x801;
	/* Load the PRG into the emulated address space (honours the
	 * two-byte load address header) and extract metadata. */
	loadData(compressed, length, mem, info);
		/* no start address from load */
	if (info->_run == -1) {
		/* look for sys line */
		info->_run = findSys(mem + info->_basicTxtStart, 0x9e);
	}

	/* Scan the loaded image for known packer signatures, which
	 * populates _depAdr/_endAdr/_strMem hints and may set _idFlag. */
	scanners(&_G(_unp));
	if (_G(_unp)._idFlag == 2) {
		return fail();
	}

	/* Apply user-supplied switches, but only on the first pass — when we
	 * recurse on a nested payload, the inner pass should use whatever the
	 * scanners detected, not the outer command-line overrides. */
	if ((_G(_unp)._recurs == 0) && (numSettings > 0)) {
		applySwitches(settings, numSettings, p, info, &iterMax);
	}

	/* Identify-only mode: bail out early if no depack address was found. */
	if (_G(_unp)._idOnly) {
        if (_G(_unp)._depAdr == 0) {
            return fail();
        }
	}

	/* For diff-based output trimming (-u/-l), snapshot RAM before run. */
	if (_G(_unp)._wrMemF | _G(_unp)._lfMemF) {
		memcpy(oldmem, mem, 65536);
	}

	if (_G(_unp)._forced) {
		info->_run = _G(_unp)._forced;
	}

	if (info->_run == -1) {
		return fail();
	}

	if (_G(_unp)._strMem > _G(_unp)._retAdr) {
		_G(_unp)._strMem = _G(_unp)._retAdr;
	}

	/* Seed $0000 with RTS so any stray jump to zero terminates cleanly,
	 * and $0001 (CPU port / bank-switching latch) to the default $37
	 * (BASIC + KERNAL + I/O visible) unless the forced entry is in a
	 * ROM region — then expose RAM there ($38). */
	mem[0] = 0x60;
	r->_cycles = 0;
	mem[1] = 0x37;

	if (((_G(_unp)._forced >= 0xa000) && (_G(_unp)._forced < 0xc000)) || (_G(_unp)._forced >= 0xd000))
		mem[1] = 0x38;

	/* some packers rely on basic pointers already set */
	mem[0x2b] = info->_basicTxtStart & 0xff;  /* TXTTAB - start of BASIC text */
	mem[0x2c] = info->_basicTxtStart >> 8;

	if (info->_basicVarStart == -1) {
		mem[0x2d] = info->_end & 0xff;
		mem[0x2e] = info->_end >> 8;
	} else {
		mem[0x2d] = info->_basicVarStart & 0xff;
		mem[0x2e] = info->_basicVarStart >> 8;
	}

	/* VARTAB / ARYTAB / STREND all collapsed onto end-of-BASIC. */
	mem[0x2f] = mem[0x2d];
	mem[0x30] = mem[0x2e];
	mem[0x31] = mem[0x2d];
	mem[0x32] = mem[0x2e];
	mem[0xae] = info->_end & 0xff;  /* EAL - end addr of last LOAD */
	mem[0xaf] = info->_end >> 8;

	/* CCS unpacker requires $39/$3a (current basic line number) set */
	mem[0x39] = mem[0x803];
	mem[0x3a] = mem[0x804];
	mem[0x52] = 0;
	mem[0x53] = 3;

	if (_G(_unp)._fStack) {
		memcpy(mem + 0x100, stack,
			   sizeof(stack)); /* stack as found on clean start */
		r->_sp = 0xf6;         /* sys from immediate mode leaves $f6 in stackptr */
	} else {
		r->_sp = 0xff;
	}

	if (info->_start > (long int)(0x314 + sizeof(vector))) {
		/* some packers use values in irq pointers to decrypt themselves */
		memcpy(mem + 0x314, vector, sizeof(vector));
	}

	/* $0200 is the start of BASIC's input buffer; $8A (TXA) is what
	 * the SYS evaluator leaves there. */
	mem[0x200] = 0x8a;
	r->_mem = mem;
	r->_pc = info->_run;
	r->_flags = 0x20;
	r->_a = 0;
	r->_y = 0;

	if (info->_run > 0x351) /* temporary for XIP */ {
		r->_x = 0;
	}

	/* Phase 1: emulate forward until we reach the depack-done address
	 * (or, if none is known, until pc passes the configured return
	 * address). Most of the body handles "the program just called a
	 * KERNAL/BASIC ROM routine" — we don't emulate ROM code, so we
	 * fake its observable side effects and let execution continue. */
	_G(_iter) = 0;
	while ((_G(_unp)._depAdr ? r->_pc != _G(_unp)._depAdr : r->_pc >= _G(_unp)._retAdr)) {
		/* Are we executing inside ROM (BASIC at $A000-BFFF or KERNAL at $E000+)? */
		if ((((mem[1] & 0x7) >= 6) && (r->_pc >= 0xe000)) || ((r->_pc >= 0xa000) && (r->_pc <= 0xbfff) && ((mem[1] & 0x7) > 6))) {
			/* some packer relies on regs set at return from CLRSCR */
			if ((r->_pc == 0xe536) || (r->_pc == 0xe544) || (r->_pc == 0xff5b) || ((r->_pc == 0xffd2) && (r->_a == 0x93))) {
				if (r->_pc != 0xffd2) {
					r->_x = 0x01;
					r->_y = 0x84;

					if (r->_pc == 0xff5b)
						r->_a = 0x97; /* actually depends on $d012 */
					else
						r->_a = 0xd8;

					r->_flags &= ~(128 | 2);
					r->_flags |= (r->_a == 0 ? 2 : 0) | (r->_a & 128);
				}
				memset(mem + 0x400, 0x20, 1000);
			}
			/* intros: GETIN ($FFE4) / GET-CHAR ($F13E). Cycle through a
			 * canned sequence of keypresses (space, N, Ctrl-C, _, cursor
			 * down, return, 1) so any keyboard-driven intro/menu eventually
			 * advances rather than spinning forever. */
			if ((r->_pc == 0xffe4) || (r->_pc == 0xf13e)) {
				static int flipspe4 = -1;
				static unsigned char fpressedchars[] = {0x20, 0, 0x4e, 0, 3, 0, 0x5f, 0, 0x11, 00, 0x0d, 0, 0x31, 0};
				flipspe4++;

				if (flipspe4 > ARRAYSIZE(fpressedchars))
					flipspe4 = 0;

				r->_a = fpressedchars[flipspe4];
				r->_flags &= ~(128 | 2);
				r->_flags |= (r->_a == 0 ? 2 : 0) | (r->_a & 128);
			}

			/* RESTOR ($FD15): fake registers normally set by the KERNAL
			 * vector restore so packers reading them get plausible values. */
			if (r->_pc == 0xfd15) {
				r->_a = 0x31;
				r->_x = 0x30;
				r->_y = 0xff;
			}

			/* IOINIT ($FDA3): mimic the post-init CIA/$01 state. */
			if (r->_pc == 0xfda3) {
				mem[0x01] = 0xe7;
				r->_a = 0xd7;
				r->_x = 0xff;
			}

			/* SETNAM ($FFBD): persist filename pointer/length in zeropage. */
			if (r->_pc == 0xffbd) {
				mem[0xB7] = r->_a;
				mem[0xBB] = r->_x;
				mem[0xBC] = r->_y;
			}

			/* LOAD ($FFD5) / KERNAL serial bus routine ($F4A2):
			 * we can't service real I/O, so treat hitting these as "done". */
			if ((r->_pc == 0xffd5) || (r->_pc == 0xf4a2)) {
				break;
			}

			/* If we've landed in BASIC's RUN/READY path, the unpacker has
			 * handed control back to BASIC — look for the new SYS line and
			 * jump there; otherwise fall through with an RTS. */
			if (isBasicRun1(r->_pc)) {
				info->_run = findSys(mem + info->_basicTxtStart, 0x9e);
				if (info->_run > 0) {
					r->_sp = 0xf6;
					r->_pc = info->_run;
				} else {
					mem[0] = 0x60;
					r->_pc = 0; /* force a RTS instead of executing ROM code */
				}
			} else {
				mem[0] = 0x60;
				r->_pc = 0; /* force a RTS instead of executing ROM code */
			}
		}

		/* Step the emulated CPU one instruction. Non-zero means we hit
		 * an illegal/unsupported opcode and must abort. */
        if (nextInst(r) == 1) {
            return fail();
        }

		_G(_iter)++;
		if (_G(_iter) == iterMax) {
				return false;
		}

		/* Exomizer family: while the decompressor is executing from its
		 * code-on-stack tail at $100-$200, latch the end address out of
		 * the $FE/$FF zeropage pair (Exomizer's "to" pointer). */
		if (_G(_unp)._exoFnd && (_G(_unp)._endAdr == 0x10000) && (r->_pc >= 0x100) && (r->_pc <= 0x200) && (_G(_unp)._strMem != 2)) {
			_G(_unp)._endAdr = r->_mem[0xfe] + (r->_mem[0xff] << 8);
			if ((_G(_unp)._exoFnd & 0xff) == 0x30) { /* low byte of _endAdr, it's a lda $ff00,y */
				_G(_unp)._endAdr = (_G(_unp)._exoFnd >> 8) + (r->_mem[0xff] << 8);
			} else if ((_G(_unp)._exoFnd & 0xff) == 0x32) { /* add 1 */
				_G(_unp)._endAdr = 1 + ((_G(_unp)._exoFnd >> 8) + (r->_mem[0xff] << 8));
			}

			if (_G(_unp)._endAdr == 0)
				_G(_unp)._endAdr = 0x10001;
		}

		/* "Read end-address before-depack-done from this RAM location."
		 * Scanners set _fEndBf when the packer keeps its end pointer in
		 * a known temp word that's only valid right at the depack address. */
		if (_G(_unp)._fEndBf && (_G(_unp)._endAdr == 0x10000) && (r->_pc == _G(_unp)._depAdr)) {
			_G(_unp)._endAdr = r->_mem[_G(_unp)._fEndBf] | r->_mem[_G(_unp)._fEndBf + 1] << 8;
			_G(_unp)._endAdr++;

			if (_G(_unp)._endAdr == 0)
					_G(_unp)._endAdr = 0x10001;

			_G(_unp)._fEndBf = 0;
		}

		/* Same idea, but for the start address. */
		if (_G(_unp)._fStrBf && (_G(_unp)._strMem != 0x2) && (r->_pc == _G(_unp)._depAdr)) {
			_G(_unp)._strMem = r->_mem[_G(_unp)._fStrBf] | r->_mem[_G(_unp)._fStrBf + 1] << 8;
			_G(_unp)._fStrBf = 0;
		}

		/* Debug: track which IRQ/NMI/BRK vector bytes the unpacker touches. */
		if (_G(_unp)._debugP) {
			for (p = 0; p < 0x20; p += 2) {
				if (READ_LE_UINT16(mem + 0x314 + p) != READ_LE_UINT16(vector + p)) {
                    vector[p] = mem[0x314 + p];
                    vector[p + 1] = mem[0x314 + p + 1];
				}
			}
		}
	}

	/* Phase 2: depacker has finished writing the payload to RAM and is
	 * now in its "transition to the depacked program" tail. Keep stepping
	 * until pc reaches (or passes) the configured return address, so any
	 * final fix-ups the packer performs land in RAM before we dump it. */
	_G(_iter) = 0;
	while (_G(_unp)._rtAFrc ? r->_pc != _G(_unp)._retAdr : r->_pc < _G(_unp)._retAdr) {
		/* Each time we re-pass the depack address, sample the watched
		 * "current write pointer" location; track the maximum end and
		 * minimum start the packer ever wrote to. */
		if (_G(_unp)._monEnd && r->_pc == _G(_unp)._depAdr) {
			p = r->_mem[_G(_unp)._monEnd >> 16] | r->_mem[_G(_unp)._monEnd & 0xffff] << 8;
			if (p > (_G(_unp)._endAdr & 0xffff)) {
				_G(_unp)._endAdr = p;
			}
		}
		if (_G(_unp)._monStr && r->_pc == _G(_unp)._depAdr) {
			p = r->_mem[_G(_unp)._monStr >> 16] | r->_mem[_G(_unp)._monStr & 0xffff] << 8;
			if (p > 0) {
				if (_G(_unp)._mon1st == 0) {
					_G(_unp)._strMem = p;
				}
				_G(_unp)._mon1st = (unsigned int)_G(_unp)._strMem;
				_G(_unp)._strMem = (p < _G(_unp)._strMem ? p : _G(_unp)._strMem);
			}
		}

		/* Don't actually run KERNAL code — bounce out to a synthesized RTS. */
		if (r->_pc >= 0xe000) {
			if (((mem[1] & 0x7) >= 6) && ((mem[1] & 0x7) <= 7)) {
				mem[0] = 0x60;
				r->_pc = 0;
			}
		}
        if (nextInst(r) == 1) {
            return fail();
        }

		/* If the scanner flagged that the depacker exits via RTI ($40)
		 * (a common trick to set processor flags and jump in one step),
		 * step the RTI and treat its destination as the return address. */
        if ((mem[r->_pc] == 0x40) && (_G(_unp)._rtiFrc == 1)) {
            if (nextInst(r) == 1) {
                return fail();
            }
			_G(_unp)._retAdr = r->_pc;
			_G(_unp)._rtAFrc = 1;
			if (_G(_unp)._retAdr < _G(_unp)._strMem)
				_G(_unp)._strMem = 2;
			break;
		}

		_G(_iter)++;
		if (_G(_iter) == iterMax) {
			return false;
		}

		/* Reached BASIC ROM with BASIC banked in — likely returning to
		 * BASIC's RUN dispatcher; force re-entry at $A7AE and stop. */
		if ((r->_pc >= 0xa000) && (r->_pc <= 0xbfff) && ((mem[1] & 0x7) == 7)) {
			if (isBasicRun2(r->_pc)) {
				r->_pc = 0xa7ae;
				break;
			} else {
				mem[0] = 0x60;
				r->_pc = 0;
			}
		}

		if (r->_pc >= 0xe000) {
			if (((mem[1] & 0x7) >= 6) && ((mem[1] & 0x7) <= 7)) {
				if (r->_pc == 0xffbd) {
					mem[0xB7] = r->_a;
					mem[0xBB] = r->_x;
					mem[0xBC] = r->_y;
				}
				/* return into IRQ handler, better stop here */
				if (((r->_pc >= 0xea31) && (r->_pc <= 0xeb76)) || (r->_pc == 0xffd5) || (r->_pc == 0xfce2)) {
					break;
				}

				if (r->_pc == 0xfda3) {
					mem[0x01] = 0xe7;
					r->_a = 0xd7;
					r->_x = 0xff;
				}
				mem[0] = 0x60;
				r->_pc = 0;
			}
		}
	}

	/* "Read end-address after depack-done from this location." Resolved
	 * now because the value at _fEndAf is only correct once execution
	 * has reached the post-depack return path. */
	if (_G(_unp)._fEndAf && _G(_unp)._monEnd) {
		_G(_unp)._endAdC = (unsigned int)(mem[_G(_unp)._fEndAf] | mem[_G(_unp)._fEndAf + 1] << 8);
		if ((int)_G(_unp)._endAdC > _G(_unp)._endAdr)
			_G(_unp)._endAdr = (int)_G(_unp)._endAdC;

		_G(_unp)._endAdC = 0;
		_G(_unp)._fEndAf = 0;
	}

	if (_G(_unp)._fEndAf && (_G(_unp)._endAdr == 0x10000)) {
		_G(_unp)._endAdr = r->_mem[_G(_unp)._fEndAf] | r->_mem[_G(_unp)._fEndAf + 1] << 8;
		if (_G(_unp)._endAdr == 0)
			_G(_unp)._endAdr = 0x10000;
		else
			_G(_unp)._endAdr++;
		_G(_unp)._fEndAf = 0;
	}

	if (_G(_unp)._fStrAf /*&&(_G(_unp)._strMem==0x800)*/) {
		_G(_unp)._strMem = r->_mem[_G(_unp)._fStrAf] | r->_mem[_G(_unp)._fStrAf + 1] << 8;
		_G(_unp)._strMem++;
		_G(_unp)._fStrAf = 0;
	}

	/* Exomizer leaves the final start address in $FE/$FF, optionally
	 * adjusted by Y depending on the addressing-mode variant detected. */
	if (_G(_unp)._exoFnd && (_G(_unp)._strMem != 2)) {
		_G(_unp)._strMem = r->_mem[0xfe] + (r->_mem[0xff] << 8);

		if ((_G(_unp)._exoFnd & 0xff) == 0x30) {
			_G(_unp)._strMem += r->_y;
		} else if ((_G(_unp)._exoFnd & 0xff) == 0x32) {
			_G(_unp)._strMem += r->_y + 1;
		}
	}

	/* Stopped at the KERNAL reset vector ($FCE2) with the C64 cartridge
	 * autostart signature ("CBM80") at $8000-$8008? Then follow the
	 * cartridge's cold-start pointer. Otherwise, if we wound up at the
	 * BASIC RUN entry, look up the freshly-installed SYS line. */
	if (r->_pc == 0xfce2) {
		if (u32eq(mem + 0x8004, 0x38cdc2c3) && mem[0x8008] == 0x30) {
			r->_pc = READ_LE_UINT16(&r->_mem[0x8000]);
		}
	} else if (r->_pc == 0xa7ae) {
		info->_basicTxtStart = mem[0x2b] | mem[0x2c] << 8;
		if (info->_basicTxtStart == 0x801) {
			info->_run = findSys(mem + info->_basicTxtStart, 0x9e);
			if (info->_run > 0)
				r->_pc = info->_run;
		}
	}

	/* -u: zero any 4-byte aligned region that the unpacker didn't change
	 * compared to the pre-run snapshot — strips out leftover filler from
	 * the compressed image so only the truly-written bytes remain. */
	if (_G(_unp)._wrMemF) {
		_G(_unp)._wrMemF = 0;
		for (p = 0x800; p < 0x10000; p += 4) {
			if (u32eq(oldmem + p, READ_LE_UINT32(mem + p))) {
				mem[p + 0] = 0;
                mem[p + 1] = 0;
                mem[p + 2] = 0;
                mem[p + 3] = 0;
				_G(_unp)._wrMemF = 1;
			}
		}
		/* clean also the $fd30 table copy in RAM */
		if (memcmp(mem + 0xfd30, vector, sizeof(vector)) == 0) {
			memset(mem + 0xfd30, 0, sizeof(vector));
		}
	}

	/* -l: scan back from $FFFF zeroing bytes that match the original
	 * compressed image, stopping at the first divergence — trims any
	 * tail of unchanged bytes that the depacker happened to leave alone. */
	if (_G(_unp)._lfMemF) {
		for (p = 0xffff; p > 0x0800; p--) {
			if (oldmem[--_G(_unp)._lfMemF] == mem[p])
				mem[p] = 0x0;
			else {
				if (p >= 0xffff)
					_G(_unp)._lfMemF = 0 | _G(_unp)._ecaFlg;
				break;
			}
		}
	}

	/*  endadr is set to a ZP location? then use it as a pointer
	  todo: use __fEndAf instead, it can be used for any location, not only ZP. */
	if (_G(_unp)._endAdr && (_G(_unp)._endAdr < 0x100)) {
		p = (mem[_G(_unp)._endAdr] | mem[_G(_unp)._endAdr + 1] << 8) & 0xffff;
		_G(_unp)._endAdr = p;
	}

	/* "Extra copy after" flag: some packers move a $1000-byte block out
	 * of the I/O space ($D000-DFFF) into RAM at the end — account for
	 * that extra range so the dump covers the relocated copy too. */
	if (_G(_unp)._ecaFlg && (_G(_unp)._strMem != 2)) /* checkme */ {
		if (_G(_unp)._endAdr >= ((_G(_unp)._ecaFlg >> 16) & 0xffff)) {
			/* most of the times transfers $2000 byte from $d000-efff to $e000-ffff but there are exceptions */
			if (_G(_unp)._lfMemF)
				memset(mem + ((_G(_unp)._ecaFlg >> 16) & 0xffff), 0, 0x1000);
			_G(_unp)._endAdr += 0x1000;
		}
	}

	/* Clamp the end address to a sane range before applying offsets. */
	if (_G(_unp)._endAdr <= 0)
		_G(_unp)._endAdr = 0x10000;

	if (_G(_unp)._endAdr > 0x10000)
		_G(_unp)._endAdr = 0x10000;

	if (_G(_unp)._endAdr < _G(_unp)._strMem)
		_G(_unp)._endAdr = 0x10000;

	/* _endAdC / _strAdC encode "after the run, add this constant and/or
	 * the value of this register to the end/start address" — used when
	 * the depacker computes its final pointer in registers rather than
	 * leaving it in a known memory cell. */
	applyEndStartOffsets(r);

	if (_G(_unp)._endAdr <= 0)
		_G(_unp)._endAdr = 0x10000;

	if (_G(_unp)._endAdr > 0x10000)
		_G(_unp)._endAdr = 0x10000;

	if (_G(_unp)._endAdr < _G(_unp)._strMem)
		_G(_unp)._endAdr = 0x10000;

	if (!writeUnpackedOutput(mem, _G(_unp)._strMem, _G(_unp)._endAdr,
	                         destinationBuffer, destBufferCapacity, finalLength)) {
		return fail();
	}

	/* -c: the output we just produced may itself be packed (a wrapped
	 * payload). Reset state and run the whole pipeline again on it,
	 * capped at RECUMAX iterations to avoid infinite recursion. */
	if (_G(_unp)._recurs) {
		if (++_G(_unp)._recurs > RECUMAX)
			return true;
		reinitUnp();
		continue;
	}
	return true;
	} /* end while(true) */
}

} // End of namespace Unp64


/* C-callable wrapper around Unp64::unp64cpp. Manages the per-call
 * Globals instance so callers from C code (ScottFree, TaylorMade interpreters)
 * don't need to know about the namespace or the global state.
 *
 * The C ABI doesn't carry a destination-buffer-size parameter, so this
 * wrapper assumes the caller has allocated a full 64 KiB output buffer —
 * which is the contract every current caller already follows. */
int unp64(uint8_t *compressed, size_t length, uint8_t *destinationBuffer, size_t *finalLength, const char *settings) {

	using namespace Unp64;

	/* Stack-allocated; the constructor wires g_globals to this object
	 * for the duration of the call, and the destructor clears it. */
	Globals globals;

	return unp64cpp(compressed, length, destinationBuffer, 0x10000, finalLength, settings);
}
