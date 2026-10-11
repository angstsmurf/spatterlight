/* unp64_probe: run unp64() on one .prg, for the regression tests in this
 * directory (see README.md).
 *
 *   unp64_probe IN.prg [SWITCHES] > OUT.prg
 *
 * Writes the unpacked program (load address and data) to stdout and exits
 * with 0, or writes nothing and exits with 1 when unp64() fails.
 *
 * Built with -DUNP64_TRACE (unp64_trace) it writes one line instead, and
 * always exits with 0: a hash over the CPU state in front of every emulated
 * instruction, the number of instructions, and "ok" or "fail". */

#include <cstdint>
#include <cstdio>
#include <vector>

#include "unp64_interface.h"

#ifdef UNP64_TRACE
#include "6502_emu.h"

static uint64_t g_traceHash = 0xcbf29ce484222325ull;
static uint64_t g_traceCount = 0;

namespace Unp64 {
void unp64TraceHook(const CpuCtx *r) {
	const uint8_t state[] = {
		(uint8_t)r->_pc, (uint8_t)(r->_pc >> 8), r->_a, r->_x, r->_y, r->_sp, r->_flags,
		(uint8_t)r->_cycles, (uint8_t)(r->_cycles >> 8), (uint8_t)(r->_cycles >> 16), (uint8_t)(r->_cycles >> 24),
		r->_mem[1]};
	for (uint8_t b : state)
		g_traceHash = (g_traceHash ^ b) * 0x100000001b3ull;
	g_traceCount++;
}
} // namespace Unp64
#endif

int main(int argc, char **argv) {
	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: unp64_probe IN.prg [SWITCHES] > OUT.prg\n");
		return 2;
	}

	FILE *f = fopen(argv[1], "rb");
	if (f == nullptr) {
		perror(argv[1]);
		return 2;
	}
	std::vector<uint8_t> in;
	uint8_t chunk[4096];
	size_t got;
	while ((got = fread(chunk, 1, sizeof chunk, f)) > 0)
		in.insert(in.end(), chunk, chunk + got);
	fclose(f);

	/* The callers hand unp64() a 64 KiB buffer; so does the probe. */
	std::vector<uint8_t> out(0x10000);
	size_t outLength = 0;
	int ok = unp64(in.data(), in.size(), out.data(), &outLength, argc == 3 ? argv[2] : nullptr);
#ifdef UNP64_TRACE
	printf("%016llx\t%llu\t%s\n", (unsigned long long)g_traceHash,
	       (unsigned long long)g_traceCount, ok ? "ok" : "fail");
	return 0;
#endif
	if (!ok)
		return 1;

	fwrite(out.data(), 1, outLength, stdout);
	return 0;
}
