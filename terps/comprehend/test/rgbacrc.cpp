/* rgbacrc.cpp -- behaviour anchor for the Apple II hi-res -> RGBA artifact
 * renderer. Feeds a deterministic synthetic hi-res page through the public
 * gmSetPage/gmBlitToSurface path and prints an FNV-1a checksum of the RGBA
 * output. Used to prove the artifact-kernel extraction (gm_artifact) is
 * byte-for-byte behaviour-preserving: the checksum must be identical before
 * and after the refactor.
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

	const int w = 280, h = 160;
	std::vector<uint32_t> buf((size_t)w * h, 0);
	gmBlitToSurface(buf.data(), w, h);

	uint32_t crc = 2166136261u;
	for (uint32_t v : buf) {
		crc ^= v;
		crc *= 16777619u;
	}

	// Golden checksum captured before the gm_artifact kernel extraction. The
	// artifact renderer has no byte-exact page corpus, so this anchors it.
	const uint32_t expected = 0xbf0561c5u;
	bool ok = (crc == expected);
	printf("rgba_fnv=%08x (expected %08x) %s\n", crc, expected, ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}
