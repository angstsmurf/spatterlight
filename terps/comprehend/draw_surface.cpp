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

#include "draw_surface.h"
#include "comprehend.h"
#include "pics.h"


namespace Glk {
namespace Comprehend {

/*-------------------------------------------------------*/

void DrawSurface::reset() {
	create(G_RENDER_WIDTH, G_RENDER_HEIGHT,
		   Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0));
}

void DrawSurface::setRenderWidth(int width) {
	create(width, G_RENDER_HEIGHT,
		   Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0));
}

} // namespace Comprehend
} // namespace Glk
