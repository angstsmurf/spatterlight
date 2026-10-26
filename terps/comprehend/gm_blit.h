/* gm_blit.h -- the last step of the Apple II renderers' RGBA conversion.
 *
 * Both the standard and the double hi-res renderer expand a scanline into 560
 * native artifact colours (gm_render_line_colors / gm_render_dhgr_line_colors)
 * and then sample those down to the surface width with the Apple palette.  The
 * sampling is the same; only the double hi-res renderer maps 1:1 for a surface
 * at least 560 wide, where the standard one keeps scaling.
 */

#ifndef GLK_COMPREHEND_GM_BLIT_H
#define GLK_COMPREHEND_GM_BLIT_H

#include <cstdint>
#include "gm_artifact.h"

namespace Glk {
namespace Comprehend {

// Write one output scanline of `w` pixels (0xRRGGBBAA) from the 560 artifact
// colour indices of a row.  With `oneToOneWhenWide`, a surface w >= 560 reads
// column x directly (clamped to 559); otherwise column x*560/w.
inline void gmSampleRow560(const uint8_t colors560[560], uint32_t *out, int w,
						   bool oneToOneWhenWide) {
	const bool direct = oneToOneWhenWide && w >= 560;
	for (int x = 0; x < w; x++) {
		int sx = direct ? x : (x * 560) / w;
		if (sx > 559) sx = 559;
		uint32_t rgb = gm_apple2_palette[colors560[sx] & 0x0f];
		out[x] = (rgb << 8) | 0xff;
	}
}

} // namespace Comprehend
} // namespace Glk

#endif
