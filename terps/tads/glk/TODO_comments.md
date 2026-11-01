# Comment hygiene TODO (terps/tads/glk)

The pass from the survey of 2026-11-01 is applied. A tokenising comparison against HEAD shows the
sources unchanged apart from comments. These items are still open:

- **Licence of `osspatter.h` and `osunixt.h`.** Both have no notice. They came in with the
  Gargoyle import of 2017 and look derived from FrobTADS, so the holder isn't established.
- **Gargoyle fork.** `osglk.c` and `osmisc.c` lost about 550 lines of comments copied from
  `tads2/osifc.h`, and 15 files gained a licence block. Replay this as one commit at the next sync.

Deliberately left as they are:

- Game names in short parentheses.
- `osfrobtads.h`'s `#if 0` blocks.
- Upstream `TODO`, `XXX` and `//` comments in `osportable.cc` and `osparse.c`.
- Narrating comments in `t23run.cpp`.
- The 70-line `oshtml.c` header. Its entries were corrected rather than cut, so it still overlaps
  `README.md`.
