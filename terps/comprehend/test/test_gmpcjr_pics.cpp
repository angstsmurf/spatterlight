/* test_gmpcjr_pics.cpp -- regression test for the IBM PCjr / Tandy 16-colour
 * renderer (graphics_magician_pcjr.cpp).
 *
 * Two checks:
 *
 *  - A change detector: it renders every picture of the two PCjr-capable
 *    releases (Transylvania v1 and Crimson Crown) and compares a hash of each
 *    280x160 result against the committed list in test/gmpcjr/hashes.txt.
 *
 *  - Ground truth, where there is any: every
 *    <games-dir>/captures/<game-folder>.<picture-file>.<image-index>.ppm is a
 *    280x160 P6 crop of the picture window from a DOSBox-X screenshot of the
 *    real game (see test/gmpcjr/README.md), and the same picture must render
 *    to exactly those pixels. A screenshot of a room with objects in it names
 *    them all, in drawing order: <game-folder>.RA.MS1.6+OA.MS1.0+OB.MS1.7.ppm.
 *    Local-only like the games; none present is fine.
 *
 * Unlike the CGA tests this one reads the game folders themselves (it needs
 * JR_GRAPH.OVR, NOVEL1.EXE/NOVEL.EXE, CHARSET.GDA and every .MS1/.MS2 picture
 * file). They are copyrighted and local-only: by default they are looked for in
 * test/gmpcjr/fixtures/<game>/ (gitignored). A game whose folder is absent is
 * skipped with a message, and the test still passes.
 *
 *   ./test/test_gmpcjr_pics [games-dir]            check against hashes.txt
 *   ./test/test_gmpcjr_pics --bless [games-dir]    rewrite hashes.txt
 *
 * hashes.txt lines:  <game-folder> <picture-file> <image-index> <fnv1a-32>
 * with index -1 for a single-image title file. The hash is FNV-1a over the
 * picture's R,G,B bytes in row order, i.e. over the pixel payload of the PPM
 * that test/gmpcjr/pcjrtest writes for the same picture.
 */

#include "../graphics_magician_pcjr.h"
#include "pic_fixture_test.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

using namespace Glk::Comprehend;

static const char *const kHashFile = "test/gmpcjr/hashes.txt";
static const char *const kDefaultGamesDir = "test/gmpcjr/fixtures";

enum { kWidth = 280, kHeight = 160, kImagesPerFile = 16 };

static std::string upper(std::string s) {
	for (char &c : s)
		c = (char)toupper((unsigned char)c);
	return s;
}

// Directory entries (regular names only), sorted by upper-cased name so the
// blessed list does not depend on the file system's ordering or case.
static std::vector<std::string> listDir(const std::string &dir) {
	std::vector<std::string> names;
	if (DIR *d = opendir(dir.c_str())) {
		while (struct dirent *e = readdir(d))
			if (e->d_name[0] != '.')
				names.push_back(e->d_name);
		closedir(d);
	}
	std::sort(names.begin(), names.end(), [](const std::string &a, const std::string &b) {
		return upper(a) < upper(b);
	});
	return names;
}

// Read `name` from `dir`, matching the name case-insensitively.
static std::vector<uint8_t> readGameFile(const std::string &dir, const std::string &name) {
	for (const std::string &n : listDir(dir))
		if (upper(n) == upper(name))
			return readFile(dir + "/" + n);
	return std::vector<uint8_t>();
}

// Install the drawing tables and font of the game in `dir`.
static bool loadGame(const std::string &dir) {
	std::vector<uint8_t> ovr = readGameFile(dir, "JR_GRAPH.OVR");
	std::vector<uint8_t> exe = readGameFile(dir, "NOVEL1.EXE");
	if (exe.empty())
		exe = readGameFile(dir, "NOVEL.EXE");
	if (ovr.empty() || exe.empty())
		return false;
	if (!gmpcjrInstallDrawingTables(ovr.data(), ovr.size(), exe.data(), exe.size()))
		return false;
	std::vector<uint8_t> charset = readGameFile(dir, "CHARSET.GDA");
	if (!charset.empty())
		gmpcjrSetFont(charset.data(), charset.size());
	return true;
}

// Offset of image `index` in a picture file (-1 = single-image title file), or
// false if the file has no such image.
static bool imageStart(const std::vector<uint8_t> &pic, int index, uint32_t &start) {
	if (index < 0) {
		// 4-byte header iff the first byte is 0, matching pics.cpp's title sniff.
		if (pic.empty())
			return false;
		start = (pic[0] == 0) ? 4 : 0;
	} else {
		if (pic.size() < 4 + kImagesPerFile * 2)
			return false;
		bool hasHeader = (pic[0] | (pic[1] << 8)) == 0x1000;
		size_t o = (hasHeader ? 4 : 0) + (size_t)index * 2;
		start = (uint32_t)(pic[o] | (pic[o + 1] << 8)) + (hasHeader ? 4 : 0);
	}
	return start < pic.size();
}

// One picture: the file's bytes and where the image starts in them.
struct Picture {
	std::vector<uint8_t> data;
	uint32_t start;
};

// Render pictures on top of each other, to R,G,B bytes in row order.
static std::vector<uint8_t> renderRGB(const std::vector<Picture> &pics) {
	gmpcjrResetScreen(true);   // white room background
	for (const Picture &p : pics)
		gmpcjrDrawImage(p.data.data() + p.start, p.data.size() - p.start);
	std::vector<uint32_t> rgba(kWidth * kHeight);
	gmpcjrBlitToSurface(rgba.data(), kWidth, kHeight);

	std::vector<uint8_t> rgb;
	rgb.reserve(rgba.size() * 3);
	for (uint32_t c : rgba)
		for (int shift = 24; shift >= 8; shift -= 8)
			rgb.push_back((uint8_t)(c >> shift));
	return rgb;
}

static uint32_t renderHash(const std::vector<uint8_t> &pic, uint32_t start) {
	uint32_t h = 2166136261u;
	for (uint8_t b : renderRGB({ Picture{ pic, start } }))
		h = (h ^ b) * 16777619u;
	return h;
}

// Compare the pictures that have an emulator capture against it. Returns the
// number that differ; `checked` counts the captures compared.
static int checkCaptures(const std::string &gamesDir, int &checked) {
	static const char kHeader[] = "P6\n280 160\n255\n";
	const size_t headerLen = sizeof(kHeader) - 1;
	const std::string dir = gamesDir + "/captures";
	int failed = 0;
	for (const std::string &name : listDir(dir)) {
		// <game-folder>.<FILE>.<EXT>.<index>[+<FILE>.<EXT>.<index>...].ppm
		size_t dot = name.find('.');
		if (dot == std::string::npos || name.size() < 4 || name.compare(name.size() - 4, 4, ".ppm") != 0)
			continue;
		const std::string g = name.substr(0, dot);
		checked++;
		std::vector<uint8_t> want = readFile(dir + "/" + name);
		bool ok = want.size() == headerLen + kWidth * kHeight * 3
			&& memcmp(want.data(), kHeader, headerLen) == 0 && loadGame(gamesDir + "/" + g);
		std::vector<Picture> pics;
		std::string specs = name.substr(dot + 1, name.size() - 4 - (dot + 1));
		for (size_t at = 0; ok && at <= specs.size();) {
			size_t plus = std::min(specs.find('+', at), specs.size());
			char base[64], ext[16];
			int index;
			Picture p;
			ok = sscanf(specs.substr(at, plus - at).c_str(), "%63[^.].%15[^.].%d", base, ext, &index) == 3;
			if (ok) {
				p.data = readGameFile(gamesDir + "/" + g, std::string(base) + "." + ext);
				ok = imageStart(p.data, index, p.start);
				pics.push_back(p);
			}
			at = plus + 1;
		}
		if (!ok) {
			printf("FAIL  capture %s: cannot load it or its pictures\n", name.c_str());
			failed++;
			continue;
		}
		std::vector<uint8_t> got = renderRGB(pics);
		int diffs = 0;
		for (int i = 0; i < kWidth * kHeight; i++)
			if (memcmp(&got[i * 3], &want[headerLen + i * 3], 3) != 0)
				diffs++;
		if (diffs) {
			printf("FAIL  capture %s: %d pixels differ from the emulator\n", name.c_str(), diffs);
			failed++;
		}
	}
	return failed;
}

static bool isPictureFile(const std::string &name) {
	std::string u = upper(name);
	size_t n = u.size();
	return n > 4 && (u.compare(n - 4, 4, ".MS1") == 0 || u.compare(n - 4, 4, ".MS2") == 0);
}

static bool isTitleFile(const std::string &name) {
	return upper(name).find("TITLE") != std::string::npos;
}

// Rewrite the hash list from every game folder under `gamesDir`.
static int bless(const std::string &gamesDir) {
	FILE *out = fopen(kHashFile, "w");
	if (!out) {
		fprintf(stderr, "cannot write %s (run from terps/comprehend)\n", kHashFile);
		return 1;
	}
	int count = 0;
	for (const std::string &game : listDir(gamesDir)) {
		std::string dir = gamesDir + "/" + game;
		if (!loadGame(dir))
			continue;
		for (const std::string &name : listDir(dir)) {
			if (!isPictureFile(name))
				continue;
			std::vector<uint8_t> pic = readFile(dir + "/" + name);
			bool title = isTitleFile(name);
			for (int i = title ? -1 : 0; i < (title ? 0 : (int)kImagesPerFile); i++) {
				uint32_t start;
				if (!imageStart(pic, i, start))
					continue;
				fprintf(out, "%s %s %d %08x\n", game.c_str(), upper(name).c_str(), i,
						renderHash(pic, start));
				count++;
			}
		}
	}
	fclose(out);
	printf("blessed %d pictures into %s\n", count, kHashFile);
	return count ? 0 : 1;
}

int main(int argc, char **argv) {
	bool blessing = argc > 1 && strcmp(argv[1], "--bless") == 0;
	int dirArg = blessing ? 2 : 1;
	std::string gamesDir = argc > dirArg ? argv[dirArg] : kDefaultGamesDir;
	if (blessing)
		return bless(gamesDir);

	FILE *in = fopen(kHashFile, "r");
	if (!in) {
		fprintf(stderr, "cannot read %s (run from terps/comprehend)\n", kHashFile);
		return 1;
	}

	std::string game;          // game whose tables are currently installed
	bool gamePresent = false;
	std::string picName;       // picture file currently loaded
	std::vector<uint8_t> pic;
	int checked = 0, failed = 0, skippedGames = 0;

	char g[128], f[128];
	int index;
	unsigned expected;
	while (fscanf(in, "%127s %127s %d %x", g, f, &index, &expected) == 4) {
		if (game != g) {
			game = g;
			picName.clear();
			gamePresent = loadGame(gamesDir + "/" + game);
			if (!gamePresent) {
				printf("SKIP  %s (no PCjr game folder at %s/%s)\n", g, gamesDir.c_str(), g);
				skippedGames++;
			}
		}
		if (!gamePresent)
			continue;
		if (picName != f) {
			picName = f;
			pic = readGameFile(gamesDir + "/" + game, picName);
		}
		checked++;
		uint32_t start;
		if (!imageStart(pic, index, start)) {
			printf("FAIL  %s %s %d: no such image\n", g, f, index);
			failed++;
			continue;
		}
		uint32_t got = renderHash(pic, start);
		if (got != expected) {
			printf("FAIL  %s %s %d: hash %08x, expected %08x\n", g, f, index, got, expected);
			failed++;
		}
	}
	fclose(in);

	if (checked == 0 && skippedGames == 0) {
		fprintf(stderr, "%s lists no pictures\n", kHashFile);
		return 1;
	}
	int captures = 0;
	int captureFailed = checkCaptures(gamesDir, captures);
	printf("PCjr pictures: %d checked, %d failed, %d game(s) skipped; "
		   "%d emulator capture(s) checked, %d failed\n",
		   checked, failed, skippedGames, captures, captureFailed);
	return (failed || captureFailed) ? 1 : 0;
}
