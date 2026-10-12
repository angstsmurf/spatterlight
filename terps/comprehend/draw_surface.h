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

// Double hi-res ("<D>" mode) renders 560 native columns over the same 160 rows.
// The surface is widened to this; the window blit halves the horizontal pixel
// step so the picture keeps the standard physical size and aspect ratio.
#define G_RENDER_WIDTH_DHGR 560

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
	 * Re-create the surface at a given render width (G_RENDER_WIDTH for standard
	 * hi-res, G_RENDER_WIDTH_DHGR for double hi-res), keeping the picture height
	 * and pixel format. Used when toggling the Apple II <D> double hi-res mode.
	 */
	void setRenderWidth(int width);
};

} // namespace Comprehend
} // namespace Glk

#endif
