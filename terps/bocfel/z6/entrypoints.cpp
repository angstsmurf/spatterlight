//
//  entrypoints.cpp
//  bocfel6
//
//  Created by Administrator on 2023-08-06.
//
// Entrypoint system for the four Infocom Z-machine v6 games (Arthur,
// Journey, Shogun, Zork Zero).
//
// Background: these games' v6 binaries do graphics, status lines, mouse
// handling, etc. by calling Z-machine routines that assume the original
// Infocom interpreter's quirks. Modern Glk-based interpreters can't run
// those routines verbatim, so for each game we maintain a table of
// patterns that locate specific Z-machine routines (and certain globals
// next to them) inside the story file. When the Z-machine's program
// counter hits one of those addresses, we hijack it: either run a C++
// replacement function (`fn`) and/or patch the bytes in place so that
// execution continues with new behavior.
//
// The tables and the finders of each game are in entrypoints_<game>.cpp;
// this file has the scan, the search helpers they share and the lookup.
//
// Flow:
//   1. At startup, find_entrypoints() scans the story file for every
//      entrypoint pattern that matches the loaded game. Found addresses
//      are stored back into the EntryPoint struct.
//   2. Per-game post-processing (find_arthur_globals / find_journey_globals
//      / find_shogun_globals / find_zork0_globals) then uses those
//      addresses as anchors to locate nearby globals, routines, tables,
//      objects, properties, and to apply byte patches.
//   3. During execution, check_entrypoints() is called on every
//      instruction fetch; if PC matches a found entrypoint, the C++
//      replacement function is invoked.
//
// Entries with `stub_original = true` have their first byte overwritten
// with `rtrue` (0xb0), so the original Z-code at that address returns
// immediately and the C++ replacement is the sole implementation.
#include <algorithm>
#include <unordered_map>
#include <vector>

#include "zterp.h"
#include "screen.h"
#include "memory.h"
#include "dict.h"

#include "arthur.hpp"
#include "journey.hpp"
#include "shogun.hpp"
#include "zorkzero.hpp"

#include "v6_shared.hpp"

#include "entrypoints.hpp"
#include "entrypoints_internal.hpp"

// Global indices (in the Z-machine global variable table) of the
// foreground and background color globals. Populated by the per-game
// globals finders.
uint8_t fg_global_idx = 0, bg_global_idx = 0;

// The two hook functions that only name a row and are shared by several
// games' tables (see entrypoints_internal.hpp).
void V_COLOR(void) {}
void DISPLAY_HINT(void) {}

#pragma mark - Pattern Search Helpers

// Scans `length_to_search` bytes of story memory starting at `startpos`
// for the given pattern. WILDCARD bytes in the pattern match anything.
// Returns the absolute address of the first match, or -1 if not found.
int32_t find_pattern_in_mem(const std::vector<uint8_t> &pattern, uint32_t startpos, uint32_t length_to_search) {
    // Chained searches sometimes feed the -1 of a previous failed search
    // back in as startpos, which wraps to a huge unsigned value. Bail out
    // rather than reading out of bounds, and clamp the search window so we
    // never index past the end of story memory.
    if (startpos >= memory_size || pattern.empty() || pattern.size() > memory_size) {
        return -1;
    }
    uint32_t window_end = startpos + length_to_search;
    if (window_end > memory_size || window_end < startpos) { // also catches overflow
        window_end = memory_size;
    }
    int32_t end = static_cast<int32_t>(window_end) - static_cast<int32_t>(pattern.size());
    // <= so that a match ending exactly at the end of the window counts
    for (int32_t i = startpos; i <= end; i++) {
        bool found = true;
        for (size_t j = 0; j < pattern.size(); j++) {
            if (pattern[j] != WILDCARD && pattern[j] != memory[i + j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return i;
        }
    }
    return -1;
}

// Each WILDCARD is an operand naming a global, whose index is written into
// the next slot of `vars`.
int32_t find_globals_in_pattern(const std::vector<uint8_t> &pattern, const std::vector<uint8_t *> &vars, uint32_t startpos, uint32_t length_to_search) {
    return find_wildcards_in_pattern(pattern, vars.size(), false, startpos, length_to_search, [&](size_t slot, uint32_t addr) {
        *vars[slot] = global_index(memory[addr]);
    });
}

// Each pair of adjacent WILDCARDs is a packed routine address, stored
// unpacked (as a byte address).
int32_t find_routines_in_pattern(const std::vector<uint8_t> &pattern, const std::vector<uint32_t *> &routines, uint32_t startpos, uint32_t length_to_search) {
    return find_wildcards_in_pattern(pattern, routines.size(), true, startpos, length_to_search, [&](size_t slot, uint32_t addr) {
        *routines[slot] = unpack_routine(word(addr));
    });
}

// Each WILDCARD is captured as it is. Use this for raw byte operands such
// as object numbers, property numbers, or short constants.
int32_t find_values_in_pattern(const std::vector<uint8_t> &pattern, const std::vector<uint8_t *> &vals, uint32_t startpos, uint32_t length_to_search) {
    return find_wildcards_in_pattern(pattern, vals.size(), false, startpos, length_to_search, [&](size_t slot, uint32_t addr) {
        *vals[slot] = byte(addr);
    });
}

// Each pair of adjacent WILDCARDs is captured as a 16-bit word. Use this
// for byte-addressed tables (object tables, picture tables, etc.) where no
// unpacking is needed.
int32_t find_16_bit_values_in_pattern(const std::vector<uint8_t> &pattern, const std::vector<uint16_t *> &vals, uint32_t startpos, uint32_t length_to_search) {
    return find_wildcards_in_pattern(pattern, vals.size(), true, startpos, length_to_search, [&](size_t slot, uint32_t addr) {
        *vals[slot] = word(addr);
    });
}

// Overwrites game code at `addr`.
void patch_bytes(uint32_t addr, const std::vector<uint8_t> &bytes) {
    for (size_t i = 0; i < bytes.size(); i++)
        store_byte(addr + i, bytes[i]);
}

// Makes the game return the value of one of the routine's locals at `addr`.
// At the start of a routine this is how a replaced routine hands back the
// result that the replacement stored in that local.
void patch_ret_local(uint32_t addr, uint8_t local) {
    patch_bytes(addr, {kOpRet, local});
}

uint32_t end_of_color_addr = 0;

// Shogun and Zork Zero share the same V-COLOR shape: locate the fg/bg
// color globals and the routine's final return instruction (stored in
// end_of_color_addr). Returns the offset of the last global match, or -1.
int32_t find_v_color_globals(uint32_t v_color_addr) {
    int32_t start = find_globals_in_pattern({ 0x2d, 0x02, WILDCARD, 0x2d, 0x03, WILDCARD }, { &fg_global_idx, &bg_global_idx }, v_color_addr, 300);
    if (start == -1) {
        // Fallback for early releases (<= r288) whose V-COLOR uses the
        // 0x0d store idiom instead of 0x2d. The { &bg, &fg } order is
        // correct and verified against real binaries: r278/r283 reach
        // this path and yield fg/bg globals consistent with the primary
        // 0x2d path of the adjacent later release r288 (fg 0x1c in both).
        start = find_globals_in_pattern({ 0x0d, WILDCARD, 0x02, 0x0d, WILDCARD, 0x09 }, { &bg_global_idx, &fg_global_idx }, v_color_addr, 300);
    }

    if (start == -1) {
        fprintf(stderr, "Error! Could not find color globals!\n");
        return -1;
    }

    int32_t found = find_pattern_in_mem({ kOpRetPopped }, v_color_addr, 300);
    if (found == -1)
        found = find_pattern_in_mem({ kOpRtrue }, start, 200);
    if (found != -1)
        end_of_color_addr = found;
    else
        fprintf(stderr, "Could not find return from routine V_COLOR!\n");

    return start;
}

// Hint and function-key tables are located the same way in several games.

// DO-HINTS loads the hint table address (Arthur, Shogun, Zork Zero).
// Returns the match offset, or -1.
int32_t find_hints_table(uint32_t do_hints_addr) {
    int32_t start = find_16_bit_values_in_pattern({ 0xf3, 0x3f, 0xff, 0xfd, 0xcd, 0x4f, WILDCARD, WILDCARD, WILDCARD, 0xcf, 0x1f, WILDCARD, WILDCARD }, { &hints_table_addr, &hints_table_addr, &hints_table_addr }, do_hints_addr, 300);
    if (start == -1)
        fprintf(stderr, "Could not find hints_table_addr\n");
    return start;
}

// DISPLAY-HINT reads the table of how many hints of each question have been
// seen (Arthur, Shogun, Zork Zero). Returns the match offset, or -1.
int32_t find_seen_hints_table(uint32_t start) {
    start = find_16_bit_values_in_pattern({ 0x01, 0x00, 0xcf, 0x2f, WILDCARD, WILDCARD, 0x00, 0x04 }, { &seen_hints_table_addr }, start, 300);
    if (start == -1)
        fprintf(stderr, "seen_hints_table_addr not found!\n");
    return start;
}

// DISPLAY-HINT references the current question and chapter globals and
// the seen-hints table (Shogun, Zork Zero). Returns the last match, or -1.
int32_t find_display_hint_globals(uint32_t display_hint_addr) {
    int32_t start = find_globals_in_pattern({ 0x0b, 0x54, WILDCARD, 0x01, 0x00 }, { &hint_quest_global_idx }, display_hint_addr, 300);
    if (start == -1) {
        fprintf(stderr, "Error! Could not find hint_quest_global_idx!\n");
        return -1;
    }
    start = find_globals_in_pattern({ 0x01, 0x55, WILDCARD, 0x01, 0x00 }, { &hint_chapter_global_idx }, start, 200);
    if (start == -1) {
        fprintf(stderr, "Error! Could not find hint_chapter_global_idx!\n");
        return -1;
    }
    return find_seen_hints_table(start);
}

// V-DEFINE references the function key and function key name tables
// (Shogun, Zork Zero). Returns the last match, or -1.
int32_t find_function_key_tables(uint32_t v_define_addr) {
    int32_t start = find_16_bit_values_in_pattern({ 0xd4, 0x2f, WILDCARD, WILDCARD, 0x00, 0x06 }, { &fkeys_table_addr }, v_define_addr, 300);
    if (start == -1) {
        fprintf(stderr, "Error! Could not find fkeys_table_addr!\n");
        return -1;
    }
    return find_16_bit_values_in_pattern({ 0xf7, 0x8b, 0x0b, WILDCARD, WILDCARD, 0x00, 0x08, 0x66 }, { &fnames_table_addr }, start, 1100);
}

#pragma mark - Entrypoint Lookup

// The table of the game being played. Each game has its own, in
// entrypoints_<game>.cpp. find_entrypoints() walks it in order, advancing the
// search start address after each match, which means rows must stay in
// roughly-ascending order of where they appear in the story file.
static std::vector<EntryPoint> &current_entrypoints(void) {
    static std::vector<EntryPoint> none;
    if (is_spatterlight_arthur)
        return arthur_entrypoints;
    if (is_spatterlight_journey)
        return journey_entrypoints;
    if (is_spatterlight_shogun)
        return shogun_entrypoints;
    if (is_spatterlight_zork0)
        return zorkzero_entrypoints;
    return none;
}

// PC -> EntryPoint lookup, built by find_entrypoints() and consulted by
// check_entrypoints() on every instruction fetch.
struct EntryPointIndex {
    std::unordered_map<uint32_t, EntryPoint *> by_address;

    // The lowest and highest hooked address: a cheap range check that
    // skips everything else for a PC that can't possibly match.
    uint32_t lowest = UINT32_MAX;
    uint32_t highest = 0;

    // One bit per address in [lowest, highest], set where an entrypoint
    // lives. The range covers most of the code area, so without this
    // nearly every instruction would cost a hash lookup.
    std::vector<bool> hooked;

    // When two table rows were found at the same address, the later row is
    // the one that is hooked.
    void rebuild(std::vector<EntryPoint> &rows) {
        by_address.clear();
        hooked.clear();
        lowest = UINT32_MAX;
        highest = 0;
        for (auto &row : rows) {
            if (row.found_at_address != 0) {
                by_address[row.found_at_address] = &row;
                lowest = std::min(lowest, row.found_at_address);
                highest = std::max(highest, row.found_at_address);
            }
        }
        if (!by_address.empty()) {
            hooked.resize(highest - lowest + 1);
            for (const auto &pair : by_address)
                hooked[pair.first - lowest] = true;
        }
    }

    EntryPoint *lookup(uint32_t pc) const {
        if (pc > highest || pc < lowest || !hooked[pc - lowest])
            return nullptr;
        auto found = by_address.find(pc);
        return found != by_address.end() ? found->second : nullptr;
    }
};

static EntryPointIndex entrypoint_index;

#ifdef HEADLESS_V6
// Regression aid for the headless build: with BOCFEL_EP_DUMP set, print
// everything find_entrypoints() decided -- the hooked addresses, the raw
// contents of the per-game aggregates the finders fill in, and a checksum
// of the (patched) story -- so that a change to the pattern table or the
// finders can be compared across every story revision.
template <typename T> static void dump_aggregate(const char *name, const T &aggregate) {
    const unsigned char *bytes = reinterpret_cast<const unsigned char *>(&aggregate);
    fprintf(stderr, "EPDUMP %s", name);
    for (size_t i = 0; i < sizeof aggregate; i++)
        fprintf(stderr, "%s%02x", i % 4 == 0 ? " " : "", bytes[i]);
    fprintf(stderr, "\n");
}

static void dump_entrypoint_state(void) {
    if (getenv("BOCFEL_EP_DUMP") == nullptr)
        return;
    std::vector<const EntryPoint *> found;
    for (const auto &entrypoint : current_entrypoints())
        if (entrypoint.found_at_address != 0)
            found.push_back(&entrypoint);
    std::stable_sort(found.begin(), found.end(), [](const EntryPoint *a, const EntryPoint *b) {
        return a->found_at_address < b->found_at_address;
    });
    for (const EntryPoint *entrypoint : found)
        fprintf(stderr, "EPDUMP entry 0x%05x %s \"%s\"%s\n", entrypoint->found_at_address,
                entrypoint_index.by_address[entrypoint->found_at_address] == entrypoint ? "hooked" : "shadowed",
                entrypoint->title.c_str(), entrypoint->stub_original ? " stub" : "");
    dump_aggregate("ag", ag); dump_aggregate("ar", ar); dump_aggregate("at", at);
    dump_aggregate("jg", jg); dump_aggregate("jr", jr); dump_aggregate("jo", jo); dump_aggregate("ja", ja);
    dump_aggregate("sg", sg); dump_aggregate("sr", sr); dump_aggregate("st", st); dump_aggregate("sm", sm); dump_aggregate("so", so);
    dump_aggregate("zg", zg); dump_aggregate("zr", zr); dump_aggregate("zt", zt); dump_aggregate("zo", zo); dump_aggregate("zp", zp);
    fprintf(stderr, "EPDUMP scalars fg %u bg %u hint_chapter %u hint_quest %u hints_table 0x%x seen_hints 0x%x fkeys 0x%x fnames 0x%x\n",
            fg_global_idx, bg_global_idx, hint_chapter_global_idx, hint_quest_global_idx,
            hints_table_addr, seen_hints_table_addr, fkeys_table_addr, fnames_table_addr);
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < memory_size; i++)
        hash = (hash ^ memory[i]) * 16777619u;
    fprintf(stderr, "EPDUMP memory %08x\n", hash);
}
#endif

// One-time entrypoint discovery, called shortly after the story file
// has been loaded into memory. Walks the game's table in order, locates each row's pattern in dynamic memory, stubs the
// originals where requested, then runs the per-game globals finder and
// finally populates the lookup map consulted on every instruction.
void find_entrypoints(void) {

    int start = header.static_start;
    int end = memory_size - kScanTailMargin;

    // A routine with several rows (the "alt" and "older" patterns for the
    // earlier revisions) is real in exactly one place in any given story, so
    // once a row for a hook function has matched, the remaining rows for the
    // same function are skipped. Where two patterns can both match the same
    // story the row for the real routine comes first in the table: Zork Zero's
    // 2-locals UPDATE-STATUS-LINE pattern also hits a mid-routine false match
    // in r296, and the inline-CALL_VS2 V-MAP-LOOP call exists verbatim inside
    // the releases' standalone routine.
    std::vector<void (*)()> claimed;

    std::vector<EntryPoint> &entrypoints = current_entrypoints();
    for (auto &entrypoint : entrypoints) {
        if (is_game(entrypoint.game)) {
            if (std::find(claimed.begin(), claimed.end(), entrypoint.fn) != claimed.end()) {
                continue;
            }
            if (entrypoint.pattern.size()) {
                // Most entries search forward from the previous match (the
                // table is kept in roughly-ascending file order). A
                // global_search entry instead scans the whole story, for
                // routines whose order relative to their neighbours isn't
                // stable across revisions; its match must not rewind the
                // cursor, so the surrounding forward search is unaffected.
                int search_start = entrypoint.global_search ? header.static_start : start;
                int32_t offset = find_pattern_in_mem(entrypoint.pattern, search_start, end - search_start);
                // The 3-locals UPDATE-STATUS-LINE pattern (03 eb 7f 01 .. 04 7f
                // 04 00 = [3 locals] SET_WINDOW #1; SET_FONT #4) can false-match
                // the same SCREEN/FONT sequence buried inside INIT-STATUS-LINE in
                // the pre-release revisions, where the real UPDATE-STATUS-LINE is
                // a *2-locals* routine matched by the "alt" pattern instead (r296:
                // false hit at 0x1cd06, mid INIT-STATUS-LINE 0x1cca0; the real
                // routine is 0x1cdc4). A genuine routine head continues with
                // LOADB/JZ (10/a0); the interior copy continues with a CALL_2N to
                // PICINF-PLUS-ONE (da .. .. 01 7e). Skip such matches so the
                // routine stays unclaimed and the 2-locals alt can claim the real
                // one. (In the releases/r343/r366/r66 the real 3-locals routine
                // matches first, so this never rejects a wanted match.)
                while (offset != -1 && entrypoint.fn == z0_UPDATE_STATUS_LINE &&
                       entrypoint.pattern.size() >= 9 && entrypoint.pattern[0] == 0x03 &&
                       (uint32_t)(offset + 9) < memory_size && memory[offset + 9] == 0xda) {
                    offset = find_pattern_in_mem(entrypoint.pattern, offset + 1, end - (offset + 1));
                }
                if (offset != -1) {
                    entrypoint.found_at_address = offset + entrypoint.offset;
                    if (!entrypoint.global_search || entrypoint.found_at_address > (uint32_t)start) {
                        start = entrypoint.found_at_address;
                    }
                    fprintf(stderr, "Found routine %s at offset 0x%04x\n", entrypoint.title.c_str(), entrypoint.found_at_address);
                    claimed.push_back(entrypoint.fn);
                    if (entrypoint.stub_original) {
                        // Overwrite original byte with rtrue;
                        store_byte(entrypoint.found_at_address, kOpRtrue);
                    }
                }
            }
        }
    }

    if (is_spatterlight_arthur) {
        find_arthur_globals();
    } else if (is_spatterlight_journey) {
        find_journey_globals();
    } else if (is_spatterlight_shogun) {
        find_shogun_globals();
    } else if (is_spatterlight_zork0) {
        find_zork0_globals();
        unhook_incomplete_fanucci();
    }

    entrypoint_index.rebuild(entrypoints);

#ifdef HEADLESS_V6
    dump_entrypoint_state();
#endif
}

// Hot-path dispatcher called from the Z-machine interpreter on each
// instruction fetch. If the current PC matches a registered entrypoint,
// invokes the corresponding C++ replacement. The range check and bitmap
// keep the overhead negligible when no entrypoint matches; the hash
// lookup only runs on an actual hit.
void check_entrypoints(uint32_t pc) {
    EntryPoint *entrypoint = entrypoint_index.lookup(pc);
    if (entrypoint != nullptr)
        (entrypoint->fn)();
}
