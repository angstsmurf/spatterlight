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
#include "comprehend_compat.h"
#include "charset.h"
#include <vector>
#include "comprehend.h"
#include "draw_surface.h"
#include "file_buf.h"
#include "game.h"
#include "game_data.h"

namespace Glk {
namespace Comprehend {

#define IMAGES_PER_FILE 16

// Opcode enum is shared via graphics_magician.h (included above).

enum SpecialOpcode {
	RESETOP_0 = 0,
	RESETOP_RESET = 1,
	RESETOP_OO_TOPOS_UNKNOWN = 3
};

/*-------------------------------------------------------*/

uint32 Pics::ImageContext::getFillColor() const {
	uint color = _fillColor;

	// FIXME: Properly display text color in Crimson Crown
	if (g_vm->getGameID() == "crimsoncrown" && color == 0x000000ff)
		color = G_COLOR_WHITE;

	return color;
}

void Pics::ImageContext::lineFixes() {
	// WORKAROUND: Fix lines on title screens so floodfill works correctly
	if (g_vm->getGameID() == "transylvania" && _picIndex == 9999) {
		_drawSurface->drawLine(191, 31, 192, 31, G_COLOR_BLACK); // v
		_drawSurface->drawLine(196, 50, 197, 50, G_COLOR_BLACK); // a
		_drawSurface->drawLine(203, 49, 204, 49, G_COLOR_BLACK);
		_drawSurface->drawLine(197, 53, 202, 53, G_COLOR_BLACK);
		_drawSurface->drawLine(215, 51, 220, 51, G_COLOR_BLACK); // n
		_drawSurface->drawLine(221, 51, 222, 51, G_COLOR_BLACK);
		_drawSurface->drawLine(228, 50, 229, 50, G_COLOR_BLACK);
		_drawSurface->drawLine(217, 59, 220, 59, G_COLOR_BLACK);
		_drawSurface->drawLine(212, 49, 212, 50, G_COLOR_BLACK);
		_drawSurface->drawLine(213, 49, 213, 52, G_COLOR_WHITE);
		_drawSurface->drawLine(235, 52, 236, 61, G_COLOR_BLACK); // i
		_drawSurface->drawLine(237, 61, 238, 61, G_COLOR_BLACK);
	}

	if (g_vm->getGameID() == "crimsoncrown" && _picIndex == 9999 && _x == 67 && _y == 55) {
		_drawSurface->drawLine(78, 28, 77, 29, G_COLOR_WHITE);
		_drawSurface->drawLine(71, 43, 69, 47, G_COLOR_WHITE);
		_drawSurface->drawLine(67, 57, 68, 56, G_COLOR_WHITE);
		_drawSurface->drawLine(79, 101, 80, 101, G_COLOR_WHITE);
		_drawSurface->drawLine(183, 101, 184, 100, G_COLOR_WHITE);
		_drawSurface->drawLine(193, 47, 193, 48, G_COLOR_WHITE);
		_drawSurface->drawLine(68, 48, 71, 48, G_COLOR_BLACK);
	}
}

/*-------------------------------------------------------*/

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
			const Common::String &id = g_comprehend->getGameID();
			if (id == "transylvania")
				_imageOffsets[0] = 0xFE;
			else if (id == "crimsoncrown" || id == "ootopos" ||
			         id == "covetedmirror")
				_imageOffsets[0] = 0x100;
			else
				_imageOffsets[0] = 0; // talisman
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

void Pics::ImageFile::draw(uint index, ImageContext *ctx) const {
	if (!ctx->_file.open(_filename))
		error("Opening image file");

	ctx->_file.seek(_imageOffsets[index]);

	for (bool done = false; !done;) {
		done = doImageOp(ctx);
	}
}

bool Pics::ImageFile::doImageOp(Pics::ImageContext *ctx) const {
	uint8 opcode;
	uint16 a, b;

	opcode = ctx->_file.readByte();
	debugCN(kDebugGraphics, "  %.4x [%.2x]: ", (int)ctx->_file.pos() - 1, opcode);

	byte param = opcode & 0xf;
	opcode >>= 4;

	bool talisman = g_vm->getGameID() == "talisman";

	switch (opcode) {
	case OPCODE_END2: // 7
		if (talisman) {
			// Talisman (and the Lands Beyond scenario) repurpose 0x7x: it is
			// NOT an end-of-image marker (op0 alone terminates) but a 2-operand
			// opcode that appears in the per-image preamble. Its exact effect is
			// still being reverse-engineered; for now we consume its operands so
			// the rest of the vector stream stays aligned and renders.
			a = imageGetOperand(ctx) + (param & 1 ? 256 : 0);
			b = imageGetOperand(ctx);
			(void)a; (void)b;
			break;
		}
		// fallthrough
	case OPCODE_END: // 0
		// End of the rendering
		debugC(kDebugGraphics, "End of image");
		return true;

	case OPCODE_SET_TEXT_POS: // 1
		a = imageGetOperand(ctx) + (param & 1 ? 256 : 0);
		b = imageGetOperand(ctx);
		debugC(kDebugGraphics, "set_text_pos(%d, %d)", a, b);

		ctx->_textX = a;
		ctx->_textY = b;
		break;

	case OPCODE_SET_PEN_COLOR: // 2
		debugC(kDebugGraphics, "set_pen_color(%.2x)", opcode);
		if (!(ctx->_drawFlags & IMAGEF_NO_FILL))
			ctx->_penColor = ctx->_drawSurface->getPenColor(param);
		break;

	case OPCODE_TEXT_CHAR: // 3
	case OPCODE_TEXT_OUTLINE: // 5
		// Text outline mode draws a bunch of pixels that sort of looks like the char
		// TODO: See if the outline mode is ever used
		if (opcode == OPCODE_TEXT_OUTLINE)
			warning("TODO: Implement drawing text outlines");

		a = imageGetOperand(ctx);
		if (a < 0x20 || a >= 0x7f) {
			warning("Invalid character - %c", a);
			a = '?';
		}

		debugC(kDebugGraphics, "draw_char(%c)", a);
		if (ctx->_font) {
			ctx->_font->drawChar(ctx->_drawSurface, a, ctx->_textX, ctx->_textY, ctx->getFillColor());
			ctx->_textX += ctx->_font->getCharWidth(a);
		}
		break;

	case OPCODE_SET_SHAPE: // 4
		debugC(kDebugGraphics, "set_shape_type(%.2x)", param);

		if (param == 8) {
			// FIXME: This appears to be a _shape type. Only used by OO-Topos
			warning("TODO: Shape type 8");
			ctx->_shape = SHAPE_PIXEL;
		} else {
			ctx->_shape = (Shape)param;
		}
		break;

	case OPCODE_SET_FILL_COLOR: // 6
		a = imageGetOperand(ctx);
		debugC(kDebugGraphics, "set_fill_color(%.2x)", a);
		ctx->_fillColor = ctx->_drawSurface->getFillColor(a);
		break;

	case OPCODE_MOVE_TO: // 8
		a = imageGetOperand(ctx) + (param & 1 ? 256 : 0);
		b = imageGetOperand(ctx);

		debugC(kDebugGraphics, "move_to(%d, %d)", a, b);
		ctx->_x = a;
		ctx->_y = b;
		break;

	case OPCODE_DRAW_BOX: // 9
		a = imageGetOperand(ctx) + (param & 1 ? 256 : 0);
		b = imageGetOperand(ctx);

		debugC(kDebugGraphics, "draw_box (%d, %d) - (%d, %d)",
		       ctx->_x, ctx->_y, a, b);

		ctx->_drawSurface->drawBox(ctx->_x, ctx->_y, a, b, ctx->_penColor);
		break;

	case OPCODE_DRAW_LINE: // 10
		a = imageGetOperand(ctx) + (param & 1 ? 256 : 0);
		b = imageGetOperand(ctx);

		debugC(kDebugGraphics, "draw_line (%d, %d) - (%d, %d)",
		       ctx->_x, ctx->_y, a, b);
		ctx->_drawSurface->drawLine(ctx->_x, ctx->_y, a, b, ctx->_penColor);

		ctx->_x = a;
		ctx->_y = b;
		break;

	case OPCODE_DRAW_CIRCLE: // 11
		a = imageGetOperand(ctx);
		debugC(kDebugGraphics, "draw_circle (%d, %d) diameter=%d",
		       ctx->_x, ctx->_y, a);

		ctx->_drawSurface->drawCircle(ctx->_x, ctx->_y, a, ctx->_penColor);
		break;

	case OPCODE_DRAW_SHAPE: // 12
		a = imageGetOperand(ctx) + (param & 1 ? 256 : 0);
		b = imageGetOperand(ctx);
		debugC(kDebugGraphics, "draw_shape(%d, %d), style=%.2x, fill=%.2x",
		       a, b, ctx->_shape, ctx->_fillColor);

		if (!(ctx->_drawFlags & IMAGEF_NO_FILL))
			ctx->_drawSurface->drawShape(a, b, ctx->_shape, ctx->_fillColor);
		break;

	case OPCODE_DELAY: // 13
		// The original allowed for rendering to be paused briefly. We don't do
		// that in ScummVM, and just show the finished rendered image
		(void)imageGetOperand(ctx);
		break;

	case OPCODE_PAINT: // 14
		a = imageGetOperand(ctx) + (param & 1 ? 256 : 0);
		b = imageGetOperand(ctx);
		if (opcode & 0x1)
			a += 255;

		debugC(kDebugGraphics, "paint(%d, %d)", a, b);
		ctx->lineFixes();
		if (!(ctx->_drawFlags & IMAGEF_NO_FILL))
			ctx->_drawSurface->floodFill(a, b, ctx->_fillColor);
		break;

	case OPCODE_RESET: // 15
		if (talisman) {
			// The reset sub-opcode is the low nibble (param), exactly as the
			// pixel-validated Apple renderer (graphics_magician OPCODE_RESET)
			// reads it. The operand count MUST match that renderer or the rest
			// of the vector stream desyncs: only sub-op 2 carries operands (four
			// bytes of fill bounds); every other sub-op (0/1/3) has none. The
			// earlier code read one operand for *every* op15, which shifted the
			// whole stream -- Talisman rooms begin with op15, so they desynced
			// from the first opcode.
			if (param == 2) {
				// Fill bounds (right, left, bottom, top). Consumed to stay in
				// sync; the DOS draw surface does not yet clip fills to them.
				imageGetOperand(ctx);
				imageGetOperand(ctx);
				imageGetOperand(ctx);
				imageGetOperand(ctx);
			}
			doResetOp(ctx, param);
		}
		// FIXME: The reset case was causing room outside cell to be drawn all
		// white for the earlier (V1) games, so it stays disabled for them.
		break;
	}

	//ctx->_drawSurface->dumpToScreen();

	return false;
}

void Pics::ImageFile::doResetOp(ImageContext *ctx, byte param) const {
	switch (param) {
	case RESETOP_0:
		// In Transylvania this sub-opcode is a do nothing
		break;

	case RESETOP_RESET:
		// TODO: Calls same reset that first gets called when rendering starts.
		// Figure out what the implication of resetting the variables does
		break;

	case RESETOP_OO_TOPOS_UNKNOWN:
		// TODO: This is called for some scenes in OO-Topis. Figure out what it does
		break;

	default:
		break;
	}
}

uint16 Pics::ImageFile::imageGetOperand(ImageContext *ctx) const {
	return ctx->_file.readByte();
}

void Pics::ImageFile::renderApple(uint index) const {
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

	gmDrawImage(buf.data(), len);
}

void Pics::ImageFile::renderGmcga(uint index) const {
	Common::File f;
	if (!f.open(_filename))
		error("Opening image file");

	int64 start = _imageOffsets[index];
	int64 fsize = f.size();
	if (start < 0 || start >= fsize)
		return;

	size_t len = (size_t)(fsize - start);
	std::vector<byte> buf(len);
	f.seek(start);
	f.read(buf.data(), (uint32)len);

	gmcgaDrawImage(buf.data(), len);
}

/*-------------------------------------------------------*/

Pics::Pics() : _font(nullptr) {
	if (Common::File::exists("charset.gda")) {
		_font = new CharSet();
	} else if (!Common::DiskImageFS::active()) {
		// The DOS release keeps the in-picture font inside novel.exe. The Apple II
		// release has no novel.exe (and its room images don't draw text), so skip
		// it rather than erroring out on a file that cannot exist.
		_font = new TalismanFont();
	}

	// DOS releases: load the CGA drawing tables (fill patterns, brushes, font)
	// out of the Penguin Graphics Magician interpreter image (NOVEL.EXE). The
	// loader locates the tables by signature, so it succeeds for every Comprehend
	// v2 game that shares the CGA interpreter (Talisman, OO-Topos, Transylvania
	// v2, The Coveted Mirror) and silently no-ops for the older v1 releases
	// (Crimson Crown, Transylvania v1) and for any rip that lacks the file.
	// Pictures route through graphics_magician_cga whenever the tables loaded.
	if (!Common::DiskImageFS::active()) {
		Common::File exe;
		if (exe.open("novel.exe") || exe.open("NOVEL.EXE")) {
			int64 sz = exe.size();
			if (sz > 0) {
				std::vector<byte> buf((size_t)sz);
				if (exe.read(buf.data(), (uint32)sz) == (uint32)sz)
					gmcgaInstallDrawingTables(buf.data(), (size_t)sz);
			}
		}
	}
}

Pics::~Pics() {
	delete _font;
}

void Pics::clear() {
	_rooms.clear();
	_items.clear();
}

void Pics::load(const Common::StringArray &roomFiles,
				const Common::StringArray &itemFiles,
				const Common::String &titleFile) {
	clear();

	for (uint idx = 0; idx < roomFiles.size(); ++idx)
		_rooms.push_back(ImageFile(roomFiles[idx]));
	for (uint idx = 0; idx < itemFiles.size(); ++idx)
		_items.push_back(ImageFile(itemFiles[idx]));

	if (!titleFile.empty())
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
	if (num < ITEMS_OFFSET && (num % 100) < (int)(_rooms.size() * IMAGES_PER_FILE))
		return true;

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

void Pics::drawPicture(int pictureNum) const {
	ImageContext ctx(g_comprehend->_drawSurface, _font, g_comprehend->_drawFlags, pictureNum);

	// Apple II: route through the faithful standard hi-res renderer. Pictures are
	// decoded onto a persistent Apple hi-res page (so item overlays compose onto
	// the room already drawn there, exactly as the real interpreter does), then
	// the whole page is converted to RGBA and blitted. All four Apple disk titles
	// (Talisman, Transylvania, OO-Topos, Crimson Crown) are validated pixel-exact
	// vs MAME; the dialect differences are handled by gmSetLegacyFormat().
	if (Common::DiskImageFS::active()) {
		DrawSurface *ds = ctx._drawSurface;

		if (pictureNum == DARK_ROOM) {
			gmResetScreen(false);
		} else if (pictureNum == BRIGHT_ROOM) {
			gmResetScreen(true);
		} else if (pictureNum == TITLE_IMAGE) {
			// The Apple II title (T0) is a Graphics Magician vector image. The
			// legacy titles (Talisman/Transylvania/Crimson Crown) are drawn on a
			// white background like a bright room; OO-Topos starts from black (it
			// fills its own background with op15, and its lettering blends with
			// the page, so a white start corrupts it).
			gmResetScreen(g_comprehend->getGameID() != "ootopos");
			if (_title.isLoaded())
				_title.renderApple(0);
		} else if (pictureNum >= ITEMS_OFFSET) {
			// Item overlay: draw on top of the existing room page.
			int n = pictureNum - ITEMS_OFFSET;
			_items[n / IMAGES_PER_FILE].renderApple(n % IMAGES_PER_FILE);
		} else {
			// Room picture. Background variants start from a fresh page; the
			// no-background variant composes onto whatever is already shown.
			if (pictureNum < LOCATIONS_NO_BG_OFFSET)
				gmResetScreen(!(ctx._drawFlags & IMAGEF_REVERSE));
			int n = pictureNum % 100;
			_rooms[n / IMAGES_PER_FILE].renderApple(n % IMAGES_PER_FILE);
		}

		// With slow-draw active the page is revealed a chunk at a time by the
		// host (Comprehend::drawPicture); blit only what is on the visible page
		// so far. Otherwise blit the finished page.
		if (gmSlowDrawActive())
			gmBlitSlowToSurface((uint32 *)ds->getPixels(), ds->w, ds->h);
		else
			gmBlitToSurface((uint32 *)ds->getPixels(), ds->w, ds->h);
		return;
	}

	// DOS v2 releases (Talisman, OO-Topos, Transylvania v2, The Coveted Mirror):
	// route through the shared CGA Graphics Magician renderer once its drawing
	// tables have been loaded from NOVEL.EXE. The disk/Apple path already returned
	// above, and gmcgaHaveDrawingTables() is only ever true for an eligible DOS
	// game, so this flag alone identifies the right renderer.
	if (gmcgaHaveDrawingTables()) {
		DrawSurface *ds = ctx._drawSurface;

		if (pictureNum == DARK_ROOM) {
			gmcgaResetScreen(false);
		} else if (pictureNum == BRIGHT_ROOM) {
			gmcgaResetScreen(true);
		} else if (pictureNum == TITLE_IMAGE) {
			// The v2 title streams open with op15/3, which paints the whole
			// picture with the startup fill (white), so the reset value is
			// cosmetic; white matches the native interpreter's text page.
			gmcgaResetScreen(true);
			if (_title.isLoaded())
				_title.renderGmcga(0);
		} else if (pictureNum >= ITEMS_OFFSET) {
			int n = pictureNum - ITEMS_OFFSET;
			_items[n / IMAGES_PER_FILE].renderGmcga(n % IMAGES_PER_FILE);
		} else {
			if (pictureNum < LOCATIONS_NO_BG_OFFSET)
				gmcgaResetScreen(!(ctx._drawFlags & IMAGEF_REVERSE));
			int n = pictureNum % 100;
			_rooms[n / IMAGES_PER_FILE].renderGmcga(n % IMAGES_PER_FILE);
		}

		// As with the Apple path: while a reveal is queued, blit only what is on
		// the visible page so far; the host drives the rest on a timer.
		if (gmcgaSlowDrawActive())
			gmcgaBlitSlowToSurface((uint32 *)ds->getPixels(), ds->w, ds->h);
		else
			gmcgaBlitToSurface((uint32 *)ds->getPixels(), ds->w, ds->h);
		return;
	}

	if (pictureNum == DARK_ROOM) {
		ctx._drawSurface->clearScreen(G_COLOR_BLACK);

	} else if (pictureNum == BRIGHT_ROOM) {
		ctx._drawSurface->clearScreen(G_COLOR_WHITE);

	} else if (pictureNum == TITLE_IMAGE) {
		ctx._drawSurface->clearScreen(G_COLOR_WHITE);
		// Some releases (e.g. Apple II) have no vector title image.
		if (_title.isLoaded())
			_title.draw(0, &ctx);

	} else if (pictureNum >= ITEMS_OFFSET) {
		pictureNum -= ITEMS_OFFSET;
		ctx._drawSurface->clear(0);
		_items[pictureNum / IMAGES_PER_FILE].draw(
		    pictureNum % IMAGES_PER_FILE, &ctx);

	} else {
		if (pictureNum < LOCATIONS_NO_BG_OFFSET) {
			ctx._drawSurface->clearScreen((ctx._drawFlags & IMAGEF_REVERSE) ? G_COLOR_BLACK : G_COLOR_WHITE);
			if (ctx._drawFlags & IMAGEF_REVERSE)
				ctx._penColor = RGB(255, 255, 255);
		} else {
			ctx._drawSurface->clear(0);
		}
		pictureNum %= 100;
		_rooms[pictureNum / IMAGES_PER_FILE].draw(
		    pictureNum % IMAGES_PER_FILE, &ctx);
	}
}

} // namespace Comprehend
} // namespace Glk
