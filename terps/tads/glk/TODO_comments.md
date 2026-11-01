# Comment hygiene TODO (terps/tads/glk)

The pass from the survey of 2026-11-01 is applied. A tokenising comparison against HEAD shows the
sources unchanged apart from comments. This item is still open:

- **Licence of `osunixt.h`.** It has no notice. It is an empty placeholder from the Gargoyle import of
  2017, with nothing in it to license.

Deliberately left as they are:

- Game names in short parentheses.
- `osfrobtads.h`'s `#if 0` blocks.
- Upstream `TODO`, `XXX` and `//` comments in `osportable.cc` and `osparse.c`.
- Narrating comments in `t23run.cpp`.
- The 70-line `oshtml.c` header. Its entries were corrected rather than cut, so it still overlaps
  `README.md`.
