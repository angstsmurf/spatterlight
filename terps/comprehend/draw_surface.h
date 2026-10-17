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

#ifndef GLK_COMPREHEND_GRAPHICS_H
#define GLK_COMPREHEND_GRAPHICS_H

#include "comprehend_compat.h"

namespace Glk {
namespace Comprehend {

#define G_RENDER_WIDTH  280
#define G_RENDER_HEIGHT 160

// The Apple II renderers (standard and double hi-res) render the 560 NTSC dots
// of a scanline over the same 160 rows. The surface is widened to this; the
// window blit halves the horizontal pixel step so the picture keeps the
// standard physical size and aspect ratio.
#define G_RENDER_WIDTH_APPLE 560

// The RGBA page a Graphics Magician renderer blits its picture to, and the
// window blit reads back from.
class DrawSurface : public Graphics::ManagedSurface {
public:
	DrawSurface() {
		reset();
	}

	/**
	 * Sets up the surface to the correct size and pixel format
	 */
	void reset();

	/**
	 * Re-create the surface at a given render width (G_RENDER_WIDTH for the DOS
	 * renderers, G_RENDER_WIDTH_APPLE for the Apple II ones), keeping the
	 * picture height and pixel format.
	 */
	void setRenderWidth(int width);
};

} // namespace Comprehend
} // namespace Glk

#endif
