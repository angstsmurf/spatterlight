#!/usr/bin/env python3
"""Fit the 16 display colours of the Atari 8-bit S.A.G.A. vector pictures.

The games show two 4-colour screens on alternate frames and leave the mixing
to the eye. The only statement of what the mixes should look like is the
colour test picture (image 250, TESTP. on the S.A.G.E. disk): six blocks the
engine labels WHITE BROWN ORANGE YELLOW BLUE GREEN, with the instruction
"Adjust TV to match above colors. Use Contrast, Picture, Bright, & Color
controls to remove any flickering!"

This script does what the player was told to do, numerically. The six colour
registers the games load are unknowns; the rules are:

  * A pixel's colour is the linear-light mean of its two frames. The eye
    integrates flicker linearly above the fusion rate (Talbot-Plateau), and
    the second frame of a pair is as long as the first.
  * The six labelled blocks should land on their names: the CIELAB hue of a
    named colour, with enough chroma to deserve the name, and near-zero
    chroma for white.
  * Registers with the same GTIA luminance nibble start out equally bright,
    but a TV with the Color knob turned up clips, and that is the only way
    a white, a blue, a yellow and a green can all come out of four
    registers (the mix rule makes GREEN = BLUE + YELLOW - WHITE). So equal
    luma is a weak prior, not a rule; the lum-2 registers (orange) must
    stay darker than the lum-4 ones.
  * The GTIA generates hues in a fixed order around the colour circle, so
    the registers' hues are asked to keep that order, with the two lum-2
    registers (hues 3 and 4) one step apart.

Everything else (the ten mixes the test does not show) follows from the
solved registers.

Usage: a8_palette_fit.py [--table]   (--table prints the C initializer)
"""

import math
import random
import sys

# The colour registers all four vector games load and never change. The
# $9000 plane (SCREEN1 in the S.A.G.E. source) shows COLPF0-2 = REGS_9000 for
# pixel values 1-3, the $A000 plane (SCREEN2) shows REGS_A000; 0 is black on
# both. A 4-bit pixel value is SCREEN2 * 4 + SCREEN1.
REGS_9000 = [0x00, 0x42, 0xB4, 0xE4]
REGS_A000 = [0x00, 0x32, 0x84, 0xD4]
REGISTERS = [0x42, 0xB4, 0xE4, 0x32, 0x84, 0xD4]  # the six non-black ones

# The colour test blocks: pixel value (or the two values of a checkerboard
# dither) and the name the engine prints under it.
TEST_BLOCKS = [
    ((11,), "WHITE"),
    ((5, 15), "BROWN"),
    ((5,), "ORANGE"),
    ((15,), "YELLOW"),
    ((10,), "BLUE"),
    ((14,), "GREEN"),
]

# What the names mean: the range of CIELAB hue angles a colour may have and
# still be called that, and the least chroma that deserves the name. The
# ranges are wide on purpose. BLUE reaches from violet-blue (sRGB #0055ff,
# 297) down to azure (#2080e0, 278); GREEN from pure green (136) to a
# sea green (#3fa06e, 156); BROWN from chocolate (#8b4513, 57) to tan
# (#c08020, 73).
TARGET_HUE = {
    "BROWN": (50.0, 75.0),
    "ORANGE": (50.0, 70.0),
    "YELLOW": (95.0, 106.0),
    "BLUE": (265.0, 300.0),
    "GREEN": (130.0, 160.0),
}
MIN_CHROMA = {"BROWN": 35.0, "ORANGE": 55.0, "YELLOW": 55.0, "BLUE": 45.0, "GREEN": 50.0}
WHITE_L = 80.0  # lightness of the white block
ORANGE_L = 60.0  # lightness of the orange block
LUMA_TOLERANCE = 0.25  # how far same-luminance registers may drift apart


# ---- colour maths -----------------------------------------------------------

def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(l):
    l = min(max(l, 0.0), 1.0)
    return 12.92 * l if l <= 0.0031308 else 1.055 * l ** (1 / 2.4) - 0.055


def luma(rgb):
    r, g, b = rgb
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def linear_to_lab(rgb):
    r, g, b = rgb
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = luma(rgb)
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116

    fx, fy, fz = f(x), f(y), f(z)
    return 116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)


def hue_chroma(rgb):
    _, a, b = linear_to_lab(rgb)
    return math.degrees(math.atan2(b, a)) % 360, math.hypot(a, b)


def hue_diff(h1, h2):
    d = (h1 - h2) % 360
    return d if d <= 180 else 360 - d


def srgb_int_linear(v):
    return tuple(srgb_to_linear(((v >> s) & 255) / 255) for s in (16, 8, 0))


def hue_miss(hue, lo_hi):
    """Degrees by which a hue falls outside its allowed range."""
    lo, hi = lo_hi
    if (hue - lo) % 360 <= (hi - lo) % 360:
        return 0.0
    return min(hue_diff(hue, lo), hue_diff(hue, hi))


# ---- the model: six registers, each a free linear RGB colour -----------------

def register_rgb(params, reg):
    """Linear RGB of a register from the 18 fit parameters, clipped to 0..1."""
    i = 3 * REGISTERS.index(reg)
    return tuple(min(max(c, 0.0), 1.0) for c in params[i:i + 3])


def register_overdrive(params, reg):
    """How far a register's raw parameters sit outside the displayable range."""
    i = 3 * REGISTERS.index(reg)
    return sum(max(0.0, c - 1.0) + max(0.0, -c) for c in params[i:i + 3])


def pixel_rgb(params, pixel):
    """A pixel value's colour: the linear mean of its two frames."""
    frames = []
    for reg in (REGS_9000[pixel & 3], REGS_A000[pixel >> 2]):
        frames.append((0.0, 0.0, 0.0) if reg == 0 else register_rgb(params, reg))
    return tuple((a + b) / 2 for a, b in zip(*frames))


def block_rgb(params, pixels):
    cols = [pixel_rgb(params, p) for p in pixels]
    return tuple(sum(c[i] for c in cols) / len(cols) for i in range(3))


def palette_srgb(params):
    out = []
    for p in range(16):
        r, g, b = (round(255 * linear_to_srgb(c)) for c in pixel_rgb(params, p))
        out.append(r << 16 | g << 8 | b)
    return out


# ---- the objective ----------------------------------------------------------

def cost(params):
    total = 0.0
    for pixels, name in TEST_BLOCKS:
        rgb = block_rgb(params, pixels)
        lightness = linear_to_lab(rgb)[0]
        hue, chroma = hue_chroma(rgb)
        if name == "WHITE":
            total += (chroma / 3) ** 2 + ((lightness - WHITE_L) / 3) ** 2
            continue
        if name == "ORANGE":
            total += ((lightness - ORANGE_L) / 5) ** 2
        total += (hue_miss(hue, TARGET_HUE[name]) / 3) ** 2
        if chroma < MIN_CHROMA[name]:
            total += ((MIN_CHROMA[name] - chroma) / 3) ** 2
    # Keep the parameters inside the displayable range (they are clipped
    # anyway; this just stops them wandering).
    for reg in REGISTERS:
        total += 10 * register_overdrive(params, reg) ** 2
    # Luma prior: same GTIA luminance, roughly the same brightness; the
    # lum-2 registers darker than the lum-4 ones.
    bright = [luma(register_rgb(params, reg)) for reg in (0xB4, 0xE4, 0x84, 0xD4)]
    dark = [luma(register_rgb(params, reg)) for reg in (0x42, 0x32)]
    for group in (bright, dark):
        mean = sum(group) / len(group)
        for y in group:
            total += (max(0.0, abs(y - mean) - LUMA_TOLERANCE) / 0.05) ** 2
    total += (max(0.0, max(dark) - min(bright)) / 0.05) ** 2
    # GTIA hue order. Hues 8, 11, 13, 14 (the lum-4 registers) run blue,
    # blue-green, yellow-green, yellow: a falling CIELAB hue angle, with
    # gaps of 3, 2 and 1 steps. Hue 3 (red-orange) is one step before hue 4
    # (red) in the same sense. Only the direction is enforced; the step
    # size is a weak preference.
    hues = {reg: hue_chroma(register_rgb(params, reg))[0] for reg in REGISTERS}
    gaps = [((hues[0x84] - hues[0xB4]) % 360, 3), ((hues[0xB4] - hues[0xD4]) % 360, 2),
            ((hues[0xD4] - hues[0xE4]) % 360, 1), ((hues[0x32] - hues[0x42]) % 360, 1)]
    for gap, steps in gaps:
        if gap > 180:  # wrong way round
            total += ((gap - 180) / 10) ** 2
        total += ((gap - 25 * steps) / 40) ** 2
    return total


def nelder_mead(f, x0, scale, iters=6000):
    n = len(x0)
    pts = [list(x0)]
    for i in range(n):
        p = list(x0)
        p[i] += scale
        pts.append(p)
    vals = [f(p) for p in pts]
    for _ in range(iters):
        order = sorted(range(n + 1), key=lambda i: vals[i])
        pts = [pts[i] for i in order]
        vals = [vals[i] for i in order]
        if vals[-1] - vals[0] < 1e-10:
            break
        centroid = [sum(p[i] for p in pts[:-1]) / n for i in range(n)]
        worst = pts[-1]
        refl = [c + (c - w) for c, w in zip(centroid, worst)]
        fr = f(refl)
        if fr < vals[0]:
            exp = [c + 2 * (c - w) for c, w in zip(centroid, worst)]
            fe = f(exp)
            pts[-1], vals[-1] = (exp, fe) if fe < fr else (refl, fr)
        elif fr < vals[-2]:
            pts[-1], vals[-1] = refl, fr
        else:
            con = [c + 0.5 * (w - c) for c, w in zip(centroid, worst)]
            fc = f(con)
            if fc < vals[-1]:
                pts[-1], vals[-1] = con, fc
            else:
                best = pts[0]
                pts = [best] + [[b + 0.5 * (p - b) for b, p in zip(best, q)] for q in pts[1:]]
                vals = [vals[0]] + [f(p) for p in pts[1:]]
    i = min(range(n + 1), key=lambda i: vals[i])
    return pts[i], vals[i]


def fit(seed=1, restarts=40):
    rng = random.Random(seed)
    best = None
    for _ in range(restarts):
        x0 = [rng.uniform(0.0, 1.0) for _ in range(18)]
        x, v = nelder_mead(cost, x0, 0.15)
        x, v = nelder_mead(cost, x, 0.03)
        if best is None or v < best[1]:
            best = (x, v)
    return best


def describe(params):
    print(f"cost {cost(params):.3f}")
    print("register  linear rgb            srgb     L*    C*   hue")
    for reg in REGISTERS:
        rgb = register_rgb(params, reg)
        lab = linear_to_lab(rgb)
        hue, chroma = hue_chroma(rgb)
        s = "".join(f"{round(255 * linear_to_srgb(c)):02x}" for c in rgb)
        print(f"  ${reg:02x}    ({rgb[0]:.2f}, {rgb[1]:.2f}, {rgb[2]:.2f})   #{s}  {lab[0]:5.1f} {chroma:5.1f} {hue:5.1f}")
    print("block            fused    L*    C*   hue  target")
    for pixels, name in TEST_BLOCKS:
        rgb = block_rgb(params, pixels)
        lab = linear_to_lab(rgb)
        hue, chroma = hue_chroma(rgb)
        s = "".join(f"{round(255 * linear_to_srgb(c)):02x}" for c in rgb)
        t = "%3.0f-%3.0f" % TARGET_HUE[name] if name in TARGET_HUE else "  grey"
        print(f"  {name:7s} {str(pixels):8s} #{s}  {lab[0]:5.1f} {chroma:5.1f} {hue:5.1f}  {t}")
    print()
    for p, v in enumerate(palette_srgb(params)):
        print(f"{p:2d}: #{v:06x}")


def c_table(pal):
    names = {pixels[0]: name.lower() for pixels, name in TEST_BLOCKS if len(pixels) == 1}
    lines = []
    for p, v in enumerate(pal):
        same = [q for q in range(16) if pal[q] == v and q != p]
        label = names.get(p, "")
        if same:
            label += (", " if label else "") + f"same as {same[0]}"
        lines.append(f"    /* {p:2d}: SCREEN1 {p & 3} SCREEN2 {p >> 2}  {label} */ 0x{v:06x},")
    return "\n".join(lines)


if __name__ == "__main__":
    params, _ = fit()
    if "--table" in sys.argv:
        print(c_table(palette_srgb(params)))
    else:
        describe(params)
        print(",".join(f"{v:06x}" for v in palette_srgb(params)))
