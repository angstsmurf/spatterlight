/* Unit tests for the parts of unp64 that need no game: the BASIC SYS line
 * parser, the switch number parser and unp64() on input that is no program.
 * Run by `make check`. */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "exo_util.h"
#include "unp64_interface.h"

using namespace Unp64;

static int g_failed = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			g_failed++; \
		} \
	} while (0)

/* A one-line BASIC program as it lies at $0801: link, line number, the
 * tokenised text, and the terminators. */
static std::vector<uint8_t> basicLine(const std::string &text) {
	std::vector<uint8_t> line = {0x0b, 0x08, 0x0a, 0x00};
	line.insert(line.end(), text.begin(), text.end());
	line.insert(line.end(), {0, 0, 0});
	return line;
}

static int sysOf(const std::string &text) {
	std::vector<uint8_t> line = basicLine(text);
	return findSys(line.data(), line.size());
}

static void testFindSys() {
	CHECK(sysOf("\x9e" "2061") == 2061);
	CHECK(sysOf("\x9e" " 2061") == 2061);
	CHECK(sysOf("\x9e" "(2061)") == 2061);
	CHECK(sysOf("\x9e" "2048\xaa" "13") == 2061);
	CHECK(sysOf("\x9e" "2100\xab" "39") == 2061);
	CHECK(sysOf("\x9e" "5E3") == 5000);
	/* A statement before the SYS, as in "10 POKE 53280,0:SYS 2061". */
	CHECK(sysOf("\x97" "53280,0:\x9e" "2061") == 2061);
	/* No SYS, no number after the SYS, nothing at all. */
	CHECK(sysOf("\x99" "\"HELLO\"") == -1);
	CHECK(sysOf("\x9e") == -1);
	CHECK(sysOf("\x9e" "X") == -1);
	CHECK(sysOf("") == -1);
	CHECK(findSys(nullptr, 100) == -1);
	uint8_t tiny[3] = {0x0b, 0x08, 0x0a};
	CHECK(findSys(tiny, sizeof tiny) == -1);
	/* The hidden SYS line: the text starts with a NUL, which ends the line
	 * for LIST, and the SYS follows a few bytes on. */
	CHECK(sysOf(std::string("\0\0\0\0\0\0", 6) + "\x9e" "2061") == 2061);
	/* A line that never ends does not run the scan off the buffer. */
	std::vector<uint8_t> endless(2000, 0x20);
	CHECK(findSys(endless.data(), endless.size()) == -1);
}

static void testStrToInt() {
	int value = -1;
	CHECK(strToInt("2061", &value) && value == 2061);
	CHECK(strToInt("$80d", &value) && value == 0x80d);
	CHECK(strToInt("0x80D", &value) && value == 0x80d);
	CHECK(strToInt("040", &value) && value == 40);
	value = 7;
	CHECK(!strToInt("", &value) && value == 7);
	CHECK(!strToInt("$", &value) && value == 7);
	CHECK(!strToInt("12z", &value) && value == 7);
	CHECK(!strToInt("99999999999999999999", &value) && value == 7);
	CHECK(!strToInt(nullptr, &value));
}

/* unp64() on things that are not a packed program: it has to fail, not crash. */
static void testNotAProgram() {
	std::vector<uint8_t> out(0x10000);
	size_t outLength = 0;

	for (size_t length = 0; length < 4; length++) {
		uint8_t stub[4] = {0x01, 0x08, 0x00, 0x00};
		CHECK(!unp64(stub, length, out.data(), &outLength, nullptr));
	}

	/* A BASIC program with no SYS line has no entry point. */
	std::vector<uint8_t> prg = {0x01, 0x08};
	std::vector<uint8_t> line = basicLine("\x99" "\"HELLO\"");
	prg.insert(prg.end(), line.begin(), line.end());
	CHECK(!unp64(prg.data(), prg.size(), out.data(), &outLength, nullptr));

	/* A program loaded to the very top of memory. */
	uint8_t top[6] = {0xfe, 0xff, 0x60, 0x60, 0x60, 0x60};
	CHECK(!unp64(top, sizeof top, out.data(), &outLength, nullptr));
}

/* "10 SYS2061" and an RTS at $080d: a program with an entry point, though
 * not a packed one. */
static std::vector<uint8_t> rtsProgram() {
	std::vector<uint8_t> prg = {0x01, 0x08};
	std::vector<uint8_t> line = basicLine("\x9e" "2061");
	prg.insert(prg.end(), line.begin(), line.end());
	prg.push_back(0x60);
	return prg;
}

/* Switch strings that cannot be parsed fail the call; they are not skipped. */
static void testSwitches() {
	std::vector<uint8_t> prg = rtsProgram();
	std::vector<uint8_t> out(UNP64_OUTPUT_CAPACITY);
	size_t outLength = 0;

	CHECK(!unp64(prg.data(), prg.size(), out.data(), &outLength, "-e"));
	CHECK(!unp64(prg.data(), prg.size(), out.data(), &outLength, "-e0xzz"));
	CHECK(!unp64(prg.data(), prg.size(), out.data(), &outLength, "-q"));
	/* One more switch than there is room for. */
	CHECK(!unp64(prg.data(), prg.size(), out.data(), &outLength, "-s -s -s -s -s"));
	/* A switch too long to be a switch. */
	std::string longSwitch = "-e0x" + std::string(80, '0') + "80d";
	CHECK(!unp64(prg.data(), prg.size(), out.data(), &outLength, longSwitch.c_str()));
}

int main() {
	testFindSys();
	testStrToInt();
	testNotAProgram();
	testSwitches();
	if (g_failed) {
		printf("unp64 unit tests: %d failed\n", g_failed);
		return 1;
	}
	printf("unp64 unit tests: passed\n");
	return 0;
}
