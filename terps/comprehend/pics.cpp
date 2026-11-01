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
#include "game_cm.h"   // CM_PIECE_PICS
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
			// vector image (offset 0). For the others the T0 file opens with a
			// disk copy-protection and loader stub, and the title vector image
			// starts just past it, at a fixed game-specific offset.
			_imageOffsets[0] = g_comprehend->traits().appleTitleOffset;
		} else {
			// DOS title files. The v1 releases (TRTITLE.MS1 / CCTITLE.MS1) have
			// a 4-byte header (00 63 xx xx) before the vector stream. The v2 T0
			// files are a raw stream from byte 0, opening with op15/3 (fill
			// with the startup pattern, white) and a MOVE_TO that must not be
			// skipped. A leading 0x00 would be op0 (END), which no real stream
			// starts with, so it means a header.
			_imageOffsets[0] = (f.readByte() == 0) ? 4 : 0;
		}
		return;
	}

	version = f.readUint16LE();
	if (version == 0x1000)
		f.seek(4);
	else
		f.seek(0);

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

	// The renderer stops at op0 (END), so it gets everything from the image's
	// offset to end of file.
	size_t len = (size_t)(fsize - start);
	std::vector<byte> buf(len);
	f.seek(start);
	f.read(buf.data(), (uint32)len);

	gm.drawImage(buf.data(), len);
}

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
	// from NOVEL.EXE. The loader finds them by signature, which works for every
	// v2 game. The v1 games (Crimson Crown, Transylvania v1) keep the fill and
	// subindex tables in PC_GRAPH.OVR and the brushes in NOVEL.EXE, so they
	// need the v1 loader. A folder lacking those files gets the built-in
	// tables; CHARSET.GDA, which only the v1 games ship, selects which set.
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
		// JR_GRAPH.OVR ships alongside PC_GRAPH.OVR; its 4bpp tables are
		// installed in parallel with the CGA tables. Comprehend::gfxBackend()
		// selects the renderer per picture.
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

	// Picture numbers select a file by list position ((graphic-1) /
	// IMAGES_PER_FILE), so a missing file must keep its slot or later files
	// shift down. Talisman numbers its item art against OA..OF but ships no
	// OC/OD. Missing entries get an empty placeholder.
	for (uint idx = 0; idx < roomFiles.size(); ++idx)
		_rooms.push_back(Common::File::exists(roomFiles[idx])
						 ? ImageFile(roomFiles[idx]) : ImageFile());
	for (uint idx = 0; idx < itemFiles.size(); ++idx)
		_items.push_back(Common::File::exists(itemFiles[idx])
						 ? ImageFile(itemFiles[idx]) : ImageFile());

	// A game may ship no title image, and the ImageFile constructor error()s on
	// a failed open. Leave _title empty then.
	if (!titleFile.empty() && Common::File::exists(titleFile))
		_title = ImageFile(titleFile, true);
}

// Draw one picture through a Graphics Magician renderer. Pictures are decoded
// onto the renderer's persistent page, so item overlays compose onto the room
// already drawn there, then the whole page is converted to RGBA and blitted to
// the draw surface. The renderers (Apple II hi-res and double hi-res, DOS CGA
// and PCjr) take the same vector streams and image offsets; only the rasteriser
// differs.
void Pics::drawPictureGm(int pictureNum, const GmBackend &gm, uint drawFlags,
						 DrawSurface *ds) const {
	const bool apple = gm.cm != nullptr;
	const bool dhgr = gm.id == GFX_APPLE_DHGR;

	// The Coveted Mirror (Apple II only) has a persistent right-hand panel in
	// screen columns 24..39, drawn once at boot. Room images here span the full
	// width, so the panel is snapshotted and composited over every in-game
	// picture. The snapshot is RG image 0 (logo), OG image 0 (hourglass frame
	// and static bottom mound) and the mirror pieces found so far, in the black
	// band left of the hourglass. It is built once per hi-res mode, since the
	// pages are independent, and rebuilt when the piece mask changes.
	// drawHourglass() stamps the draining sand on top.
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
		// The title is a vector image on a white background, like a bright
		// room. The Apple II OO-Topos title starts from black instead: its
		// lettering blends with the page, so a white start corrupts it. The DOS
		// v2 titles fill the whole picture themselves with op15/3.
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
	// Every picture goes through a Graphics Magician renderer; see Pics::Pics
	// for the DOS tables.
	drawPictureGm(pictureNum, *gmBackend(g_comprehend->gfxBackend()),
				  g_comprehend->_drawFlags, g_comprehend->_drawSurface);
}

} // namespace Comprehend
} // namespace Glk
