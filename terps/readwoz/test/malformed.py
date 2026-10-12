#!/usr/bin/env python3
"""Feeds damaged Apple II disk images to ciderpress.c, built with the address
and undefined behaviour sanitizers: none may crash it or hang it.

    ./malformed.py [image ...]

The images to damage default to one DOS 3.3 disk from the scott corpus, one
ProDOS disk from the comprehend corpus and one .woz, whichever are present
(the corpora are not in the repository). Every damaged image goes through all
the operations of ciderpress_probe. What comes out is not checked, only that
the probe exits normally.
"""

import glob
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TERPS = os.path.dirname(os.path.dirname(HERE))
OPS = ["dos", "twice", "saga", "prodos", "full", "range", "v6"]
SECTOR = 256
TIMEOUT = 20


def sector(track, sect):
    return (track * 16 + sect) * SECTOR


def variants(data):
    """Yields (name, bytes) for each way of damaging a sector image."""
    for size in (0, 1, 255, 256, 4096, sector(17, 0) + 3, 70000, len(data) - 1):
        yield "truncated to %d" % size, data[:size]
    yield "extended", data + data[:5000]

    def patched(*patches):
        image = bytearray(data)
        for offset, values in patches:
            image[offset:offset + len(values)] = bytes(values)
        return bytes(image)

    vtoc = sector(17, 0)
    cat = sector(data[vtoc + 1] % 35, data[vtoc + 2] % 16)
    yield "catalog off the disk", patched((vtoc + 1, [0xfe, 0xfe]))
    yield "catalog is the VTOC", patched((vtoc + 1, [17, 0]))
    yield "catalog links to itself", patched((cat + 1, [data[vtoc + 1], data[vtoc + 2]]))
    yield "catalog links off the disk", patched((cat + 1, [0x40, 0x40]))

    entries = [cat + 0x0b + i * 0x23 for i in range(7)]
    yield "T/S lists off the disk", patched(*[(e, [0xfe, 0xfe]) for e in entries])
    yield "T/S lists are the VTOC", patched(*[(e, [17, 0]) for e in entries])
    yield "T/S lists are the catalog", patched(*[(e, [data[vtoc + 1], data[vtoc + 2]]) for e in entries])
    yield "files of no sectors", patched(*[(e + 0x21, [0, 0]) for e in entries])
    yield "files of 65535 sectors", patched(*[(e + 0x21, [0xff, 0xff]) for e in entries])
    yield "unterminated names", patched(*[(e + 3, [0xc1] * 30) for e in entries])

    # every sector that looks like a T/S list: link it to itself, then fill it
    lists = [sector(data[e] % 35, data[e + 1] % 16) for e in entries if data[e] not in (0, 0xff)]
    yield "T/S lists link to themselves", patched(*[(t + 1, [t // SECTOR // 16, t // SECTOR % 16]) for t in lists])
    yield "T/S lists full of 34,15", patched(*[(t + 0x0c, [34, 15] * 122) for t in lists])
    yield "T/S lists full of ff", patched(*[(t + 0x0c, [0xff] * 244) for t in lists])

    # ProDOS: block 2 is DOS sectors 11 and 10 of track 0
    vol = sector(0, 11)
    yield "volume name of 15", patched((vol + 4, [0xff] + [0x41] * 15))
    yield "entries of no length", patched((vol + 0x23, [0, 0]))
    yield "255 entries of 255", patched((vol + 0x23, [0xff, 0xff]))
    yield "directory links to itself", patched((vol + 2, [2, 0]))
    yield "directory links off the disk", patched((vol + 2, [0xff, 0xff]))

    rng = random.Random(1)
    image = bytearray(data)
    for block in range(len(data) // 512):
        for i in range(13):
            e = block * 512 + 4 + i * 0x27
            if image[e] >> 4 in (1, 2, 3) and image[e + 0x10] == 6:
                image[e + 0x11:e + 0x13] = rng.choice([b"\xff\xff", b"\x00\x00", b"\x18\x01", b"\x02\x00"])
                image[e + 0x15:e + 0x18] = rng.choice([b"\xff\xff\xff", b"\x00\x02\x00", b"\xf0\x49\x02"])
    yield "wild key blocks and lengths", bytes(image)

    for seed in range(8):
        rng = random.Random(seed)
        image = bytearray(data)
        for _ in range(len(data) // (20 << seed) + 1):
            image[rng.randrange(len(image))] = rng.randrange(256)
        yield "noise, seed %d" % seed, bytes(image)

    yield "all zero", bytes(len(data))
    yield "all ff", b"\xff" * len(data)


def default_images():
    found = []
    for pattern in ("scott/test/formats/games/dsk-adventureland*/*Side A.dsk",
                    "comprehend/test/**/*.dsk",
                    "scott/test/formats/games/woz-adventureland*/*Side A.woz",
                    "plus/test/**/*.woz"):
        matches = sorted(glob.glob(os.path.join(TERPS, pattern), recursive=True))
        if matches:
            found.append(matches[0])
    return found


def main():
    probe = os.path.join(tempfile.gettempdir(), "ciderpress_probe_san")
    build = ["cc", "-std=c11", "-O1", "-g", "-fsanitize=address,undefined",
             "-fno-sanitize-recover=undefined", "-I" + os.path.join(HERE, ".."),
             "-I" + os.path.join(TERPS, "common_utils"), "-o", probe,
             os.path.join(HERE, "ciderpress_probe.c"),
             os.path.join(HERE, "..", "ciderpress.c"), os.path.join(HERE, "..", "woz2nib.c")]
    if subprocess.run(build).returncode != 0:
        return 1

    images = sys.argv[1:] or default_images()
    if not images:
        print("malformed images: no image to damage (the corpora are not here), skipped")
        return 0

    runs = failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        for path in images:
            with open(path, "rb") as f:
                data = f.read()
            ext = os.path.splitext(path)[1]
            for name, damaged in variants(data):
                victim = os.path.join(tmp, "victim" + ext)
                with open(victim, "wb") as f:
                    f.write(damaged)
                for op in OPS:
                    runs += 1
                    try:
                        status = subprocess.run([probe, op, victim], stdout=subprocess.DEVNULL,
                                                stderr=subprocess.PIPE, timeout=TIMEOUT)
                        if status.returncode == 0:
                            continue
                        why = "exit %d\n%s" % (status.returncode, status.stderr.decode(errors="replace")[:1500])
                    except subprocess.TimeoutExpired:
                        why = "no answer in %d seconds" % TIMEOUT
                    failures += 1
                    print("FAIL    %s, %s, %s: %s" % (os.path.basename(path), name, op, why))

    print("malformed images: %d passed, %d failed" % (runs - failures, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
