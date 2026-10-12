#!/usr/bin/env python3
"""Feeds damaged Apple II disk images to ciderpress.c, built with the address
and undefined behaviour sanitizers: none may crash it or hang it.

    ./malformed.py [image ...]

The images to damage default to one DOS 3.3 disk from the scott corpus, one
ProDOS disk from the comprehend corpus and one .woz, whichever are present
(the corpora are not in the repository). Every damaged image goes through all
the operations of ciderpress_probe. What comes out is not checked, only that
the probe exits normally.

A .woz is damaged in two ways: as any image is, which the checksum of the file
catches, and inside its chunks with the checksum made right again, which gets
the damage as far as woz2nib's parsing and decoding.
"""

import glob
import os
import random
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TERPS = os.path.dirname(os.path.dirname(HERE))
OPS = ["dos", "twice", "saga", "prodos", "full", "range", "v6", "nib"]
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


def woz_variants(data):
    """Yields (name, bytes) for each way of damaging a WOZ behind its checksum."""
    def sealed(body, crc=None):
        if crc is None:
            crc = zlib.crc32(body)
        return data[:8] + crc.to_bytes(4, "little") + bytes(body)

    chunks = {}
    pos = 12
    while pos + 8 <= len(data):
        size = int.from_bytes(data[pos + 4:pos + 8], "little")
        chunks[data[pos:pos + 4]] = (pos + 8, size)
        pos += 8 + size
    if not all(name in chunks for name in (b"INFO", b"TMAP", b"TRKS")):
        return
    info, tmap, (trks, trks_size) = chunks[b"INFO"][0], chunks[b"TMAP"][0], chunks[b"TRKS"]

    def patched(*patches, crc=None):
        image = bytearray(data)
        for offset, values in patches:
            image[offset:offset + len(values)] = bytes(values)
        return sealed(image[12:], crc)

    for size in (4, 5, 8, 11, 12, 13, 19, 20, 21):
        yield "woz: the first %d bytes" % size, data[:size]
    yield "woz: no checksum", patched(crc=0)
    for junk in (1, 4, 7, 8, 9):
        yield "woz: %d bytes after the last chunk" % junk, sealed(data[12:] + b"\xff" * junk)
    yield "woz: nothing after the header", sealed(b"")
    yield "woz: INFO alone", sealed(data[12:info + 60])
    yield "woz: cut inside TMAP", sealed(data[12:tmap + 80])
    yield "woz: cut inside TRKS", sealed(data[12:trks + 100])
    yield "woz: cut at the TRKS header", sealed(data[12:trks - 3])

    for name, at in ((b"INFO", info), (b"TMAP", tmap), (b"TRKS", trks)):
        for size in (0, 8, 0x7fffffff, 0x80000000, 0xffffffff):
            yield "woz: %s of %#x bytes" % (name.decode(), size), patched((at - 4, size.to_bytes(4, "little")))
        yield "woz: %s followed by nothing" % name.decode(), sealed(data[12:at])
    yield "woz: TRKS of 8 bytes and no more", sealed(data[12:trks - 4] + (8).to_bytes(4, "little") + data[trks:trks + 8])
    yield "woz: an empty chunk first", sealed(b"WRIT" + bytes(4) + data[12:])
    yield "woz: META of ff", sealed(data[12:] + b"META" + (300).to_bytes(4, "little") + b"\xff" * 300)

    yield "woz: 3.5 inch", patched((info + 1, [2]))
    yield "woz: two sides", patched((info, [2]), (info + 37, [2]))
    yield "woz: the other version", patched((3, [ord("1") ^ ord("2") ^ data[3]]))
    for value in (0x23, 0x9f, 0xa0, 0xfe, 0xff):
        yield "woz: every track mapped to %#x" % value, patched((tmap, [value] * 160))

    if data[3:4] == b"2":
        entries = range(trks, trks + 160 * 8, 8)
        for bits in (0, 1, 72, 73, 136, 0x7fffffff, 0x80000000, 0xffffffff, len(data) * 8, len(data) * 8 + 1):
            yield "woz: tracks of %#x bits" % bits, patched(*[(e + 4, bits.to_bytes(4, "little")) for e in entries])
        for block in (0, 1, len(data) // 512 - 1, len(data) // 512, 0xffff):
            yield "woz: tracks at block %#x" % block, patched(*[(e, block.to_bytes(2, "little")) for e in entries])
    else:
        records = range(trks, trks + trks_size - 6655, 6656)
        for bits in (0, 1, 72, 73, 6646 * 8 + 1, 0xffff):
            yield "woz: tracks of %#x bits" % bits, patched(*[(r + 6648, bits.to_bytes(2, "little")) for r in records])
        yield "woz: half a track record at the end", sealed(data[12:trks - 4] + (trks_size - 3328).to_bytes(4, "little") + data[trks:trks + trks_size - 3328])

    # the bitstreams: from block 3 on in version 2, the first 6646 bytes of each record in version 1
    if data[3:4] == b"2":
        streams = [(1536, len(data) - 1536)]
    else:
        streams = [(r, 6646) for r in range(trks, trks + trks_size - 6655, 6656)]
    yield "woz: tracks of nothing", patched(*[(at, bytes(size)) for at, size in streams])
    yield "woz: tracks of ff", patched(*[(at, b"\xff" * size) for at, size in streams])
    for seed in range(6):
        rng = random.Random(seed)
        image = bytearray(data)
        for _ in range(4 << seed):
            image[rng.randrange(12, len(image))] = rng.randrange(256)
        yield "woz: noise behind the checksum, seed %d" % seed, sealed(image[12:])


def all_variants(data):
    yield from variants(data)
    if data[:3] == b"WOZ":
        yield from woz_variants(data)


def default_images():
    found = []
    for pattern in ("scott/test/formats/games/dsk-adventureland*/*Side A.dsk",
                    "comprehend/test/**/*.dsk",
                    "scott/test/formats/games/woz-adventureland*/*Side A.woz",
                    "plus/test/**/*.woz"):
        matches = sorted(glob.glob(os.path.join(TERPS, pattern), recursive=True))
        if matches:
            found.append(matches[0])
    # one .woz of each version
    wozzes = sorted(glob.glob(os.path.join(TERPS, "*/test/**/*.woz"), recursive=True))
    for magic in (b"WOZ1", b"WOZ2"):
        have = False
        for path in found:
            with open(path, "rb") as f:
                have = have or f.read(4) == magic
        for path in wozzes:
            if have:
                break
            with open(path, "rb") as f:
                if f.read(4) == magic:
                    found.append(path)
                    have = True
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
            for name, damaged in all_variants(data):
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
