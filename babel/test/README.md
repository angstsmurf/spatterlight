# babel tests

```sh
cd babel
make && make test                    # one directory per format, output vs expected
test/corpus_ifids.sh -b OLD DIR... > before.tsv
test/corpus_ifids.sh DIR... > after.tsv && diff before.tsv after.tsv
```

`make test` covers adrift, alan, hugo, twine, Blorb (bronze, sensory), quest,
the `UUID://…//` marker in Z-code and Glulx (marker) and scott/sagaplus
(saga). `saga` reads game files from `terps/scott/test` and
`terps/plus/test/synthetic`, so it needs the whole repository. `html` is left
out of `all`: the html module is not in `modules.h`, so the app never uses it
(it would claim every unclaimed `.html`).

## Corpus IFID regression

IFIDs are stored in the Spatterlight library and in iFiction files, so a
change to a module must not change the IFID of a file it already identified.
`corpus_ifids.sh` prints `sha256, extension, format, IFIDs, hash of -meta`
for every file below the given directories with an extension some module
claims. Keep a copy of the old `babel` binary, list a corpus with each and
diff. A useful corpus is `terps/*/test` plus wherever your games are; the
lists are keyed on content, so they can be diffed across machines.

For a sanitizer run build in a copy of the folder:

```sh
make COMPILER="cc -g -O1 -Wall -Wextra -fsanitize=address,undefined"
```

## Shared helpers

`babel_util.h` holds what the modules written for Spatterlight share: the
16-bit additive checksum of the length + checksum tables (`babel_sum16`), a
bounded byte search (`babel_memfind`) and `babel_copy_ifid`, which copies a
table IFID in capitals. The upstream modules (zcode, glulx, tads, hugo,
level9, adrift, alan, agt, advsys, magscrolls, executable, twine, html,
blorb, babel_handler …) keep their own copies so that they stay close to the
Treaty of Babel distribution; change them only for a real bug.

## Behaviour worth knowing

- `modules.h` order is the claim order (see the comment there): the first
  module that lists the file's extension and claims it wins, then the first
  to claim it at all, then a weak (`NO_REPLY_RV`) claim. `executable` claims
  MZ/ELF/Mach-O… files outright; the Spatterlight modules come after it, and
  moving it would change which module names some files.
- A SAGA Plus text database is known by its title string alone (the
  structural check that used to run first accepted a subset of the same
  titles and was removed). Before 2026-10-19 a database with a known title
  that failed that check (any `X-MEN` database other than the 0xc480-byte
  original, e.g. the plus synthetic games) was claimed but given no IFID, and
  Spatterlight refused to import it.
- taylor claims only files in its table, comprehend only the disk images and
  DOS executables in its tables; scott also claims any ScottFree text
  database, TI-99/4A image or Atari disk and falls back to the MD5.
- An unclaimed file is reported as `executable (non-authoritative)` with its
  MD5 as IFID: `executable` answers `NO_REPLY_RV` for anything it does not
  recognise. Spatterlight treats a non-authoritative format as unknown.

## Looked at and left alone (review of 2026-10-19)

- Upstream-shaped code: the twin `find_text_in_file` in twine.c/html.c and
  adrift.c, the big-endian readers and image-size code in blorb.c, glulx.c,
  tads.c, babel_story_functions.c, `tads2.c`/`tads3.c`, md5.c, treaty*.h.
- `deeper_babel_init`'s two copies of the two-pass search (containers and
  formats): upstream code with no failing case.
- The table layout of scott/sagaplus/taylor (`ifids[]` + enum + registry):
  works, and recent commits add rows without trouble. A few GUIDs appear in
  two modules (Claymorgue, Spider-Man and Buckaroo Banzai in scott and
  sagaplus; Questprobe 3 in taylor), all identical and upper-cased on output.
- Splitting quest5.c (zip reader, inflate, metadata) into several files.
- `babel_rev7.txt`, `extras/`, `babel-get/` and the Borland makefile branch
  are upstream and kept.
