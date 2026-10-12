#!/usr/bin/env python3
"""Tests c64diskimage.c, built with the address and undefined behaviour
sanitizers, on disk images made here and on damaged ones.

    ./malformed.py [image ...]

Two parts. The first builds a disk of every kind the reader takes (D64 of 35
and of 40 tracks, D71, D81, each with and without error info) with files
whose contents are known, and checks every line that c64diskimage_probe
prints for them. This is the only test of D71 and D81: no game in the corpora
comes on one.

The second damages those disks, and the images given or else one D64 from the
scott corpus if it is there (the corpora are not in the repository), and runs
the probe on each. What comes out is checked for a few of them only; for all
of them the probe must exit normally and in time.
"""

import glob
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TERPS = os.path.dirname(os.path.dirname(HERE))
PROBE = os.path.join(HERE, "c64diskimage_probe")
TIMEOUT = 20
PAD = 0xa0


class Geometry:
    """Where the sectors of one kind of disk are."""

    def __init__(self, kind, tracks, errors=False):
        self.kind, self.tracks, self.errors = kind, tracks, errors
        self.dirtrack = 40 if kind == "d81" else 18
        self.firstdir = 3 if kind == "d81" else 1

    def sectors(self, track):
        if self.kind == "d81":
            return 40
        if self.kind == "d71" and track > 35:
            track -= 35
        return 21 if track < 18 else 19 if track < 25 else 18 if track < 31 else 17

    def offset(self, track, sector):
        return (sum(self.sectors(t) for t in range(1, track)) + sector) * 256

    def blocks(self):
        return sum(self.sectors(t) for t in range(1, self.tracks + 1))

    def name(self):
        return "%s, %d tracks%s" % (self.kind, self.tracks, ", error info" if self.errors else "")


GEOMETRIES = [Geometry(kind, tracks, errors)
              for kind, tracks in (("d64", 35), ("d64", 40), ("d71", 70), ("d81", 80))
              for errors in (False, True)]


class Disk:
    """Builds a disk image file by file."""

    def __init__(self, geo):
        self.geo = geo
        self.image = bytearray(geo.blocks() * 256)
        self.used = {(geo.dirtrack, s) for s in range(geo.sectors(geo.dirtrack))}
        self.entries = 0
        self.dirsector = geo.firstdir
        # the sectors the directory may grow into, not in the order of the disk
        system = (0, 1, 2, 3) if geo.kind == "d81" else (0, 1)
        self.dirfree = sorted((s for s in range(geo.sectors(geo.dirtrack)) if s not in system),
                              key=lambda s: (s % 3, s))
        if geo.kind == "d81":
            self.put(40, 0, bytes([40, 3]))
        else:
            # the readers must not follow this link: the directory is at 18/1
            self.put(18, 0, bytes([18, 9]))
        self.put(geo.dirtrack, geo.firstdir, bytes([0, 0xff]))

    def put(self, track, sector, data, at=0):
        o = self.geo.offset(track, sector) + at
        self.image[o:o + len(data)] = data

    def free_sector(self, track):
        """The next unused sector, on track or the nearest one after it."""
        for t in list(range(track, self.geo.tracks + 1)) + list(range(1, track)):
            for s in range(self.geo.sectors(t)):
                if (t, s) not in self.used:
                    self.used.add((t, s))
                    return t, s
        raise RuntimeError("disk full")

    def add_chain(self, data, track):
        """Writes data as a chain of sectors and returns where it starts."""
        chunks = [data[i:i + 254] for i in range(0, len(data), 254)] or [b""]
        where = [self.free_sector(track) for _ in chunks]
        for i, chunk in enumerate(chunks):
            link = where[i + 1] if i + 1 < len(chunks) else (0, len(chunk) + 1)
            self.put(*where[i], bytes(link) + chunk)
        return where

    def add_entry(self, name, start, blocks, filetype=0x82):
        if self.entries and self.entries % 8 == 0:
            sector = self.dirfree.pop(0)
            self.put(self.geo.dirtrack, self.dirsector, bytes([self.geo.dirtrack, sector]))
            self.put(self.geo.dirtrack, sector, bytes([0, 0xff]))
            self.dirsector = sector
        entry = bytearray(30)
        entry[0] = filetype
        entry[1:3] = bytes(start)
        entry[3:19] = name.ljust(16, bytes([PAD]))[:16]
        entry[28:30] = blocks.to_bytes(2, "little")
        self.put(self.geo.dirtrack, self.dirsector, entry, 2 + (self.entries % 8) * 32)
        self.entries += 1

    def add_file(self, name, data, track=1, filetype=0x82, blocks=None):
        where = self.add_chain(data, track)
        self.add_entry(name, where[0], len(where) if blocks is None else blocks, filetype)
        return where

    def bytes(self):
        errors = bytes([1]) * self.geo.blocks() if self.geo.errors else b""
        return bytes(self.image) + errors


def contents(seed, size):
    return random.Random(seed).randbytes(size)


def fnv(data):
    h = 1469598103934665603
    for b in data:
        h = ((h ^ b) * 1099511628211) & 0xffffffffffffffff
    return "%016x" % h


def row(what, name, data):
    shown = "".join(chr(c) if 0x20 <= c <= 0x7e and c != 0x5c else "\\x%02x" % c for c in name)
    if data is None:
        return "%s\t%s\t-\t-" % (what, shown)
    return "%s\t%s\t%d\t%s" % (what, shown, len(data), fnv(data))


def standard_disk(geo):
    """A disk with a file of every kind that matters, and the rows other than
    ts that the probe must print for it. The directory takes three sectors:
    SCRATCHED and the file without a name are in the first, BIG is the first
    entry of the second."""
    directory = [
        # name, contents, track to start looking for room on, file type
        (b"FIRST", contents(1, 1000), 1, 0x82),
        (b"EMPTY", b"", 1, 0x82),
        (b"ONE BYTE", b"\x42", 2, 0x82),
        (b"LAST TRACK", contents(5, 3000), geo.tracks, 0x82),  # track 36 on, side 2, track 80
        (b"SCRATCHED", contents(7, 75000), 20, 0),  # not listed, not found, not the largest
        (b"", contents(8, 10), 4, 0x82),            # not listed
        (b"ONE SECTOR", contents(2, 254), 2, 0x81),
        (b"SIXTEEN CHARS 16", contents(3, 255), 3, 0x82),
        (b"BIG", contents(4, 70000), 5, 0x82),      # more than the 0xffff of the old reader
        (b"\xc1\xc2 \xff", contents(6, 20), 4, 0xc2),
    ]
    directory += [(b"X%d" % i, contents(20 + i, 300 + i), 6, 0x82) for i in range(9)]
    directory.append((b"FIRST", contents(9, 500), 7, 0x82))  # the first FIRST is the one read

    disk = Disk(geo)
    expected = []
    for name, data, track, filetype in directory:
        disk.add_file(name, data, track, filetype)
        if filetype != 0 and name != b"":
            expected.append(row("file", name, directory[0][1] if name == b"FIRST" else data))
    expected.append(row("largest", b"", contents(4, 70000)))
    expected.append(row("named", b"FIRST", directory[0][1]))
    return disk, expected


def run_probe(data, tmp):
    path = os.path.join(tmp, "image")
    with open(path, "wb") as f:
        f.write(data)
    try:
        r = subprocess.run([PROBE, path], capture_output=True, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        return None, "hung"
    if r.returncode != 0:
        lines = r.stderr.decode("latin-1").strip().splitlines()
        return None, "exit %d: %s" % (r.returncode, next((l for l in lines if "ERROR" in l or "error" in l), ""))
    return r.stdout.decode("latin-1").splitlines(), None


def rows(lines, what):
    return [l for l in lines if l.startswith(what + "\t")]


class Results:
    def __init__(self):
        self.runs = self.failures = 0

    def check(self, name, ok, why=""):
        self.runs += 1
        if not ok:
            self.failures += 1
            print("FAIL %s%s" % (name, ": " + why if why else ""))


def known_disks(tmp, res):
    """Every line of the probe for disks whose contents are known."""
    for geo in GEOMETRIES:
        disk, expected = standard_disk(geo)
        lines, err = run_probe(disk.bytes(), tmp)
        if err:
            res.check(geo.name(), False, err)
            continue
        got = [l for l in lines if not l.startswith("ts\t")]
        res.check(geo.name() + ": files", got == expected,
                  "\n  expected %s\n  got      %s" % (expected, got))
        # every chain is also found from its first sector
        ts = {l.split("\t")[1]: l.split("\t", 2)[2] for l in rows(lines, "ts")}
        big = row("x", b"", contents(4, 70000)).split("\t", 2)[2]
        res.check(geo.name() + ": chains by track and sector", big in ts.values())
        # no sector off the disk gives a file
        valid = {"%d/%d" % (t, s) for t in range(1, geo.tracks + 1) for s in range(geo.sectors(t))}
        res.check(geo.name() + ": sectors off the disk", set(ts) <= valid, str(sorted(set(ts) - valid)[:5]))

    # A full directory: 144 entries on a D64, 296 on a D81
    for geo, count in ((GEOMETRIES[0], 144), (GEOMETRIES[6], 296)):
        disk = Disk(geo)
        start = disk.add_chain(b"same", 1)[0]
        for i in range(count):
            disk.add_entry(b"FILE %d" % i, start, 1)
        lines, err = run_probe(disk.bytes(), tmp)
        res.check("%s: %d entries" % (geo.name(), count),
                  not err and rows(lines, "file") == [row("file", b"FILE %d" % i, b"same") for i in range(count)], err or "")

    # Sizes that are no disk image
    for size in (0, 1, 255, 174847, 174849, 175530, 196607, 349695, 819201, 900000):
        lines, err = run_probe(bytes(size), tmp)
        res.check("%d bytes" % size, lines == ["image\t\t-\t-"], err or str(lines))


def damaged_known(tmp, res):
    """Damage whose outcome is known."""
    for geo in GEOMETRIES:
        o = geo.offset
        dt, ds = geo.dirtrack, geo.firstdir
        disk, expected = standard_disk(geo)
        good = disk.bytes()
        # where FIRST starts: its entry is the first of the directory
        first = (good[o(dt, ds) + 3], good[o(dt, ds) + 4])
        second = (good[o(*first)], good[o(*first) + 1])

        def patched(*patches):
            image = bytearray(good)
            for offset, values in patches:
                image[offset:offset + len(values)] = bytes(values)
            return bytes(image)

        def file_row(lines, name=b"FIRST"):
            shown = row("file", name, b"").split("\t")[1]
            found = [l for l in rows(lines, "file") if l.split("\t")[1] == shown]
            return found[0] if found else None

        def case(name, image, test):
            lines, err = run_probe(image, tmp)
            res.check("%s: %s" % (geo.name(), name), not err and test(lines), err or "unexpected result")

        whole = contents(1, 1000)
        beyond = [(0, 1), (geo.tracks + 1, 0), (1, geo.sectors(1)), (255, 255), (geo.tracks, geo.sectors(geo.tracks))]
        for bad in beyond:
            # the first sector, or the link in it, off the disk: no file
            case("file starts at %d/%d" % bad, patched((o(dt, ds) + 3, bad)),
                 lambda lines: file_row(lines) == row("file", b"FIRST", None))
            if bad[0] != 0:
                case("first sector links to %d/%d" % bad, patched((o(*first), bad)),
                     lambda lines: file_row(lines) == row("file", b"FIRST", None))
                # further on: the file ends before the sector with the bad link
                case("second sector links to %d/%d" % bad, patched((o(*second), bad)),
                     lambda lines: file_row(lines) == row("file", b"FIRST", whole[:254]))
        # a chain that comes back to a sector ends there
        case("second sector links to itself", patched((o(*second), second)),
             lambda lines: file_row(lines) == row("file", b"FIRST", whole[:508]))
        case("second sector links to the first", patched((o(*second), first)),
             lambda lines: file_row(lines) == row("file", b"FIRST", whole[:508]))
        case("first sector links to itself", patched((o(*first), first)),
             lambda lines: file_row(lines) == row("file", b"FIRST", whole[:254]))
        # a last sector that says it holds nothing, in both ways
        for link in ((0, 0), (0, 1)):
            case("first sector links to %d/%d" % link, patched((o(*first), link)),
                 lambda lines: file_row(lines) == row("file", b"FIRST", b""))
        case("last sector is full", patched((o(*first), (0, 255))),
             lambda lines: file_row(lines) == row("file", b"FIRST", whole[:254]))

        # the directory
        names = len(expected) - 2
        case("directory links to itself", patched((o(dt, ds), (dt, ds))),
             lambda lines: len(rows(lines, "file")) == 6)  # eight entries, two not listed
        for bad in beyond:
            case("directory links to %d/%d" % bad, patched((o(dt, ds), bad)),
                 lambda lines: len(rows(lines, "file")) == 6)
        second_dir = good[o(dt, ds) + 1]
        third = good[o(dt, second_dir) + 1]
        case("directory links back to its start", patched((o(dt, third), (dt, ds))),
             lambda lines: len(rows(lines, "file")) == names)
        case("largest entry is scratched", patched((o(dt, second_dir) + 2, [0])),
             lambda lines: rows(lines, "largest") == [row("largest", b"", contents(5, 3000))])
        if geo.kind == "d81":
            case("header links off the disk", patched((o(40, 0), (81, 0))),
                 lambda lines: rows(lines, "file") == [] and rows(lines, "largest") == [row("largest", b"", None)])


def variants(data, geo):
    """Yields (name, bytes) for each way of damaging a disk image blindly."""
    o = geo.offset
    dt = geo.dirtrack
    dirsectors = [o(dt, s) for s in range(geo.sectors(dt))]

    def patched(*patches):
        image = bytearray(data)
        for offset, values in patches:
            image[offset:offset + len(values)] = bytes(values)
        return bytes(image)

    entries = [d + i * 32 for d in dirsectors for i in range(8)]
    yield "files start off the disk", patched(*[(e + 3, [0xfe, 0xfe]) for e in entries])
    yield "files start at track 0", patched(*[(e + 3, [0, 0]) for e in entries])
    yield "files start in the directory", patched(*[(e + 3, [dt, geo.firstdir]) for e in entries])
    yield "files of 65535 blocks", patched(*[(e + 30, [0xff, 0xff]) for e in entries])
    yield "files of no blocks", patched(*[(e + 30, [0, 0]) for e in entries])
    yield "every entry live", patched(*[(e + 2, [0x82]) for e in entries])
    yield "every entry scratched", patched(*[(e + 2, [0]) for e in entries])
    yield "names of ff", patched(*[(e + 5, [0xff] * 16) for e in entries])
    yield "names of nul", patched(*[(e + 5, [0] * 16) for e in entries])
    yield "names of stars", patched(*[(e + 5, b"*" * 16) for e in entries])
    yield "directory sectors link to themselves", patched(*[(d, [dt, i]) for i, d in enumerate(dirsectors)])
    yield "directory sectors link round the track", patched(
        *[(d, [dt, (i + 1) % len(dirsectors)]) for i, d in enumerate(dirsectors)])
    yield "directory sectors link off the disk", patched(*[(d, [0xff, 0xff]) for d in dirsectors])

    blocks = len(data) // 256
    for link, what in (((1, 0), "1/0"), ((0, 0), "0/0"), ((0, 0xff), "0/255"), ((0xff, 0xff), "255/255"),
                       ((geo.tracks, 0), "the last track"), ((geo.tracks + 1, 0), "past the last track")):
        yield "every sector links to %s" % what, patched(*[(b * 256, link) for b in range(blocks)])
    # one chain through every sector of the disk: the longest file there can be
    order = [(t, s) for t in range(1, geo.tracks + 1) for s in range(geo.sectors(t))]
    yield "one chain through the whole disk", patched(
        *[(o(*order[i]), order[(i + 1) % len(order)]) for i in range(len(order))])
    yield "every sector links to itself", patched(*[(o(*ts), ts) for ts in order])

    for seed in range(8):
        rng = random.Random(seed)
        image = bytearray(data)
        for _ in range(len(data) // (20 << seed) + 1):
            image[rng.randrange(len(image))] = rng.randrange(256)
        yield "noise, seed %d" % seed, bytes(image)
    for seed in range(4):
        # noise in the directory and in the links only, which goes deeper
        rng = random.Random(100 + seed)
        image = bytearray(data)
        for d in dirsectors:
            for _ in range(8 << seed):
                image[d + rng.randrange(256)] = rng.randrange(256)
        for b in range(blocks):
            if rng.randrange(4) == 0:
                image[b * 256] = rng.randrange(geo.tracks + 3)
                image[b * 256 + 1] = rng.randrange(44)
        yield "noise in directory and links, seed %d" % seed, bytes(image)

    yield "all zero", bytes(len(data))
    yield "all ff", b"\xff" * len(data)
    for size in (0, 1, 256, o(dt, 0) + 3, len(data) - 1, len(data) + 1):
        yield "cut or grown to %d" % size, (data + b"\0")[:size]


def geometry_of(size):
    for geo in GEOMETRIES:
        if geo.blocks() * (257 if geo.errors else 256) == size:
            return geo
    return None


def main():
    build = subprocess.run(["cc", "-std=c11", "-O1", "-g", "-Wall", "-fsanitize=address,undefined",
                            "-fno-sanitize-recover=undefined", "-I" + os.path.join(HERE, ".."),
                            "-o", PROBE, os.path.join(HERE, "c64diskimage_probe.c"),
                            os.path.join(HERE, "..", "c64diskimage.c")])
    if build.returncode != 0:
        return 1

    images = sys.argv[1:] or sorted(glob.glob(os.path.join(TERPS, "scott/test/formats/games/d64-*/*.d64")))[:1]
    res = Results()
    with tempfile.TemporaryDirectory() as tmp:
        known_disks(tmp, res)
        damaged_known(tmp, res)

        sources = [(geo.name(), standard_disk(geo)[0].bytes(), geo) for geo in GEOMETRIES]
        for path in images:
            with open(path, "rb") as f:
                data = f.read()
            geo = geometry_of(len(data))
            if geo is None:
                print("%s is not a disk image" % path)
                return 1
            sources.append((os.path.basename(path), data, geo))
        for source, data, geo in sources:
            for name, image in variants(data, geo):
                lines, err = run_probe(image, tmp)
                res.check("%s: %s" % (source, name), not err, err or "")

    print("c64diskimage: %d runs, %d failed" % (res.runs, res.failures))
    return 1 if res.failures else 0


if __name__ == "__main__":
    sys.exit(main())
