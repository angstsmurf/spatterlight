/* rgbacrc.cpp -- behaviour anchor for the Apple II hi-res -> RGBA artifact
 * renderer. Feeds a deterministic synthetic hi-res page through the public
 * gmSetPage/gmBlitToSurface path and prints an FNV-1a checksum of the RGBA
 * output, at the game's 560 columns and at 280. Used to prove the
 * artifact-kernel extraction (gm_artifact) was byte-for-byte
 * behaviour-preserving, and guards the kernel since.
 */
#include "../graphics_magician.h"
#include <cstdio>
#include <cstdint>
#include <vector>

using namespace Glk::Comprehend;

int main() {
	std::vector<uint8_t> page(0x2000);
	// Exercise the full range of byte values (including the high palette bit)
	// and every column parity / phase.
	for (size_t i = 0; i < page.size(); i++)
		page[i] = (uint8_t)(i * 37u + i / 40u + (i << 3));
	gmSetPage(page.data());

	// 280 columns: the sampled path pixtest and the tools use. Its golden was
	// captured before the gm_artifact kernel extraction. 560 columns: what the
	// game draws, every NTSC dot of the kernel (which matches MAME).
	struct { int w; uint32_t expected; } const cases[] = {
		{ 280, 0xbf0561c5u },
		{ 560, 0x84b041c5u },
	};
	const int h = 160;
	bool ok = true;
	for (const auto &c : cases) {
		std::vector<uint32_t> buf((size_t)c.w * h, 0);
		gmBlitToSurface(buf.data(), c.w, h);

		uint32_t crc = 2166136261u;
		for (uint32_t v : buf) {
			crc ^= v;
			crc *= 16777619u;
		}
		bool pass = (crc == c.expected);
		printf("w=%d rgba_fnv=%08x (expected %08x) %s\n", c.w, crc, c.expected, pass ? "PASS" : "FAIL");
		ok = ok && pass;
	}
	return ok ? 0 : 1;
}
