//
//  entrypoints_internal.hpp
//  bocfel6
//
// What entrypoints.cpp and the per-game entrypoints_*.cpp files share: the
// table row, the opcode constants, the pattern search helpers and the
// finders that more than one game uses.

#ifndef entrypoints_internal_hpp
#define entrypoints_internal_hpp

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "memory.h"
#include "zterp.h"

// One row in the entrypoints table.
//   game:              which game this row applies to
//   title:             human-readable name (used for diagnostics)
//   pattern:           byte sequence to search for in story-file memory;
//                      0xf8 (WILDCARD) matches any byte
//   offset:            adjustment added to the match address (lets us point
//                      at a location partway through the matched pattern)
//   found_at_address:  populated by find_entrypoints(); 0 means not found
//   stub_original:     when true, overwrite the first matched byte with
//                      `rtrue` so the original Z-code immediately returns,
//                      leaving the C++ `fn` as the sole implementation
//   fn:                C++ replacement (or stub) invoked when PC reaches
//                      the found address
//   global_search:     when true, search the whole story (from static_start)
//                      instead of continuing from the previous match. Use only
//                      for entries whose pattern is globally unique but whose
//                      position relative to neighbouring routines varies across
//                      revisions, so the normal forward-only search would skip
//                      it (e.g. Zork Zero V-REFRESH, which precedes V-DEFINE in
//                      r366 but follows it in r393).
struct EntryPoint {
    Game game;
    std::string title;
    std::vector<uint8_t> pattern;
    int offset;
    uint32_t found_at_address;
    bool stub_original;
    void (*fn)();
    bool global_search = false;
    // Tells rows that hook the same routine through different patterns
    // apart where the finder needs to know which one matched. Only the
    // Zork Zero V-MAP-LOOP rows set it, see z0_find_map_globals().
    int variant = 0;
};

// Variants of the Zork Zero V-MAP-LOOP rows.
constexpr int kVMapLoopAlt = 1;    // r242: the loop is inlined into V-MAP, CALL_VN2 anchor
constexpr int kVMapLoopInline = 2; // r296/r66/r366: inlined too, CALL_VS2 anchor

// Sentinel value in a pattern that matches any byte. Chosen so it's
// unlikely to collide with a literal opcode at the same position.
constexpr uint8_t WILDCARD = 0xf8;

// Scans of the story stop this many bytes short of its end, so that code
// looking a few bytes past a match stays inside it.
constexpr int kScanTailMargin = 12;

// The instructions the finders look for, or patch in, on their own. (The
// patterns are byte dumps of game code and are left as numbers.)
constexpr uint8_t kOpRet = 0xab;       // RET <variable>, the variable number follows
constexpr uint8_t kOpRfalse = 0xb1;
constexpr uint8_t kOpNop = 0xb4;
constexpr uint8_t kOpRetPopped = 0xb8;
constexpr uint8_t kOpQuit = 0xba;

// An operand byte naming a variable counts the globals from 0x10 (below that
// are the stack and the locals). The interpreter's global accessors count
// them from 0.
constexpr uint8_t kFirstGlobal = 0x10;

inline uint8_t global_index(uint8_t variable_number) {
    return variable_number - kFirstGlobal;
}

// Hook functions that only name a row. Many rows exist only so that a
// finder can locate a routine or the globals next to it: the row's `fn` is
// never called, but the finder recognises the row by comparing with it. The
// ones used by a single game are static in that game's file; these two are
// shared.
void V_COLOR(void);
void DISPLAY_HINT(void);

// The tables, one per game, in the order find_entrypoints() walks them.
extern std::vector<EntryPoint> arthur_entrypoints;
extern std::vector<EntryPoint> journey_entrypoints;
extern std::vector<EntryPoint> shogun_entrypoints;
extern std::vector<EntryPoint> zorkzero_entrypoints;

// The globals finders, run once after the scan.
void find_arthur_globals(void);
void find_journey_globals(void);
void find_shogun_globals(void);
void find_zork0_globals(void);
void unhook_incomplete_fanucci(void);

// Address of the final instruction (rtrue/rfalse) inside V-COLOR, found by
// the finders, which then point the after_V_COLOR row at it.
extern uint32_t end_of_color_addr;

int32_t find_pattern_in_mem(const std::vector<uint8_t> &pattern, uint32_t startpos, uint32_t length_to_search);
int32_t find_globals_in_pattern(const std::vector<uint8_t> &pattern, const std::vector<uint8_t *> &vars, uint32_t startpos, uint32_t length_to_search);
int32_t find_routines_in_pattern(const std::vector<uint8_t> &pattern, const std::vector<uint32_t *> &routines, uint32_t startpos, uint32_t length_to_search);
int32_t find_values_in_pattern(const std::vector<uint8_t> &pattern, const std::vector<uint8_t *> &vals, uint32_t startpos, uint32_t length_to_search);
int32_t find_16_bit_values_in_pattern(const std::vector<uint8_t> &pattern, const std::vector<uint16_t *> &vals, uint32_t startpos, uint32_t length_to_search);
void patch_bytes(uint32_t addr, const std::vector<uint8_t> &bytes);
void patch_ret_local(uint32_t addr, uint8_t local);
int32_t find_v_color_globals(uint32_t v_color_addr);
int32_t find_hints_table(uint32_t do_hints_addr);
int32_t find_seen_hints_table(uint32_t start);
int32_t find_display_hint_globals(uint32_t display_hint_addr);
int32_t find_function_key_tables(uint32_t v_define_addr);

// Finds `pattern` and hands every WILDCARD in it to `capture`, as the
// address of the matched byte and a running count. With `pairs`, only a
// WILDCARD followed by another one counts: those are 16-bit operands. (Three
// in a row therefore count as two overlapping words, and the callers that
// have such a pattern pass the same slot twice so that the second, which is
// the one they want, wins.)
// Returns the address of the last WILDCARD captured (of the match, if there
// was none), for chaining successive searches forward, or -1.
template <typename Capture>
int32_t find_wildcards_in_pattern(const std::vector<uint8_t> &pattern, size_t slots, bool pairs, uint32_t startpos, uint32_t length_to_search, Capture capture) {
    int32_t offset = find_pattern_in_mem(pattern, startpos, length_to_search);
    if (offset == -1)
        return -1;
    size_t captured = 0;
    int32_t last_match_offset = offset;
    for (size_t i = 0; i + (pairs ? 1 : 0) < pattern.size(); i++) {
        if (pattern[i] != WILDCARD || (pairs && pattern[i + 1] != WILDCARD))
            continue;
        if (captured >= slots) {
            fprintf(stderr, "Pattern has more wildcards than output slots!\n");
            break;
        }
        capture(captured++, offset + i);
        last_match_offset = offset + i;
    }
    return last_match_offset;
}

#endif /* entrypoints_internal_hpp */
