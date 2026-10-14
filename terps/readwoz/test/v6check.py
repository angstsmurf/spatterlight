#!/usr/bin/env python3
"""Checks what ReadInfocomV6File() extracts from the Apple II disks of the
Infocom V6 games (Zork Zero, Shogun, Journey, Arthur) against v6_expected.tsv.
bocfel's z6/extract_apple_2.cpp is the only caller, and no other suite gets
there.

    ./v6check.py [--bless] [directory ...]

The disks are not in the repository. They are looked for under the given
directories, by default v6games/ next to this script (a link to wherever they
are will do), as .woz, .nib, .dsk, .do and .po files. An image is known by
the SHA-256 of its contents, not by its name, and a row whose image is not
there is skipped. An image under the directories that has no row is reported:
--bless gives it one if a game file comes out of it.

The columns are the SHA-256 of the image, what came out of it (game and disk
number, length, hash, as ciderpress_probe prints them), the name the image
had when the row was made, and a note that --bless keeps.

The probe is built with the address and undefined behaviour sanitizers.
"""

import hashlib
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TERPS = os.path.dirname(os.path.dirname(HERE))
EXPECTED = os.path.join(HERE, "v6_expected.tsv")
EXTENSIONS = (".woz", ".nib", ".dsk", ".do", ".po")
TIMEOUT = 20


def build(directory):
    probe = os.path.join(directory, "ciderpress_probe")
    subprocess.run(["cc", "-std=c11", "-O1", "-g", "-Wall",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                    "-I" + os.path.join(HERE, ".."), "-I" + os.path.join(TERPS, "common_utils"),
                    "-o", probe, os.path.join(HERE, "ciderpress_probe.c"),
                    os.path.join(HERE, "..", "ciderpress.c"), os.path.join(HERE, "..", "woz2nib.c")],
                   check=True)
    return probe


def extract(probe, image):
    """Returns "game<n>.disk<n> <length> <hash>", "-" if no game file came
    out, or a line starting with CRASH."""
    try:
        done = subprocess.run([probe, "v6", image], stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return "CRASH timeout"
    if done.returncode != 0:
        return "CRASH %d" % done.returncode
    fields = done.stdout.decode().strip().split("\t")
    if len(fields) != 4 or fields[0] != "v6":
        return "CRASH output"
    return "-" if fields[2] == "-" else " ".join(fields[1:])


def find_images(directories):
    """Returns {sha256: path} for every disk image under the directories."""
    found = {}
    for directory in directories:
        for root, _, names in os.walk(directory, followlinks=True):
            for name in sorted(names):
                if name.lower().endswith(EXTENSIONS):
                    path = os.path.join(root, name)
                    with open(path, "rb") as f:
                        found.setdefault(hashlib.sha256(f.read()).hexdigest(), path)
    return found


def read_expected():
    rows = []
    if os.path.exists(EXPECTED):
        with open(EXPECTED, encoding="utf-8") as f:
            for line in f:
                if line.strip() and not line.startswith("#"):
                    rows.append((line.rstrip("\n").split("\t") + ["", ""])[:4])
    return rows


def write_expected(rows):
    with open(EXPECTED, "w", encoding="utf-8") as f:
        f.write("# sha256 of the image\tgame file\tname of the image\tnote\n")
        for row in sorted(rows, key=lambda row: (row[1].split(" ")[0], row[2])):
            f.write("\t".join(row).rstrip("\t") + "\n")


def main():
    args = sys.argv[1:]
    bless = "--bless" in args
    directories = [a for a in args if a != "--bless"] or [os.path.join(HERE, "v6games")]

    rows = read_expected()
    images = find_images(directories)
    passed = failed = skipped = added = 0

    with tempfile.TemporaryDirectory() as tmp:
        probe = build(tmp)
        for row in rows:
            digest, want, name = row[0], row[1], row[2]
            if digest not in images:
                skipped += 1
                continue
            got = extract(probe, images.pop(digest))
            if got == want:
                passed += 1
            elif bless and not got.startswith("CRASH"):
                print("BLESSED %s: %s" % (name, got))
                row[1] = got
                added += 1
            else:
                print("FAIL    %s\n    expected %s\n    got      %s" % (name, want, got))
                failed += 1

        for digest, path in sorted(images.items(), key=lambda item: item[1]):
            got = extract(probe, path)
            if got.startswith("CRASH"):
                print("FAIL    %s (no row)\n    got      %s" % (path, got))
                failed += 1
            elif got != "-" and bless:
                print("BLESSED %s: %s" % (os.path.basename(path), got))
                rows.append([digest, got, os.path.basename(path), ""])
                added += 1
            elif got != "-":
                print("NO ROW  %s\n    got      %s" % (path, got))
                failed += 1

    if bless and added:
        write_expected(rows)
    print("V6 disks: %d passed, %d failed, %d skipped (image not found)%s"
          % (passed, failed, skipped, ", %d rows written" % added if bless else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
