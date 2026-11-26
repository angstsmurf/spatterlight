#!/usr/bin/env python3
"""Build the "Atari ST 8x8 2y" fonts in resources/fonts.

The glyphs are the 8x8 system font from EmuTOS (bios/fnt_st_8x8.c,
GPL-2.0-or-later), which Infocom's Atari ST interpreters show in medium
resolution (640x200). ST medium-res pixels are twice as tall as they are
wide, so each font pixel is 100x200 units, like the PxPlus "-2y" fonts:
at 16 points a cell is 8x16 screen pixels. The Regular style matches a
Hatari screenshot of an Infocom game (EmuTOS, medium res) pixel for pixel. Bold is GEM's thicken (smear right by one pixel), Italic
GEM's skew, both as Infocom's interpreter draws them through the VDI.

usage: make_atarist_font.py EMUTOS/bios/fnt_st_8x8.c ATARIST.TXT OUTDIR
  ATARIST.TXT: https://www.unicode.org/Public/MAPPINGS/VENDORS/MISC/ATARIST.TXT
  needs fontTools (pip install fonttools)
"""
import re, sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

FONT_C, MAPPING, OUTDIR = sys.argv[1:4]

src = open(FONT_C).read()
words = [int(w, 16) for w in
         re.findall(r'0x([0-9a-fA-F]{4})', src[src.index('dat_table[] ='):])]
data = b''.join(bytes([w >> 8, w & 255]) for w in words)
assert len(data) == 256 * 8, len(data)

# Infocom's font 3 (Beyond Zork's pictures, map, arrows and runes) as the
# Atari ST interpreter draws it, characters 0x20-0x7E, 8 rows each, top row
# first. Copied from the font table in Beyond Zork's ST interpreter (disk
# image beyond_zork.ST, offset 0x42320, 8 bytes per character from 0x00).
# The arrows, the runes BLARN and the room box were checked against Hatari,
# pixel for pixel. It becomes a font of its own, AtariST8x8-2y-Font3, which
# the Atari ST theme's windows use for font 3 (see below).
FONT3_TABLE = [
    '0000000000000000',  # 20 ' '
    '00002060ff602000',  # 21 '!'
    '00000406ff060400',  # 22 '"'
    '0102040810204080',  # 23 '#'
    '8040201008040201',  # 24 '$'
    '0000000000000000',  # 25 '%'
    '00000000ff000000',  # 26 '&'
    '000000ff00000000',  # 27 "'"
    '0808080808080808',  # 28 '('
    '1010101010101010',  # 29 ')'
    '080808ff00000000',  # 2a '*'
    '00000000ff080808',  # 2b '+'
    '080808080f080808',  # 2c ','
    '10101010f0101010',  # 2d '-'
    '101010101f000000',  # 2e '.'
    '0000001f10101010',  # 2f '/'
    '000000f808080808',  # 30 '0'
    '08080808f8000000',  # 31 '1'
    '101010101f204080',  # 32 '2'
    '8040201f10101010',  # 33 '3'
    '010204f808080808',  # 34 '4'
    '08080808f8040201',  # 35 '5'
    'ffffffffffffffff',  # 36 '6'
    'ffffffffff000000',  # 37 '7'
    '000000ffffffffff',  # 38 '8'
    'f8f8f8f8f8f8f8f8',  # 39 '9'
    '1f1f1f1f1f1f1f1f',  # 3a ':'
    '080808ffffffffff',  # 3b ';'
    'ffffffffff080808',  # 3c '<'
    'f8f8f8f8fff8f8f8',  # 3d '='
    '1f1f1f1fff1f1f1f',  # 3e '>'
    '1f1f1f1f1f000000',  # 3f '?'
    '0000001f1f1f1f1f',  # 40 '@'
    '000000f8f8f8f8f8',  # 41 'A'
    'f8f8f8f8f8000000',  # 42 'B'
    '1f1f1f1f1f204080',  # 43 'C'
    '8040201f1f1f1f1f',  # 44 'D'
    '010204f8f8f8f8f8',  # 45 'E'
    'f8f8f8f8f8040201',  # 46 'F'
    '0100000000000000',  # 47 'G'
    '0000000000000001',  # 48 'H'
    '0000000000000080',  # 49 'I'
    '8000000000000000',  # 4a 'J'
    'ff00000000000000',  # 4b 'K'
    '00000000000000ff',  # 4c 'L'
    '8080808080808080',  # 4d 'M'
    '0101010101010101',  # 4e 'N'
    '00ff00000000ff00',  # 4f 'O'
    '00ff80808080ff00',  # 50 'P'
    '00ffc0c0c0c0ff00',  # 51 'Q'
    '00ffe0e0e0e0ff00',  # 52 'R'
    '00fff0f0f0f0ff00',  # 53 'S'
    '00fff8f8f8f8ff00',  # 54 'T'
    '00fffcfcfcfcff00',  # 55 'U'
    '00fffefefefeff00',  # 56 'V'
    '00ffffffffffff00',  # 57 'W'
    '0001010101010100',  # 58 'X'
    '0080808080808000',  # 59 'Y'
    '8142241818244281',  # 5a 'Z'
    '08080808ff080808',  # 5b '['
    '081c3e0808080808',  # 5c '\\'
    '08080808083e1c08',  # 5d ']'
    '081c3e08083e1c08',  # 5e '^'
    'ff818181818181ff',  # 5f '_'
    '3844040810100010',  # 60 '`'
    '00c4a890c0a09080',  # 61 'a'
    '007048447c444870',  # 62 'b'
    '0018141210905030',  # 63 'c'
    '0082c6aa92aac682',  # 64 'd'
    '0082c6aa92828282',  # 65 'e'
    '0092a4c890a0c080',  # 66 'f'
    '0082446c926c4482',  # 67 'g'
    '00c4a494cca4948c',  # 68 'h'
    '0010101010101010',  # 69 'i'
    '0010385492543810',  # 6a 'j'
    '0040406050484444',  # 6b 'k'
    '0060504844404040',  # 6c 'l'
    '00c6aa92aac68282',  # 6d 'm'
    '0090503010181412',  # 6e 'n'
    '00c6aa92c4a89080',  # 6f 'o'
    '00404040444c5464',  # 70 'p'
    '00ba541010101010',  # 71 'q'
    '0070484448704844',  # 72 'r'
    '0080868a92a2c202',  # 73 's'
    '0038549210101010',  # 74 't'
    '0060504844444444',  # 75 'u'
    '0038442810284482',  # 76 'v'
    '0070484448704040',  # 77 'w'
    '0010101038549292',  # 78 'x'
    '00f098d4b29a9692',  # 79 'y'
    '003e40804020100e',  # 7a 'z'
    'f7e3c1f7f7f7f7f7',  # 7b '{'
    'f7f7f7f7f7c1e3f7',  # 7c '|'
    'f7e3c1f7f7c1e3f7',  # 7d '}'
    'c7bbfbf7efefffef',  # 7e '~'
]
FONT3_TABLE = [bytes.fromhex(h) for h in FONT3_TABLE]

def font3(ch):
    return list(FONT3_TABLE[ord(ch) - 0x20])

# The arrows and runes of font 3 at their Unicode stand-ins, so the normal
# fonts carry them too. Font3LayoutManager paints font 3 itself, but text
# copied out of Beyond Zork, and autosaves from before it, have these
# characters in the normal style.
FONT3_UNICODE = dict(zip('!"\\]abcdefghijklmnopqrstuvwxyz',
                         '←→↑↓ᚪᛒᛇᛞᛖᚠᚷᚻᛁᛄᛦᛚᛗᚾᚩᖾᚳᚱᛋᛏᚢᛠᚹᛉᚥᛟ'))
FONT3 = {ord(u): font3(ch) for ch, u in FONT3_UNICODE.items()}

def glyph(c):
    """The 8 rows of character c; the font form is 256 bytes wide. Codes
    above 255 are the Unicode values of FONT3."""
    if c > 255:
        return FONT3[c]
    return [data[r * 256 + c] for r in range(8)]

PX, PY, UPM = 100, 200, 1600
ASC, DESC = 7 * PY, PY          # baseline sits under row 6
SKEW = [3, 2, 2, 1, 1, 0, 0, -1] # GEM skew, as seen in Hatari for "No"

cmap_src = {}
for line in open(MAPPING):
    m = re.match(r'0x([0-9A-Fa-f]{2})\s+0x([0-9A-Fa-f]{4})', line)
    if m:
        c, u = int(m[1], 16), int(m[2], 16)
        if c >= 0x20 and u not in (0x7F,):
            cmap_src[c] = u

# ATARIST.TXT leaves 0x00-0x1F as control codes, but the ST font draws
# symbols there: outlined arrows at 0x01-0x04 and a tick at 0x08. The plain
# arrows are the font 3 ones above.
cmap_src.update({0x01: 0x21E7, 0x02: 0x21E9, 0x03: 0x21E8, 0x04: 0x21E6,
                 0x08: 0x2713})
cmap_src.update({u: u for u in FONT3})

def rows_for(bitmap, bold, italic):
    """Each row as a set of pixel columns (may spill outside 0..7)."""
    rows = []
    for r, bits in enumerate(bitmap):
        cols = {i for i in range(8) if bits & (0x80 >> i)}
        if bold: cols |= {i + 1 for i in cols}      # GEM thicken
        if italic: cols = {i + SKEW[r] for i in cols}  # lean right
        rows.append(cols)
    return rows

def draw(bitmap, bold=False, italic=False):
    pen = TTGlyphPen(None)
    for r, cols in enumerate(rows_for(bitmap, bold, italic)):
        top = ASC - r * PY
        cs = sorted(cols)
        while cs:
            i = j = cs.pop(0)
            while cs and cs[0] == j + 1: j = cs.pop(0)
            x0, x1 = i * PX, (j + 1) * PX
            pen.moveTo((x0, top - PY)); pen.lineTo((x0, top))
            pen.lineTo((x1, top)); pen.lineTo((x1, top - PY)); pen.closePath()
    return pen.glyph()

from fontTools.ttLib.tables.O_S_2f_2 import Panose
PAN = Panose(); PAN.bFamilyType = 2; PAN.bProportion = 9
STYLES = [('Regular', False, False), ('Bold', True, False),
          ('Italic', False, True), ('BoldItalic', True, True)]

def save(fam, ps, style, bold, italic, bitmaps, aliases, copyright):
    """bitmaps: {unicode: 8 rows}; aliases: {unicode: unicode drawn instead}."""
    order, glyphs, cmap = ['.notdef'], {}, {}
    glyphs['.notdef'] = TTGlyphPen(None).glyph()
    for u, bitmap in sorted(bitmaps.items()):
        n = 'uni%04X' % u
        order.append(n); glyphs[n] = draw(bitmap, bold, italic); cmap[u] = n
    for u, src in aliases.items():
        if u not in cmap: cmap[u] = 'uni%04X' % src
    fb = FontBuilder(UPM, isTTF=True)
    fb.setupGlyphOrder(order); fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics({n: (800, getattr(glyphs[n], 'xMin', 0) or 0) for n in order})
    fb.setupHorizontalHeader(ascent=ASC, descent=-DESC)
    sub = {'BoldItalic': 'Bold Italic'}.get(style, style)
    fb.setupNameTable({'familyName': fam, 'styleName': sub, 'psName': ps,
        'fullName': f'{fam} {sub}', 'uniqueFontIdentifier': ps,
        'version': 'Version 1.0', 'copyright': copyright,
        'licenseDescription': 'GPL version 2 or any later version'})
    fsSel = (0x20 if bold else 0) | (0x01 if italic else 0) or 0x40
    fb.setupOS2(sTypoAscender=ASC, sTypoDescender=-DESC, sTypoLineGap=0,
                usWinAscent=ASC, usWinDescent=DESC, fsSelection=fsSel,
                usWeightClass=700 if bold else 400, xAvgCharWidth=800,
                panose=PAN)
    fb.setupPost(isFixedPitch=1, italicAngle=-26.6 if italic else 0)
    fb.font['head'].macStyle = (1 if bold else 0) | (2 if italic else 0)
    fb.save(f'{OUTDIR}/{ps}.ttf')
    print(ps, len(order), 'glyphs')

# The text fonts. Extra aliases so typographic text still renders in the ST
# look.
text = {}
for c, u in cmap_src.items():
    text.setdefault(u, glyph(c))
for style, bold, italic in STYLES:
    save('Atari ST 8x8 2y', 'AtariST8x8-2y-' + style, style, bold, italic,
         text,
         {0xA0: 0x20, 0x2018: 0x27, 0x2019: 0x27, 0x201C: 0x22,
          0x201D: 0x22, 0x2013: 0x2D, 0x2014: 0x2D},
         'Glyphs from the EmuTOS 8x8 system font, (C) 2001-2002 The EmuTOS development team')
