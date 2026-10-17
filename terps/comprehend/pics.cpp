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

#include "pics.h"
#include "graphics_magician.h"
#include "graphics_magician_cga.h"
#include "graphics_magician_dhgr.h"
#include "graphics_magician_pcjr.h"
#include "comprehend_compat.h"
#include <vector>
#include "comprehend.h"
#include "draw_surface.h"
#include "file_buf.h"
#include "game.h"
#include "game_cm.h"   // CM_PIECE_PICS: mirror-piece item pictures folded into the panel
#include "game_data.h"

namespace Glk {
namespace Comprehend {

#define IMAGES_PER_FILE 16

Pics::ImageFile::ImageFile(const Common::String &filename, bool isSingleImage) : _filename(filename) {
	Common::File f;
	uint16 version;
	int i;

	if (!f.open(_filename))
		error("Could not open file - %s", filename.c_str());

	if (isSingleImage) {
		// It's a title image file, which has only a single image with no
		// table of image offsets.
		_imageOffsets.resize(1);
		if (Common::DiskImageFS::active()) {
			// Apple II title (T0). Talisman's T0 is a pure Graphics Magician
			// vector image (offset 0). For the other three the T0 file opens
			// with a disk copy-protection + loader stub, and the title vector
			// image starts just past it, at a fixed (game-specific) offset.
			_imageOffsets[0] = g_comprehend->traits().appleTitleOffset;
		} else {
			// DOS title files. The v1 releases (TRTITLE.MS1 / CCTITLE.MS1)
			// carry a 4-byte header (00 63 xx xx) before the vector stream.
			// The v2 T0 files are a raw stream from byte 0: they open with
			// op15/3 (fill the whole picture with the startup pattern, i.e.
			// the white background) followed by a MOVE_TO -- skipping those
			// 4 bytes loses the background fill and the initial pen position
			// (it broke the OO-Topos and Transylvania titles; Talisman only
			// survived because its first 4 bytes happen to be op15/3 plus a
			// no-op op7). Sniff the first byte: a leading 0x00 would be op0
			// (END), which no real stream starts with, so it must be a header.
			_imageOffsets[0] = (f.readByte() == 0) ? 4 : 0;
		}
		return;
	}

	version = f.readUint16LE();
	if (version == 0x1000)
		f.seek(4);
	else
		f.seek(0);

	// Get the image offsets in the file
	_imageOffsets.resize(IMAGES_PER_FILE);
	for (i = 0; i < IMAGES_PER_FILE; i++) {
		_imageOffsets[i] = f.readUint16LE();
		if (version == 0x1000)
			_imageOffsets[i] += 4;
	}
}

void Pics::ImageFile::renderGm(uint index, const GmBackend &gm) const {
	Common::File f;
	if (!f.open(_filename))
		error("Opening image file");

	int64 start = _imageOffsets[index];
	int64 fsize = f.size();
	if (start < 0 || start >= fsize)
		return;

	// The renderer stops at op0 (END); we hand it everything from the image's
	// offset to end-of-file and let it terminate itself.
	size_t len = (size_t)(fsize - start);
	std::vector<byte> buf(len);
	f.seek(start);
	f.read(buf.data(), (uint32)len);

	gm.drawImage(buf.data(), len);
}

/*-------------------------------------------------------*/

// Read a whole file, trying each of the given names; empty if none opens.
static std::vector<byte> readWholeFile(std::initializer_list<const char *> names) {
	std::vector<byte> buf;
	Common::File f;
	for (const char *name : names) {
		if (!f.open(name))
			continue;
		int64 sz = f.size();
		if (sz > 0) {
			buf.resize((size_t)sz);
			if (f.read(buf.data(), (uint32)sz) != (uint32)sz)
				buf.clear();
		}
		break;
	}
	return buf;
}

Pics::Pics() {
	// DOS releases: load the CGA drawing tables (fill patterns, brushes, font)
	// out of the Penguin Graphics Magician interpreter image (NOVEL.EXE). The
	// loader locates the tables by signature, so it succeeds for every Comprehend
	// v2 game that shares the CGA interpreter (Talisman, OO-Topos, Transylvania
	// v2, The Coveted Mirror).
	// The older v1 releases (Crimson Crown, Transylvania v1) split the Graphics
	// Magician across NOVEL.EXE and PC_GRAPH.OVR: their fill pattern + subindex
	// tables live in the overlay (not in NOVEL.EXE, where they are BSS built at
	// overlay load), so the v2 signature scan misses them -- fall back to the v1
	// loader, which reads the fill tables from PC_GRAPH.OVR and the brushes from
	// NOVEL.EXE.
	// A folder that has lost those files (the pictures and game data are all
	// this interpreter otherwise needs) gets the built-in copy of the tables,
	// which every release seen so far shares; CHARSET.GDA, which only the v1
	// games ship, tells which of the two sets it wants. Either way every DOS
	// picture is drawn by graphics_magician_cga.
	if (!Common::DiskImageFS::active()) {
		const std::vector<byte> exeBuf = readWholeFile({ "novel.exe", "NOVEL.EXE", "novel1.exe", "NOVEL1.EXE" });
		// v1 NOVEL.EXE has no embedded picture font; the op3/op5 glyphs (map
		// labels) of those games come from CHARSET.GDA.
		const std::vector<byte> charset = readWholeFile({ "charset.gda", "CHARSET.GDA" });

		if (exeBuf.empty() || !gmcgaInstallDrawingTables(exeBuf.data(), exeBuf.size())) {
			// v2 tables absent -> try the v1 split layout (needs PC_GRAPH.OVR).
			const std::vector<byte> ovrBuf = readWholeFile({ "pc_graph.ovr", "PC_GRAPH.OVR" });
			if (exeBuf.empty() || ovrBuf.empty() ||
				!gmcgaInstallV1DrawingTables(ovrBuf.data(), ovrBuf.size(),
											 exeBuf.data(), exeBuf.size()))
				gmcgaInstallBuiltinDrawingTables(!charset.empty());
			if (!charset.empty())
				gmcgaSetV1Font(charset.data(), charset.size());
		}

		// IBM PCjr / Tandy 16-colour mode (DOS v1 games, opt-in via #pcjr).
		// The PCjr release ships JR_GRAPH.OVR alongside PC_GRAPH.OVR; its 4bpp
		// drawing tables (pen colours, fill dithers, subindex) are installed
		// here from the overlay + NOVEL.EXE brushes, in parallel with the CGA
		// tables above. The renderer is selected per-picture by Comprehend::gfxBackend().
		const std::vector<byte> jrBuf = readWholeFile({ "jr_graph.ovr", "JR_GRAPH.OVR" });
		if (!exeBuf.empty() && !jrBuf.empty() &&
			gmpcjrInstallDrawingTables(jrBuf.data(), jrBuf.size(), exeBuf.data(), exeBuf.size()) &&
			!charset.empty())
			gmpcjrSetFont(charset.data(), charset.size());
	}
}

void Pics::clear() {
	_rooms.clear();
	_items.clear();
}

void Pics::load(const Common::StringArray &roomFiles,
				const Common::StringArray &itemFiles,
				const Common::String &titleFile) {
	clear();

	// Picture numbers select a file by its position in the list ((graphic-1) /
	// IMAGES_PER_FILE), so a file that doesn't exist on the current disk must
	// still occupy its slot or every later file shifts down and the index runs
	// off the end of the array. Talisman is the case in point: its item art is
	// numbered against the full OA..OF sequence, but only OA, OB, OE and OF ship
	// (OC/OD are absent), so OE/OF must stay at slots 4/5. Push an empty
	// placeholder ImageFile for any missing entry instead of erroring out.
	for (uint idx = 0; idx < roomFiles.size(); ++idx)
		_rooms.push_back(Common::File::exists(roomFiles[idx])
						 ? ImageFile(roomFiles[idx]) : ImageFile());
	for (uint idx = 0; idx < itemFiles.size(); ++idx)
		_items.push_back(Common::File::exists(itemFiles[idx])
						 ? ImageFile(itemFiles[idx]) : ImageFile());

	// Guard the title exactly like the room/item files above: a game may ship
	// its room art but no separate title image (or no graphics at all), and the
	// ImageFile ctor error()s on a failed open. Leave _title empty if absent.
	if (!titleFile.empty() && Common::File::exists(titleFile))
		_title = ImageFile(titleFile, true);
}

int Pics::getPictureNumber(const Common::String &filename) const {
	// Ensure prefix and suffix
	if (!filename.hasPrefixIgnoreCase("pic") ||
		!filename.hasSuffixIgnoreCase(".raw"))
		return -1;

	// Get the number part
	Common::String num(filename.c_str() + 3, filename.size() - 7);
	if (num.empty() || !Common::isDigit(num[0]))
		return -1;

	return atoi(num.c_str());
}

bool Pics::hasFile(const Common::Path &path) const {
	Common::String name = path.baseName();
	int num = getPictureNumber(name);
	if (num == -1)
		return false;

	if (num == DARK_ROOM || num == BRIGHT_ROOM || num == TITLE_IMAGE)
		return true;
	if (num >= ITEMS_OFFSET && num < (int)(ITEMS_OFFSET + _items.size() * IMAGES_PER_FILE))
		return true;
	if (num < ITEMS_OFFSET) {
		int loc = (num >= LOCATIONS_NO_BG_OFFSET) ? num - LOCATIONS_NO_BG_OFFSET : num;
		if (loc >= 0 && loc < (int)(_rooms.size() * IMAGES_PER_FILE))
			return true;
	}

	return false;
}

int Pics::listMembers(Common::ArchiveMemberList &list) const {
	return list.size();
}

const Common::ArchiveMemberPtr Pics::getMember(const Common::Path &path) const {
	if (!hasFile(path))
		return Common::ArchiveMemberPtr();

	return Common::ArchiveMemberPtr(new Common::GenericArchiveMember(path, *this));
}

Common::SeekableReadStream *Pics::createReadStreamForMember(const Common::Path &path) const {
	Common::String name = path.baseName();
	// Get the picture number
	int num = getPictureNumber(name);
	if (num == -1 || !hasFile(path))
		return nullptr;

	// Draw the image
	drawPicture(num);

	// Create a stream with the data for the surface
	Common::MemoryReadWriteStream *stream =
		new Common::MemoryReadWriteStream(Common::YES);
	const DrawSurface &ds = *g_comprehend->_drawSurface;
	stream->writeUint16LE(ds.w);
	stream->writeUint16LE(ds.h);
	stream->writeUint16LE(0); // Palette size
	stream->write(ds.getPixels(), ds.w * ds.h * 4);

	return stream;
}

// Draw one picture through a Graphics Magician renderer. Pictures are decoded
// onto the renderer's persistent page (so item overlays compose onto the room
// already drawn there, exactly as the real interpreter does), then the whole
// page is converted to RGBA and blitted to the draw surface.
//
// The four renderers take the same vector streams and image offsets; only the
// rasteriser differs:
//  - Apple II standard hi-res: all four disk titles (Talisman, Transylvania,
//    OO-Topos, Crimson Crown) are validated pixel-exact vs MAME; the dialect
//    differences are handled by gmSetLegacyFormat().
//  - Apple II double hi-res ("<D>" mode): the 560-wide aux/main pages,
//    NTSC-converted onto the same 560-wide surface as standard hi-res.
//  - DOS CGA: the v2 releases, tables from NOVEL.EXE; and the v1 releases,
//    tables from the graphics overlay.
//  - DOS PCjr: the v1 releases (Transylvania v1 / Crimson Crown) in 16 colours,
//    tables from JR_GRAPH.OVR.
void Pics::drawPictureGm(int pictureNum, const GmBackend &gm, uint drawFlags,
						 DrawSurface *ds) const {
	const bool apple = gm.cm != nullptr;
	const bool dhgr = gm.id == GFX_APPLE_DHGR;

	// The Coveted Mirror keeps a persistent right-hand panel (the game's
	// logo above an hourglass) in screen columns 24..39, drawn once at boot
	// while room pictures fill only the left columns. Our room images span
	// the full width, so we save the panel columns the first time we need
	// them and re-composite them on top of every in-game picture below.
	// Apple II only: the DOS release draws no panel.
	//
	// The original boot draws the panel as two Graphics Magician images, in
	// order (confirmed by tracing the real Apple II in MAME): RG image 0 is
	// the "The Coveted Mirror" logo, then OG image 0 is the hourglass frame
	// (posts, bars, bowtie glass + the static bottom-bulb mound). The
	// draining top-bulb sand is NOT part of OG0 -- it is grain-drawn each turn
	// by drawHourglass() on top. Compositing RG0 + OG0 here reproduces the
	// static panel pixel-exactly (verified byte-for-byte vs the MAME capture
	// in test/cm/fixtures/throne_sand60.page).
	// The panel lives on whichever pages are active, so it is built once per
	// hi-res mode (the standard single page and the double-hi-res aux+main
	// pages are independent buffers).
	// The snapshot also folds in the mirror pieces the player has collected
	// (see CovetedMirrorGame): each is a fixed item picture drawn once into the
	// black band left of the hourglass, which the original never repaints. We
	// rebuild the snapshot whenever a piece has just been found (the mask
	// changed) so overlayPanel() keeps painting the assembled pieces on
	// every picture; the hourglass grains are still stamped fresh on top.
	static bool s_cmPanelBuilt[2] = { false, false };   // [standard, double hi-res]
	static uint8 s_cmPanelMask[2] = { 0, 0 };
	const bool isCM = apple && g_comprehend->traits().hasHourglassPanel;
	uint8 pieceMask = 0;
	if (isCM)
		if (ComprehendGame *game = g_comprehend->getGame())
			pieceMask = game->cmMirrorPieceMask();
	if (isCM && pictureNum != TITLE_IMAGE && !_rooms.empty() && !_items.empty() &&
		(!s_cmPanelBuilt[dhgr] || s_cmPanelMask[dhgr] != pieceMask)) {
		gm.cm->beginPanelRebuild();
		_rooms[_rooms.size() - 1].renderGm(0, gm); // RG = last location file: logo
		_items[_items.size() - 1].renderGm(0, gm); // OG = last item file: hourglass frame
		for (int i = 0; i < 4; i++) {
			if (!(pieceMask & (1 << i)))
				continue;
			int p = CM_PIECE_PICS[i];
			if (p / IMAGES_PER_FILE < (int)_items.size())
				_items[p / IMAGES_PER_FILE].renderGm(p % IMAGES_PER_FILE, gm);
		}
		gm.cm->endPanelRebuild();
		s_cmPanelBuilt[dhgr] = true;
		s_cmPanelMask[dhgr] = pieceMask;
	}

	if (pictureNum == DARK_ROOM) {
		gm.resetScreen(false);
	} else if (pictureNum == BRIGHT_ROOM) {
		gm.resetScreen(true);
	} else if (pictureNum == TITLE_IMAGE) {
		// The title is a Graphics Magician vector image drawn on a white
		// background like a bright room. The exception is the Apple II OO-Topos
		// title, which starts from black (it fills its own background with op15,
		// and its lettering blends with the page, so a white start corrupts it).
		// The DOS v2 title streams open with op15/3, which paints the whole
		// picture with the startup fill (white), so there the reset value is
		// cosmetic; white matches the native interpreter's text page.
		gm.resetScreen(!(apple && g_comprehend->traits().appleTitleOnBlack));
		if (_title.isLoaded())
			_title.renderGm(0, gm);
	} else if (pictureNum >= ITEMS_OFFSET) {
		// Item overlay: draw on top of the existing room page.
		int n = pictureNum - ITEMS_OFFSET;
		_items[n / IMAGES_PER_FILE].renderGm(n % IMAGES_PER_FILE, gm);
	} else {
		// Room picture. Background variants start from a fresh page; the
		// no-background variant composes onto whatever is already shown.
		if (pictureNum < LOCATIONS_NO_BG_OFFSET)
			gm.resetScreen(!(drawFlags & IMAGEF_REVERSE));
		int n = pictureNum < LOCATIONS_NO_BG_OFFSET
					? pictureNum : pictureNum - LOCATIONS_NO_BG_OFFSET;
		_rooms[n / IMAGES_PER_FILE].renderGm(n % IMAGES_PER_FILE, gm);
	}

	// Re-apply The Coveted Mirror's persistent panel on top of the room/item
	// just drawn (the title screen has no panel), then stamp the hourglass
	// sand pile for the current sand level (VM variable 0x11): the engine
	// shows that many grains and drains one per turn.
	if (isCM && pictureNum != TITLE_IMAGE) {
		int sand = 0;
		if (ComprehendGame *game = g_comprehend->getGame())
			sand = game->_variables[0x11];
		gm.cm->overlayPanel();
		gm.cm->drawHourglass(sand);
	}

	// With slow-draw active the page is revealed a chunk at a time by the host
	// (Comprehend::tickSlowDraw), so blit only what is on the visible page so
	// far. Otherwise blit the finished page.
	gm.blitVisible((uint32 *)ds->getPixels(), ds->w, ds->h);
}

void Pics::drawPicture(int pictureNum) const {
	// Every picture goes through a Graphics Magician renderer; the ScummVM
	// vector path that used to stand in for missing DOS drawing tables went
	// when the tables got a built-in copy (see Pics::Pics).
	drawPictureGm(pictureNum, *gmBackend(g_comprehend->gfxBackend()),
				  g_comprehend->_drawFlags, g_comprehend->_drawSurface);
}

} // namespace Comprehend
} // namespace Glk
