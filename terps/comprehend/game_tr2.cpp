/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "game_tr2.h"
#include "game_tr_common.h"

namespace Glk {
namespace Comprehend {

TransylvaniaGame2::TransylvaniaGame2() : TransylvaniaCommon<ComprehendGameV2>(true) {
	_gameDataFile = "g0";

	addLetteredFiles(_locationGraphicFiles, "R", 'A', 'C');
	addLetteredFiles(_itemGraphicFiles, "O", 'A', 'C');

	_titleGraphicFile = "t0";

	// Like Oo-Topos, the DOS Transylvania v2 stores its extra (_strings2) string
	// bank inside novel.exe rather than in separate files. It is five standard
	// structured segments (4-byte header + 64-entry index + data), giving the
	// 5 x 64 = 320 strings the bytecode addresses through table 0x82 (e.g. the
	// WAIT response 0x825d, OPEN COFFIN 0x824c and the restart prompt 0x828a).
	// Without this the engine printed BAD_STRING(82xx) for all of them.
	if (!Common::DiskImageFS::active()) {
		Common::File f;
		if (f.open("novel.exe")) {
			Common::String md5 = Common::computeStreamMD5AsString(f, 1024);
			f.close();

			if (md5 == "e708b5f744e1f9b59354722ab55added") {
				static const uint32 segHeaders[5] = {
					0x1636c, 0x1726c, 0x17c6c, 0x1846c, 0x18d6c
				};
				for (int i = 0; i < 5; i++)
					_stringFiles.push_back(
						StringFile::structuredSegment("novel.exe", segHeaders[i]));
			} else {
				warning("Unrecognised Transylvania novel.exe (md5 head %s); "
						"extra strings unavailable", md5.c_str());
			}
		}
	}
}

} // namespace Comprehend
} // namespace Glk
