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

#include "game_tr1.h"
#include "game_tr_common.h"

namespace Glk {
namespace Comprehend {

TransylvaniaGame1::TransylvaniaGame1() : TransylvaniaCommon<ComprehendGameV1>(false) {
	if (Common::DiskImageFS::active()) {
		// Apple II release: data file g0, undecorated file names, and the
		// strings split into separate files. The title is a raw hi-res
		// screen rather than a Comprehend vector image.
		_gameDataFile = "g0";

		addLetteredFiles(_stringFiles, "M", 'A', 'E');
		addLetteredFiles(_locationGraphicFiles, "R", 'A', 'C');
		addLetteredFiles(_itemGraphicFiles, "O", 'A', 'C');

		// The Apple II title (gothic "Transylvania" + castle) is a Graphics
		// Magician vector image living inside T0, after the disk-protection /
		// loader stub. Pics::ImageFile starts it at the right offset (0xFE).
		_titleGraphicFile = "t0";
	} else {
		_gameDataFile = "tr.gda";

		addLetteredFiles(_stringFiles, "M", 'A', 'E', ".MS1");
		addLetteredFiles(_locationGraphicFiles, "R", 'A', 'C', ".MS1");
		addLetteredFiles(_itemGraphicFiles, "O", 'A', 'C', ".MS1");

		_titleGraphicFile = "trtitle.ms1";
	}
}

} // namespace Comprehend
} // namespace Glk
