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

#ifndef GLK_COMPREHEND_PICS_H
#define GLK_COMPREHEND_PICS_H

#include "draw_surface.h"
#include "gm_backend.h"
#include "comprehend_compat.h"

namespace Glk {
namespace Comprehend {

enum ImageFlag {
	IMAGEF_REVERSE = 1 << 0
};

// Picture numbers are packed into one int across several bands. The stride must
// exceed the largest image index (7 graphic files x IMAGES_PER_FILE = 112): The
// Coveted Mirror's astrologer constellations are room graphics 101 and 102.
enum {
	LOCATIONS_OFFSET = 0,
	LOCATIONS_NO_BG_OFFSET = 256,
	ITEMS_OFFSET = 512,
	DARK_ROOM = 1000,
	BRIGHT_ROOM = 1001,
	TITLE_IMAGE = 9999
};

class Pics {
public:
	// Called by Comprehend::drawPicture: renders the picture's bytecode
	// into the global DrawSurface, ready to be blitted.
	void renderPicture(int pictureNum) const { drawPicture(pictureNum); }
private:
	struct ImageFile {
	private:
		Common::Array<uint16> _imageOffsets;
		Common::Path _filename;

	public:
		ImageFile() {}
		ImageFile(const Common::String &filename, bool isSingleImage = false);

		// False when no image file was loaded (e.g. games whose title screen
		// is not a vector image, or releases that ship no title file at all).
		bool isLoaded() const { return !_imageOffsets.empty(); }

		// Decode image `index`'s vector stream onto the persistent page of a
		// Graphics Magician renderer (see gm_backend.h).
		void renderGm(uint index, const GmBackend &gm) const;
	};

private:
	Common::Array<ImageFile> _rooms;
	Common::Array<ImageFile> _items;
	ImageFile _title;

private:
	/**
	 * Draw the specified picture
	 */
	void drawPicture(int pictureNum) const;
	void drawPictureGm(int pictureNum, const GmBackend &gm, uint drawFlags,
					   DrawSurface *ds) const;

public:
	Pics();

	void clear();

	void load(const Common::StringArray &roomFiles,
			  const Common::StringArray &itemFiles,
			  const Common::String &titleFile);
};

} // namespace Comprehend
} // namespace Glk

#endif
