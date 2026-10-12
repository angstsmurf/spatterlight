#!/usr/bin/env python3
"""Feeds damaged ZX Spectrum snapshots and tapes to the decompressz80 library,
built with the address and undefined behaviour sanitizers: none may crash it
or hang it.

    ./malformed.py [image ...]

The images to damage default to one of each kind from the scott and taylor
corpora, whichever are present (the corpora are not in the repository): a
version 1 .z80, a 48K and a 128K one of the later versions, a .tzx and a .tap.
Every damaged image goes through all the operations of z80_probe, as the
interpreters hand any file to any of the readers. What comes out is not
checked, only that the probe exits normally.
"""

import glob
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TERPS = os.path.dirname(os.path.dirname(HERE))
OPS = ["z80", "pages", "tzx", "tap", "payloads"]
TIMEOUT = 20
PAGE = 0x4000


def le(value, size):
    return value.to_bytes(size, "little")


def patched(data, *patches):
    image = bytearray(data)
    for offset, values in patches:
        image[offset:offset + len(values)] = bytes(values)
    return bytes(image)


def kind_of(data):
    """'v1', '48k', '128k', 'tzx', 'tap' or None for a file from the corpora."""
    if data[:8] == b"ZXTape!\x1a":
        return "tzx"
    if len(data) > 87 and data[6:8] == b"\0\0" and data[30] in (23, 54, 55) and data[31] == 0:
        machine = data[34]
        if machine >= 7 or machine >= (3 if data[30] == 23 else 4):
            return "128k"
        return "48k"
    if len(data) > 30 and data[6:8] != b"\0\0" and data[-4:] == b"\x00\xed\xed\x00":
        return "v1"
    if len(data) > 21 and data[:3] == b"\x13\x00\x00":
        return "tap"
    return None


def z80_pages(data):
    """The offsets of the page headers of a version 2 or 3 snapshot."""
    pos = 32 + data[30]
    while pos + 3 <= len(data):
        yield pos
        stored = int.from_bytes(data[pos:pos + 2], "little")
        pos += 3 + (PAGE if stored == 0xffff else stored)


def z80_variants(data, kind):
    if kind == "v1":
        yield "z80: not compressed", patched(data, (12, [data[12] & ~0x20]))
        yield "z80: no end marker", data[:-4]
        yield "z80: two end markers", data + data[-4:]
        yield "z80: an end marker first", data[:30] + data[-4:] + data[30:]
        yield "z80: version 2 header on it", patched(data, (6, [0, 0]), (30, [23, 0]))
        yield "z80: version 3 header on it", patched(data, (6, [0, 0]), (30, [54, 0]))
        yield "z80: runs of 255", patched(data, *[(i, b"\xed\xed\xff") for i in range(30, len(data) - 8, 4000)])
        yield "z80: ED ED before the marker", data[:-4] + b"\xed\xed" + data[-4:]
        yield "z80: ED ED nn before the marker", data[:-4] + b"\xed\xed\x05" + data[-4:]
        return

    body = 32 + data[30]
    for size in (30, 31, 32, 33, body - 1, body, body + 1, body + 2, body + 3, body + 4):
        yield "z80: the first %d bytes" % size, data[:size]
    for extra in (0, 1, 22, 23, 24, 54, 55, 56, 0xff, 0xffff):
        yield "z80: header of %d more bytes" % extra, patched(data, (30, le(extra, 2)))
    for machine in range(20):
        yield "z80: machine type %d" % machine, patched(data, (34, [machine]))
    for machine in (0x7f, 0x80, 0x81, 0xff):
        yield "z80: machine type %d" % machine, patched(data, (34, [machine]))
    for port in (0, 5, 7, 8, 0x10, 0x17, 0xff):
        yield "z80: port value %#x" % port, patched(data, (35, [port]))
    yield "z80: a version 1 header on it", patched(data, (6, [0x34, 0x12]))
    yield "z80: version 1, compressed", patched(data, (6, [0x34, 0x12]), (12, [0x20]))

    pages = list(z80_pages(data))
    for number in (0, 1, 2, 3, 8, 11, 12, 18, 19, 0x80, 0xff):
        yield "z80: every page is page %d" % number, patched(data, *[(p + 2, [number]) for p in pages])
        yield "z80: the last page is page %d" % number, patched(data, (pages[-1] + 2, [number]))
    for stored in (0, 1, 2, 3, 4, PAGE - 1, PAGE, PAGE + 1, 0xfffe, 0xffff):
        yield "z80: pages of %#x bytes" % stored, patched(data, *[(p, le(stored, 2)) for p in pages])
        yield "z80: first page of %#x bytes" % stored, patched(data, (pages[0], le(stored, 2)))
        yield "z80: last page of %#x bytes" % stored, patched(data, (pages[-1], le(stored, 2)))
    yield "z80: no pages", data[:pages[0]]
    yield "z80: one page", data[:pages[1]] if len(pages) > 1 else data
    yield "z80: last page missing", data[:pages[-1]]
    yield "z80: last page header only", data[:pages[-1] + 3]
    yield "z80: half a page header", data[:pages[-1] + 2]
    yield "z80: every page twice", data + data[pages[0]:]
    yield "z80: ED ED at the end", data + b"\x02\x00\x03\xed\xed"
    yield "z80: ED ED nn at the end", data + b"\x03\x00\x03\xed\xed\x09"
    yield "z80: a page of runs of 255", data[:pages[0]] + le(260, 2) + b"\x08" + b"\xed\xed\xff\x00" * 65
    yield "z80: a page of one run of 0", data[:pages[0]] + le(4, 2) + b"\x08" + b"\xed\xed\x00\x00"
    yield "z80: SLT in place of the pages", data[:pages[0]] + b"\0\0\0SLT" + b"\xff" * 40
    yield "z80: SLT and nothing after it", data + b"\0\0\0SLT"
    yield "z80: SLT cut short", data + b"\0\0\0SL"
    yield "z80: SLT after the pages", data + b"\0\0\0SLT" + b"\xff" * 400


def tzx_blocks(data):
    """The offsets of the blocks of a .tzx, as far as they are of known kinds."""
    lengths = {0x10: (4, 2, 2, 1), 0x11: (18, 15, 3, 1), 0x12: (4, 0, 0, 0), 0x13: (1, 0, 1, 2),
               0x14: (10, 7, 3, 1), 0x15: (8, 5, 3, 1), 0x18: (4, 0, 4, 1), 0x19: (4, 0, 4, 1),
               0x20: (2, 0, 0, 0), 0x21: (1, 0, 1, 1), 0x22: (0, 0, 0, 0), 0x23: (2, 0, 0, 0),
               0x24: (2, 0, 0, 0), 0x25: (0, 0, 0, 0), 0x26: (2, 0, 2, 2), 0x27: (0, 0, 0, 0),
               0x28: (2, 0, 2, 1), 0x2a: (4, 0, 4, 1), 0x2b: (4, 0, 4, 1), 0x30: (1, 0, 1, 1),
               0x31: (2, 1, 1, 1), 0x32: (2, 0, 2, 1), 0x33: (1, 0, 1, 3), 0x35: (20, 16, 4, 1),
               0x5a: (9, 0, 0, 0)}
    pos = 10
    while pos < len(data) and data[pos] in lengths:
        fixed, at, size, item = lengths[data[pos]]
        yield pos, pos + 1 + at, size
        count = int.from_bytes(data[pos + 1 + at:pos + 1 + at + size], "little")
        pos += 1 + fixed + count * item


def tzx_variants(data):
    blocks = list(tzx_blocks(data))
    for size in (0, 1, 7, 8, 9, 10, 11, 12, 14, 15):
        yield "tzx: the first %d bytes" % size, data[:size]
    yield "tzx: no signature", patched(data, (0, b"ZXtape!"))
    for start, _, _ in blocks:
        yield "tzx: cut at block at %#x" % start, data[:start + 1]
        yield "tzx: cut in the header of the block at %#x" % start, data[:start + 3]
    for start, at, size in blocks:
        if size == 0:
            continue
        for value in (0, 1, 2, 3, (1 << 8 * size) - 1, 1 << (8 * size - 1), len(data)):
            value &= (1 << 8 * size) - 1
            yield "tzx: block at %#x of length %#x" % (start, value), patched(data, (at, le(value, size)))
    for block_id in list(range(0x10, 0x36)) + [0x40, 0x5a, 0x00, 0xff]:
        yield "tzx: first block is a %#04x" % block_id, patched(data, (10, [block_id]))
        yield "tzx: every block is a %#04x" % block_id, patched(data, *[(b, [block_id]) for b, _, _ in blocks])
        yield "tzx: ends with a %#04x" % block_id, data + bytes([block_id])
        yield "tzx: ends with a %#04x of ff" % block_id, data + bytes([block_id]) + b"\xff" * 30
        yield "tzx: ends with a %#04x of 00" % block_id, data + bytes([block_id]) + bytes(30)
    yield "tzx: all lengths ff", patched(data, *[(at, b"\xff" * size) for _, at, size in blocks])
    yield "tzx: all lengths 0", patched(data, *[(at, bytes(size)) for _, at, size in blocks])
    yield "tzx: nothing but a header", data[:10]
    yield "tzx: as a tap", data[10:]


def tap_variants(data):
    blocks = []
    pos = 0
    while pos + 2 <= len(data):
        blocks.append(pos)
        pos += 2 + int.from_bytes(data[pos:pos + 2], "little")
    for start in blocks:
        for value in (0, 1, 2, 3, 0x7fff, 0x8000, 0xffff, (len(data) - start - 2) & 0xffff,
                      (len(data) - start - 1) & 0xffff):
            yield "tap: block at %#x of length %#x" % (start, value), patched(data, (start, le(value, 2)))
        yield "tap: cut at block at %#x" % start, data[:start + 1]
        yield "tap: cut after the length at %#x" % start, data[:start + 2]
        yield "tap: cut after the flag at %#x" % start, data[:start + 3]
    yield "tap: all lengths 0", patched(data, *[(b, [0, 0]) for b in blocks])
    yield "tap: all lengths 1", patched(data, *[(b, [1, 0]) for b in blocks])
    yield "tap: all lengths 2", patched(data, *[(b, [2, 0]) for b in blocks])
    yield "tap: all lengths ff", patched(data, *[(b, [0xff, 0xff]) for b in blocks])
    yield "tap: a block of nothing and then the tape", b"\0\0" + data
    yield "tap: blocks of one byte", b"\x01\x00\xff" * 400
    yield "tap: blocks of two bytes", b"\x02\x00\xff\xff" * 400
    yield "tap: blocks of three bytes", b"\x03\x00\xff\x55\xff" * 400
    yield "tap: a TZX header on it", b"ZXTape!\x1a\x01\x14" + data


def variants(data, kind):
    """Yields (name, bytes) for each way of damaging an image."""
    for size in (0, 1, 2, 3, 29, 30, 31, 86, 87, 88, len(data) // 2, len(data) - 5,
                 len(data) - 4, len(data) - 3, len(data) - 2, len(data) - 1):
        if 0 <= size < len(data):
            yield "truncated to %d" % size, data[:size]
    yield "extended with itself", data + data[:5000]
    yield "extended with ED", data + b"\xed" * 7
    yield "extended with 00", data + bytes(3)
    yield "all zero", bytes(len(data))
    yield "all ff", b"\xff" * len(data)
    yield "all ED", b"\xed" * len(data)
    yield "all ED after the header", data[:87] + b"\xed" * (len(data) - 87)

    if kind == "tzx":
        yield from tzx_variants(data)
    elif kind == "tap":
        yield from tap_variants(data)
    else:
        yield from z80_variants(data, kind)

    # noise: all over the file, and in the first 300 bytes where the headers are
    for seed in range(10):
        rng = random.Random(seed)
        image = bytearray(data)
        for _ in range(len(data) // (40 << seed) + 1):
            image[rng.randrange(len(image))] = rng.randrange(256)
        yield "noise, seed %d" % seed, bytes(image)
    for seed in range(40):
        rng = random.Random(1000 + seed)
        image = bytearray(data)
        for _ in range(1 + seed % 4):
            image[rng.randrange(min(300, len(image)))] = rng.choice([0, 1, 0xed, 0xff, rng.randrange(256)])
        yield "noise in the headers, seed %d" % seed, bytes(image)


def default_images():
    """The smallest image of each kind that the corpora have."""
    found = {}
    for corpus in ("scott/test", "taylor/test"):
        for path in sorted(glob.glob(os.path.join(TERPS, corpus, "**", "*.*"), recursive=True)):
            if os.path.splitext(path)[1].lower() not in (".z80", ".tzx", ".tap"):
                continue
            with open(path, "rb") as f:
                data = f.read()
            kind = kind_of(data)
            if kind and (kind not in found or len(data) < found[kind][0]):
                found[kind] = (len(data), path)
    return [path for _, path in sorted(found.values())]


def main():
    probe = os.path.join(tempfile.gettempdir(), "z80_probe_san")
    build = ["cc", "-std=c11", "-O1", "-g", "-fsanitize=address,undefined",
             "-fno-sanitize-recover=undefined", "-I" + os.path.join(HERE, ".."),
             "-o", probe, os.path.join(HERE, "z80_probe.c")]
    build += sorted(glob.glob(os.path.join(HERE, "..", "*.c")))
    if subprocess.run(build).returncode != 0:
        return 1

    images = sys.argv[1:] or default_images()
    if not images:
        print("malformed images: no image to damage (the corpora are not here), skipped")
        return 0

    runs = failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        victim = os.path.join(tmp, "victim")
        for path in images:
            with open(path, "rb") as f:
                data = f.read()
            kind = kind_of(data)
            if kind is None:
                print("SKIP    %s: neither a .z80, a .tzx nor a .tap that is known here" % path)
                continue
            for name, damaged in variants(data, kind):
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
