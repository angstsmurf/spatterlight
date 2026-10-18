#!/usr/bin/env python3
"""What the graphics of a V6 trace look like, and whether two traces look
the same.

A trace from v6trace.py says which picture files the interpreter sent and
where it had them drawn, but two runs can send different files and still put
the same thing on the screen: a composited border with more transparent rows
under it is another file, drawn into a taller rectangle, and looks no
different.  This replays a trace the way the app would act on it -- graphics
windows only: their sizes, background colours, clears, rectangle fills and
picture draws -- and so gets the screens themselves, to compare pixel by
pixel.  It needs the pictures, which v6trace.py keeps with --pictures and
run_v6_traces.py keeps in OUT/pictures.

    v6screens.py compare OLD NEW [--montages DIR] [--jobs N] [--all]
                                 [PATTERN ...]

      OLD and NEW are run_v6_traces.py --out directories.  Every case whose
      trace differs between them (with --all, every case) is replayed from
      both, and the verdict printed: SAME SCREENS, SHIFTED PIXELS (every
      changed pixel has its old colour and its new one right next to it in
      the other screen: a picture scaled a fraction differently) or SCREENS
      DIFFER, with how many and by how much.  For the first few screens of a
      case that differ, a picture with the old screen, the new one and the
      pixels that changed goes to DIR (default NEW/montages; the name ends
      in -shift for a screen that is only shifted).  Exit status 1 if any
      screen differs by more than a shift.

    v6screens.py render OUT CASE DIR [--every N]

      Write the screens of one case as PNG files, to look at.

A "screen" is taken whenever the app answers the interpreter with an event
and something was drawn since the last one: the graphics windows, composited
in the order of their numbers on a grey ground.  Text windows are left out
(the trace has their text, and a change there is a changed trace line).  A
picture drawn into a text window, as Shogun's and Journey's illustrations
are, is a screen of its own, scaled the way it was drawn.

Pictures are scaled by nearest neighbour, so that a screen is the same for
the same input everywhere.  The app interpolates; a picture stretched by a
fraction of a pixel more will show here as a row of changed pixels where the
app shows a faint blur.
"""

import collections
import concurrent.futures
import itertools
import os
import re
import struct
import sys
import zlib

import numpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from v6trace import picture_name  # noqa: E402

WINTYPE_GRAPHICS = 5
GROUND = (128, 128, 128)        # where no graphics window is
LIMIT = 4096                    # no window or picture is larger than this
MONTAGES_PER_CASE = 4
PICTURE_LINES = ("PURGEIMG ", "DRAWIMAGE ", "LOADIMAGE ", "FINDIMAGE ")


def decode_tiff(data):
    """RGBA rows of a TIFF from writetiff.cpp: big-endian, one uncompressed
    strip of four bytes per pixel right after the header."""
    if data[:4] != b"MM\0*":
        return None
    ifd, = struct.unpack_from(">I", data, 4)
    count, = struct.unpack_from(">H", data, ifd)
    tags = {}
    for index in range(count):
        tag, kind, _, value = struct.unpack_from(">HHII", data,
                                                 ifd + 2 + index * 12)
        tags[tag] = value >> 16 if kind == 3 else value
    width, height, start = tags[0x100], tags[0x101], tags[0x111]
    pixels = numpy.frombuffer(data, numpy.uint8, width * height * 4, start)
    return pixels.reshape(height, width, 4)


def decode_picture(data):
    picture = decode_tiff(data)
    if picture is None:
        import io
        from PIL import Image
        picture = numpy.asarray(Image.open(io.BytesIO(data)).convert("RGBA"))
    return picture


class Pictures:
    """The pictures of a --pictures directory, decoded as they are asked
    for."""

    def __init__(self, directory):
        self.directory = directory
        self.decoded = collections.OrderedDict()
        self.scaled = collections.OrderedDict()

    def get(self, name):
        if name not in self.decoded:
            try:
                with open(os.path.join(self.directory, name), "rb") as f:
                    picture = decode_picture(zlib.decompress(f.read()))
            except (OSError, ValueError, KeyError, struct.error):
                picture = None
            self.decoded[name] = picture
            if len(self.decoded) > 64:
                self.decoded.popitem(last=False)
        return self.decoded[name]

    def get_scaled(self, name, width, height):
        """The picture stretched to WIDTH x HEIGHT (its own size where
        those are 0), or None."""
        key = (name, width, height)
        if key in self.scaled:
            self.scaled.move_to_end(key)
            return self.scaled[key]
        picture = self.get(name)
        if picture is not None and picture.size:
            rows, columns = picture.shape[:2]
            width = width or columns
            height = height or rows
            if (height, width) != (rows, columns):
                # Stretched to the full size, of which no more than LIMIT
                # each way is kept: what is beyond that is never on screen.
                ys = numpy.arange(min(height, LIMIT)) * rows // height
                xs = numpy.arange(min(width, LIMIT)) * columns // width
                picture = picture[ys][:, xs]
        elif picture is not None:
            picture = None
        self.scaled[key] = picture
        if len(self.scaled) > 48:
            self.scaled.popitem(last=False)
        return picture


def paste(canvas, picture, x, y):
    """PICTURE (RGBA) onto CANVAS (RGB) with its top left corner at X, Y;
    transparent pixels leave the canvas alone."""
    height, width = canvas.shape[:2]
    rows, columns = picture.shape[:2]
    left, top = max(x, 0), max(y, 0)
    right, bottom = min(x + columns, width), min(y + rows, height)
    if left >= right or top >= bottom:
        return
    piece = picture[top - y:bottom - y, left - x:right - x]
    target = canvas[top:bottom, left:right]
    opaque = piece[:, :, 3] >= 128
    target[opaque] = piece[:, :, :3][opaque]


def rgb(color):
    return ((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff)


class Window:
    def __init__(self, kind):
        self.kind = kind
        self.rect = (0, 0, 0, 0)
        self.background = 0xffffff
        self.canvas = numpy.zeros((0, 0, 3), numpy.uint8)

    def resize(self, rect):
        # What was drawn stays where it was; the game draws again after a
        # resize if it wants anything else.
        self.rect = rect
        width = max(0, min(rect[2] - rect[0], LIMIT))
        height = max(0, min(rect[3] - rect[1], LIMIT))
        if (height, width) == self.canvas.shape[:2]:
            return False
        canvas = numpy.empty((height, width, 3), numpy.uint8)
        canvas[:] = rgb(self.background)
        keep_rows = min(height, self.canvas.shape[0])
        keep_columns = min(width, self.canvas.shape[1])
        canvas[:keep_rows, :keep_columns] = \
            self.canvas[:keep_rows, :keep_columns]
        self.canvas = canvas
        return True

    def fill(self, color, x, y, width, height):
        # A colour above 0xffffff is one of the zcolor_* stand-ins, which
        # the app resolves against the theme.
        if color > 0xffffff:
            color = self.background
        rows, columns = self.canvas.shape[:2]
        left, top = max(x, 0), max(y, 0)
        right, bottom = min(x + width, columns), min(y + height, rows)
        if left < right and top < bottom:
            self.canvas[top:bottom, left:right] = rgb(color)


RECT = re.compile(r"rect (-?\d+),(-?\d+)-(-?\d+),(-?\d+) of (\d+)x(\d+)")
DRAW = re.compile(r"at (-?\d+),(-?\d+) size (\d+)x(\d+)")
FILL = re.compile(r"([0-9a-f]+)@(-?\d+),(-?\d+)\+(-?\d+)x(-?\d+)")


def screens(trace, pictures):
    """Yield (label, screen) for every screen of TRACE (its text), a screen
    being a height x width x 3 array of bytes.  PICTURES is a Pictures."""
    windows = {}
    size = [800, 600]
    current = None                      # name of the picture last sent
    held = {}                           # resource number -> picture name
    dirty = False

    def composite():
        screen = numpy.empty((min(size[1], LIMIT), min(size[0], LIMIT), 3),
                             numpy.uint8)
        screen[:] = GROUND
        for peer in sorted(windows):
            window = windows[peer]
            if window.kind != WINTYPE_GRAPHICS or not window.canvas.size:
                continue
            x, y = window.rect[:2]
            rows, columns = screen.shape[:2]
            left, top = max(x, 0), max(y, 0)
            right = min(x + window.canvas.shape[1], columns)
            bottom = min(y + window.canvas.shape[0], rows)
            if left < right and top < bottom:
                screen[top:bottom, left:right] = \
                    window.canvas[top - y:bottom - y, left - x:right - x]
        return screen

    for number, line in enumerate(trace.split("\n"), 1):
        if line.startswith("---") or line.startswith("==="):
            if dirty:
                dirty = False
                yield "line %d" % number, composite()
            if line.startswith("====== session"):
                windows.clear()
                held.clear()
                current = None
            continue
        word, _, rest = line.partition(" ")
        if word not in ("NEWWIN", "DELWIN", "SIZWIN", "CLRWIN", "SETBGND",
                        "FILLRECT", "DRAWIMAGE", "PURGEIMG", "LOADIMAGE",
                        "FINDIMAGE"):
            continue
        fields = rest.split(" ")
        a1 = int(fields[0])
        if word == "NEWWIN":
            windows[int(fields[1])] = Window(a1)
        elif word == "DELWIN":
            gone = windows.pop(a1, None)
            dirty = dirty or (gone is not None
                              and gone.kind == WINTYPE_GRAPHICS
                              and gone.canvas.size > 0)
        elif word in ("PURGEIMG", "LOADIMAGE"):
            if " file " not in line:
                current = None
                held.pop(a1, None)
                continue
            digest = line.rsplit(" file ", 1)[1]
            if word == "LOADIMAGE":
                current = picture_name(digest, int(fields[1]), int(fields[2]))
            else:
                current = picture_name(digest)
            held[a1] = current
        elif word == "FINDIMAGE":
            # Answered yes (v6trace.py) exactly when the number is held, and
            # the picture held under it becomes the current one, as in the
            # app; on a no the interpreter sends the picture next.
            current = held.get(a1)
        elif a1 not in windows:
            continue
        elif word == "SIZWIN":
            x0, y0, x1, y1, width, height = (int(n) for n in
                                             RECT.search(line).groups())
            size[:] = width, height
            window = windows[a1]
            moved = window.rect != (x0, y0, x1, y1)
            window.resize((x0, y0, x1, y1))
            dirty = dirty or (moved and window.kind == WINTYPE_GRAPHICS)
        elif word == "SETBGND":
            color = int(fields[1])
            windows[a1].background = color if 0 <= color <= 0xffffff \
                else 0xffffff
        elif word == "CLRWIN":
            window = windows[a1]
            if window.kind == WINTYPE_GRAPHICS:
                window.canvas[:] = rgb(window.background)
                dirty = True
        elif word == "FILLRECT":
            window = windows[a1]
            if window.kind == WINTYPE_GRAPHICS:
                for color, x, y, width, height in FILL.findall(rest):
                    window.fill(int(color, 16), int(x), int(y), int(width),
                                int(height))
                dirty = True
        elif word == "DRAWIMAGE":
            x, y, width, height = (int(n) for n in DRAW.search(line).groups())
            picture = pictures.get_scaled(current, width, height) \
                if current else None
            window = windows[a1]
            if window.kind == WINTYPE_GRAPHICS:
                if picture is not None:
                    paste(window.canvas, picture, x, y)
                dirty = True
            else:
                # Inline in a text window: x is the alignment, y nothing.
                if picture is None:
                    inline = numpy.zeros((1, 1, 3), numpy.uint8)
                else:
                    inline = numpy.empty(picture.shape[:2] + (3,),
                                         numpy.uint8)
                    inline[:] = GROUND
                    paste(inline, picture, 0, 0)
                yield "line %d (inline in window %d)" % (number, a1), inline
    if dirty:
        yield "end", composite()


def found_nearby(wanted, screen):
    """Where the pixel of WANTED is also in SCREEN at most one pixel away."""
    height, width = wanted.shape[:2]
    padded = numpy.pad(screen, ((1, 1), (1, 1), (0, 0)), mode="edge")
    found = numpy.zeros((height, width), bool)
    for dy in range(3):
        for dx in range(3):
            found |= (padded[dy:dy + height, dx:dx + width] == wanted) \
                .all(axis=2)
    return found


def difference(old, new):
    """None if the two screens are the same, else a description, a mask of
    the changed pixels (None if they are not the same size) and the number
    of them that are more than a picture drawn one pixel off: neither the
    old pixel nor the new one is next to where it was."""
    if old.shape != new.shape:
        return ("size %dx%d -> %dx%d" % (old.shape[1], old.shape[0],
                                         new.shape[1], new.shape[0]), None, 1)
    if numpy.array_equal(old, new):
        return None
    delta = numpy.abs(old.astype(numpy.int16) - new.astype(numpy.int16))
    mask = delta.any(axis=2)
    rows = numpy.flatnonzero(mask.any(axis=1))
    columns = numpy.flatnonzero(mask.any(axis=0))
    real = int((mask & ~(found_nearby(new, old) & found_nearby(old, new)))
               .sum())
    return ("%d pixels in %d,%d-%d,%d, largest step %d, %s"
            % (mask.sum(), columns[0], rows[0], columns[-1] + 1,
               rows[-1] + 1, delta.max(),
               "%d of them more than a shift by one pixel" % real if real
               else "all of them a shift by one pixel"), mask, real)


def save_png(path, array):
    from PIL import Image
    Image.fromarray(array).save(path)


def montage(old, new, mask):
    """Old screen, new screen, and the new one greyed with the changed
    pixels in red (or nothing, if the sizes differ), side by side."""
    height = max(old.shape[0], new.shape[0])
    third = None
    if mask is not None:
        grey = new.mean(axis=2, keepdims=True) // 2 + 96
        third = numpy.repeat(grey, 3, axis=2).astype(numpy.uint8)
        third[mask] = (255, 0, 0)
    parts = [p for p in (old, new, third) if p is not None]
    width = sum(p.shape[1] for p in parts) + 8 * (len(parts) - 1)
    out = numpy.full((height, width, 3), 255, numpy.uint8)
    x = 0
    for part in parts:
        out[:part.shape[0], x:x + part.shape[1]] = part
        x += part.shape[1] + 8
    return out


def other_lines(trace):
    return [l for l in trace.split("\n") if not l.startswith(PICTURE_LINES)]


def compare_case(name, old_dir, new_dir, montages):
    """(name, number of screens, [description of each differing screen],
    how many of those are more than a shift by one pixel, whether the lines
    that are not about pictures are the same).  The screens that are more
    than a shift come first in the list, and get the montages."""
    with open(os.path.join(old_dir, name + ".trace")) as f:
        old_trace = f.read()
    with open(os.path.join(new_dir, name + ".trace")) as f:
        new_trace = f.read()
    old = screens(old_trace, Pictures(os.path.join(old_dir, "pictures")))
    new = screens(new_trace, Pictures(os.path.join(new_dir, "pictures")))
    count, real, shifted = 0, [], []
    for a, b in itertools.zip_longest(old, new):
        count += 1
        if a is None or b is None:
            real.append("screen %d: only in the %s run (%s)"
                        % (count, "old" if b is None else "new", (a or b)[0]))
            continue
        what = difference(a[1], b[1])
        if what is None:
            continue
        kind = real if what[2] else shifted
        kind.append("screen %d (old %s, new %s): %s"
                    % (count, a[0], b[0], what[0]))
        if montages and len(kind) <= MONTAGES_PER_CASE:
            os.makedirs(montages, exist_ok=True)
            save_png(os.path.join(montages, "%s-%04d%s.png"
                                  % (name, count, "" if what[2] else "-shift")),
                     montage(a[1], b[1], what[1]))
    return name, count, real + shifted, len(real), \
        other_lines(old_trace) == other_lines(new_trace)


def changed_cases(old_dir, new_dir, everything=False, patterns=()):
    names = []
    for entry in sorted(os.listdir(new_dir)):
        if not entry.endswith(".trace"):
            continue
        name = entry[:-len(".trace")]
        if patterns and not any(p in name for p in patterns):
            continue
        old_path = os.path.join(old_dir, entry)
        if not os.path.exists(old_path):
            continue
        if not everything:
            with open(old_path) as f, open(os.path.join(new_dir, entry)) as g:
                if f.read() == g.read():
                    continue
        names.append(name)
    return names


def compare(old_dir, new_dir, names, montages=None, jobs=8, out=sys.stdout):
    """Compare the screens of the cases NAMES; the number of cases with a
    screen that differs."""
    if montages is None:
        montages = os.path.join(new_dir, "montages")
    same = shifts = bad = 0
    with concurrent.futures.ProcessPoolExecutor(jobs) as pool:
        results = pool.map(compare_case, names, itertools.repeat(old_dir),
                           itertools.repeat(new_dir),
                           itertools.repeat(montages))
        for name, count, differing, real, others_same in results:
            text = "" if others_same else \
                "; trace lines that are not about pictures differ too"
            if not differing:
                same += 1
                print("SAME SCREENS    %s (%d)%s" % (name, count, text),
                      file=out)
                continue
            if real:
                bad += 1
                print("SCREENS DIFFER  %s: %d of %d%s%s"
                      % (name, real, count,
                         ", %d more shifted by a pixel"
                         % (len(differing) - real)
                         if len(differing) > real else "", text), file=out)
            else:
                shifts += 1
                print("SHIFTED PIXELS  %s: %d of %d%s"
                      % (name, len(differing), count, text), file=out)
            for line in differing[:MONTAGES_PER_CASE]:
                print("    " + line, file=out)
            if len(differing) > MONTAGES_PER_CASE:
                print("    ...", file=out)
    print("%d cases with other traces: %d look the same, %d have pictures "
          "shifted by a pixel, %d differ more" % (len(names), same, shifts, bad),
          file=out)
    if bad or shifts:
        print("montages in " + montages, file=out)
    return bad


def render(out_dir, name, to, every=1):
    with open(os.path.join(out_dir, name + ".trace")) as f:
        trace = f.read()
    os.makedirs(to, exist_ok=True)
    count = 0
    for count, (label, screen) in enumerate(
            screens(trace, Pictures(os.path.join(out_dir, "pictures"))), 1):
        if (count - 1) % every == 0:
            save_png(os.path.join(to, "%s-%04d.png" % (name, count)), screen)
    print("%d screens" % count)


def main():
    argv = sys.argv[1:]
    options = {"--montages": None, "--jobs": "8", "--every": "1"}
    everything = False
    plain = []
    while argv:
        arg = argv.pop(0)
        if arg in options:
            options[arg] = argv.pop(0)
        elif arg == "--all":
            everything = True
        elif arg.startswith("--"):
            sys.exit(__doc__)
        else:
            plain.append(arg)
    if len(plain) >= 3 and plain[0] == "compare":
        names = changed_cases(plain[1], plain[2], everything, plain[3:])
        return 1 if compare(plain[1], plain[2], names, options["--montages"],
                            int(options["--jobs"])) else 0
    if len(plain) == 4 and plain[0] == "render":
        render(plain[1], plain[2], plain[3], int(options["--every"]))
        return 0
    sys.exit(__doc__)


if __name__ == "__main__":
    sys.exit(main())
