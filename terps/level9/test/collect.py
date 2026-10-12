#!/usr/bin/env python3
"""Add game images to the corpus of the Level 9 loader test.

  collect.py DIR...

Every file under the folders that level9_hl can load, and that is not in the
manifest yet (by SHA-256), is copied to games/<id>/ and gets a row in
games.manifest.tsv and one in scripts.tsv. The hashes of the new rows are
filled in by

  ../../common_imagetest/run_script_tests.py . . ../level9_hl --bless

../level9_hl must be built. See README.md for the files that need another one
beside them.
"""

import hashlib
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HL = os.path.join(HERE, "..", "level9_hl")
MANIFEST = os.path.join(HERE, "games.manifest.tsv")
SCRIPTS = os.path.join(HERE, "scripts.tsv")
GAMES = os.path.join(HERE, "games")
SCRIPT = "probe"
EXTENSIONS = {".z80", ".sna", ".tzx", ".tap", ".dsk", ".dat", ".l9"}
ERROR = b"Glk Level 9 Error"
BANNER = re.compile(rb"\[ ([^\]\n]*?)\s*\]")


def rows(table):
    with open(table, encoding="utf-8") as f:
        return [l.rstrip("\n").split("\t") for l in f
                if l.strip() and not l.startswith("#")]


def detect(path):
    """The game as the interpreter names it, "unnamed" if it has no name for
    it, None if the file does not load."""
    try:
        out = subprocess.run([HL, path], input=b"look\n", stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL, timeout=30).stdout
    except subprocess.TimeoutExpired:
        return None
    if ERROR in out or b"\n" not in out.strip():
        return None
    banner = BANNER.search(out)
    return banner.group(1).decode("latin-1") if banner else "unnamed"


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    if not os.access(HL, os.X_OK):
        sys.exit(f"{HL} is not built")
    manifest = rows(MANIFEST)
    known = {r[3] for r in manifest}
    ids = {r[0] for r in manifest}
    added = 0
    for root in sys.argv[1:]:
        for folder, _, names in sorted(os.walk(root)):
            for name in sorted(names):
                ext = os.path.splitext(name)[1].lower()
                if ext not in EXTENSIONS:
                    continue
                path = os.path.join(folder, name)
                with open(path, "rb") as f:
                    digest = hashlib.sha256(f.read()).hexdigest()
                if digest in known:
                    continue
                known.add(digest)
                game = detect(path)
                if game is None:
                    continue
                stem = re.sub(r"[^a-z0-9]+", "-", os.path.splitext(name)[0].lower()).strip("-")
                id = base = f"{ext[1:]}-{stem}"
                n = 2
                while id in ids:
                    id = f"{base}-{n}"
                    n += 1
                ids.add(id)
                os.makedirs(os.path.join(GAMES, id), exist_ok=True)
                shutil.copyfile(path, os.path.join(GAMES, id, name))
                with open(MANIFEST, "a", encoding="utf-8") as f:
                    f.write("\t".join([id, id, name, digest, "", game]) + "\n")
                with open(SCRIPTS, "a", encoding="utf-8") as f:
                    f.write("\t".join([SCRIPT, id, "0", "-"]) + "\n")
                added += 1
                print(f"added {id}: {game}")
    print(f"{added} added")


if __name__ == "__main__":
    main()
