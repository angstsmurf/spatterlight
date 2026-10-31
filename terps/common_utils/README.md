# common_utils

Small C helpers shared by the ScottFree family (scott, plus, taylor) and, for a
few of the files, by other interpreters. Public names mix CamelCase
(`MemAlloc`), snake_case (`erkyrath_random`) and `rgbsurf_*`. That is historical;
don't rename them.

## Modules, from the bottom layer up

| File | Provides | Needs | Used by |
|---|---|---|---|
| `minmax.h` | `MIN`, `MAX` | nothing | scott, plus, taylor, archetype, comprehend, apple2draw |
| `byteorder.h` | little- and big-endian reads, plus advancing variants | nothing | scott, plus, taylor, bocfel z6, readwoz |
| `debugprint.h` | `debug_print`; always compiled, so its arguments keep building | nothing | scott, plus, taylor, bocfel z6, readwoz |
| `glk_rocks.h` | the three window rocks | nothing | through `common_defines.h`; comprehend |
| `fatal.h` | prototype of `Fatal` | nothing | `memory_allocation.c`, `common_utils.h` |
| `randomness.[ch]` | xoshiro128** `erkyrath_random`, saveable state | libc | most interpreters |
| `rgbsurface.h` | header-only RGB drawing, sent to a Glk window | Glk, by way of the includer | scarier |
| `memory_allocation.[ch]` | checked allocators | `Fatal` | scott, plus, taylor, apple2draw |
| `common_file_utils.[ch]` | whole-file reads, file length | allocators | scott, plus, taylor |
| `companion_search.[ch]` | finds the other disk or side of a game | file utils, allocators | plus, scott saga |
| `common_defines.h` | rocks, `MAX_LENGTH` and similar | `glk_rocks.h` | scott, plus, taylor |
| `common_utils.[ch]` | `Fatal`, window lookup, `SetFixedWindowHeight` | Glk and the interpreter (below) | scott, plus, taylor |
| `layouttext.[ch]` | `LineBreakText`, `ShowTextInTopWindow` | `common_utils`, allocators | scott, plus, taylor |

`Fatal` is the one upward dependency. Whoever links `common_utils.c` must define
`Bottom`, `Display()` and `CleanupAndExit()`. A program that only wants the
allocators can link `memory_allocation.c` and define its own `Fatal` against
`fatal.h`, with no Glk involved.

## Vendored copies

`terps/scarier/common_utils/` holds byte-identical copies of `randomness.[ch]`
and `rgbsurface.h`. After changing one of them here, copy it there and run
`make -f Makefile.headless vendorcheck` in `terps/scarier`. Glulxe deliberately
keeps its own upstream RNG, and `randomness.c` is kept close to that upstream
(hence `TRUE`/`FALSE`).

The Gargoyle fork also carries these files, so a sync has to include `byteorder.h`,
`fatal.h`, `glk_rocks.h` and `randomness.[ch]`.

A new header needs a file reference and a group entry in
`Spatterlight.xcodeproj/project.pbxproj`.

## Tests

    cd test && make check      # unit tests under ASan and UBSan, with a fake Glk
    cd test && make warnings   # -Wall -Wextra -Wconversion -Wundef -Wshadow

The only `make warnings` output is `-Wsign-conversion` in `rgbsurface.h` (which is
left alone, see below) and two lines of the test itself.

## Decided against

- **`exo_util.h` using `byteorder.h`:** unp64 is C++, where its plain `inline` is
  fine, and it would tie the self-contained unp64 test and fork builds to this
  directory.
- **`ReadLE` in decompressz80, `GetWord` in ti994a:** local and working; nothing
  is gained by moving them.
- **Merging Glulxe's RNG:** it tracks upstream.
- **Symlinks instead of vendored copies:** the copies and `vendorcheck` work, and
  symlinks are fragile in Xcode and in fork builds.
- **One shared `Display()` / `CleanupAndExit()`:** the variants really differ.
  Scott has Unicode handling and `lastwasnewline`, Taylor has `PrintToStreamV`.
- **`FindOrOpenGlkWindow`:** the snippets differ in sizes, rocks and fallbacks.
- **Shared test stubs:** each suite only repeats a few one-liners.
- **`MemCallocN`, `MemStrdup`:** nothing but tests would call them.
- **Size cap in the file reader, `CompanionFallback` struct:** no caller needs them.
- **Parameter for `LineBreakText`'s leading-`.` handling:** documented instead,
  since all three callers want it.
- **`BE32` and const byteorder variants:** there are no open-coded users to replace.
- **Splitting or restyling `rgbsurface.h`:** it is vendored and works, so it only
  got tests.
- **Prefixing `MAX_WORDS` and friends:** high churn. The names are live, used by
  scott, plus and taylor.
- **Include-what-you-use in the umbrella headers, shared `sources.mk`, one include
  convention:** high churn for no behaviour change. scarier's prefixed include is
  what keeps its copy apart.
- **Reformatting and uniform header guards:** cosmetic churn in a vendored file.
- **Taylor's `LookForSecondTOTGame` using `FindCompanionFile`:** it uses a different
  naming scheme.
