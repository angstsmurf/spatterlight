/* test_dhgr_determinism.cpp -- does a DHGR image's render depend on prior state?
 *
 * Renders every RA image in isolation (reset+draw) to get a reference, then
 * renders it again after drawing a different image first (reset+draw between),
 * and reports any image whose page bytes differ. The engine draws a background
 * room as gmDhgrResetScreen(white)+gmDhgrDrawImage, so a deterministic renderer
 * MUST produce identical output regardless of what was on screen before.
 *
 * Build:
 *   c++ -std=c++17 -I.. -I../../common_sagadraw test_dhgr_determinism.cpp \
 *       ../graphics_magician_dhgr.cpp ../../common_sagadraw/gm_artifact.c -o test_dhgr_determinism
 */
#include "graphics_magician_dhgr.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Glk::Comprehend;

static std::vector<uint8_t> readFile(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	std::vector<uint8_t> v(n); fread(v.data(), 1, n, f); fclose(f);
	return v;
}

int main(int argc, char **argv) {
	const char *dir = (argc > 1) ? argv[1] : "dhgr/fixtures";
	char path[512];
	auto load = [&](const char *name) { snprintf(path, sizeof path, "%s/%s", dir, name); return readFile(path); };

	auto t5 = load("T5");
	if (!gmDhgrInstallDrawingTables(t5.data(), t5.size())) { printf("FAIL: install\n"); return 1; }

	auto ra = load("RA");
	uint32_t off[17];
	for (int i = 0; i < 16; i++) off[i] = ra[i * 2] | (ra[i * 2 + 1] << 8);
	off[16] = ra.size();

	// Count valid images (offset within file, non-empty).
	std::vector<int> imgs;
	for (int i = 0; i < 16; i++)
		if (off[i] >= 32 && off[i] < ra.size() && off[i + 1] > off[i]) imgs.push_back(i);
	printf("RA images: %zu\n", imgs.size());

	auto render = [&](int i, bool white) {
		gmDhgrResetScreen(white);
		gmDhgrDrawImage(ra.data() + off[i], off[i + 1] - off[i]);
	};
	auto snapshot = [&]() {
		std::vector<uint8_t> s(0x2000 * 2);
		memcpy(s.data(), gmDhgrMainPtr(), 0x2000);
		memcpy(s.data() + 0x2000, gmDhgrAuxPtr(), 0x2000);
		return s;
	};

	int bad = 0;
	for (int bg = 0; bg <= 1; bg++) {
		bool white = (bg == 0);
		for (int i : imgs) {
			render(i, white);
			auto ref = snapshot();
			for (int j : imgs) {
				if (j == i) continue;
				render(j, white);          // draw a different image first
				render(i, white);          // then re-draw i exactly as the engine would
				auto got = snapshot();
				if (memcmp(ref.data(), got.data(), ref.size()) != 0) {
					int bits = 0;
					for (size_t k = 0; k < ref.size(); k++)
						bits += __builtin_popcount((ref[k] ^ got[k]) & 0x7f);
					printf("ORDER-DEPENDENT: img %d differs after img %d (%s bg): %d bits\n",
					       i, j, white ? "white" : "black", bits);
					bad++;
					break; // one example per image is enough
				}
			}
		}
	}
	printf(bad ? "FAIL: %d order-dependent images\n" : "PASS: all renders deterministic\n", bad);
	return bad ? 1 : 0;
}
