#!/usr/bin/env python3
"""ADRIFT 4.0 fixtures for the game-derived style assignment
(glk/os_glk_styles.cpp).  Each is one room whose introduction and room
description carry the markup under test; LOOK reprints the room.

    styles_plain.taf     no markup at all: the natural table, today's hints,
                         every span Normal
    styles_intro.taf     <font size> only in the introduction (a centred
                         24pt title, a 16pt byline, 10pt small print), with a
                         <waitkey> after the title; play uses bold only
    styles_medium.taf    bold and 14pt text both used in play, which the
                         fixed precedence printed in the same style
    styles_overflow.taf  more states than there are styles, so the lookup
                         fallback is reached: a size steps toward 12 before
                         a flag is dropped, then italic, bold, the alignment
                         and the face go in that order

The census of all four is pinned in census_expected.txt (`make -f
Makefile.headless styles-check`); what the Spatterlight build prints them
in is checked by run_styles_tests.py.

The body is the positional 4.0 layout of harness/make_rich_text_taf.py, in
the same SCARE-readable envelope: signature, eight header bytes, zlib body.

Usage:
    python3 make_styles_tafs.py           (writes the four beside this file)
"""
from pathlib import Path
import zlib

HERE = Path(__file__).resolve().parent
SEP = "\xbd\xd0"
SIGNATURE = bytes(
    [0x3C, 0x42, 0x3F, 0xC9, 0x6A, 0x87, 0xC2,
     0xCF, 0x93, 0x45, 0x3E, 0x61, 0x39, 0xFA]
)

FIXTURES = {
    "styles_plain": (
        "A game with no markup in it at all.",
        "Plain Room",
        "A plain room, described in plain text. Nothing here is bold, "
        "slanted, centred or sized.",
    ),
    "styles_intro": (
        "<center><font size=24>The Big Title</font></center><br>"
        "<waitkey>"
        "<center><font size=16>by A. N. Author</font></center><br><br>"
        "<font size=10>Small print: this fixture is part of Scarier's test "
        "suite.</font><br><br>"
        "And so the story begins.",
        "Study",
        "A quiet study. The <b>desk</b> and the <b>chair</b> are the only "
        "furniture.",
    ),
    "styles_medium": (
        "Bold and medium-sized text, both in play.",
        "Gallery",
        "<b>Bold words</b> and <font size=14>medium words</font> share "
        "this gallery, with <b>more bold</b> and "
        "<font size=14>more medium text</font> after them.",
    ),
    "styles_overflow": (
        "<center><font size=20><b>Overflow</b></font></center><br>"
        "<right><i>an epigraph, right-aligned</i></right>",
        "Crowded Room",
        "".join([
            "<b>bold text here</b> ",
            "<i>italic text here</i> ",
            "<b><i>bold italic text</i></b> ",
            '<font face="Courier">courier text here</font> ',
            '<font face="Courier"><b>courier bold</b></font> ',
            "<br><center>centred text here</center><br>",
            "<center><b>centred bold text</b></center><br>",
            "<center><i>centred italic text</i></center><br>",
            "<right>right-aligned text</right><br>",
            "<right><b>right bold</b></right><br>",
            "<font size=14>fourteen point text</font> ",
            "<font size=16>sixteen point text</font> ",
            "<font size=10>ten point</font> ",
            "<font size=18>eighteen pt</font> ",
            "<font size=16><i>sixteen italic</i></font> ",
            "<font size=22><b>big bold</b></font> ",
            "<center><font size=14><i>ci14</i></font></center>",
        ]),
    ),
}


def build_body(intro: str, short: str, long: str) -> bytes:
    lines: list[str] = []

    def field(value: object) -> None:
        lines.append(str(value))

    def multiline(value: str) -> None:
        lines.append(value)
        lines.append(SEP)

    # HEADER: MStartupText #StartRoom MWinText
    multiline(intro)
    field(0)  # StartRoom (zero-based)
    multiline("You have won.")

    # GLOBAL
    field(short + " (styles fixture)")
    field("Scarier regression")
    field("I don't understand.")
    field(2)  # Perspective: second person
    field(0)  # ShowExits
    field(0)  # WaitTurns
    field(1)  # DispFirstRoom
    field(0)  # BattleSystem
    field(0)  # MaxScore
    field("Player")
    field(0)  # PromptName
    field("A test subject.")
    field(0)  # Task (zero means no AltDesc follows)
    field(0)  # Position
    field(0)  # ParentObject
    field(0)  # PlayerGender
    field(100)  # MaxSize
    field(100)  # MaxWt
    field(0)  # EightPointCompass: eight exits
    field(0)  # bNoDebug
    field(0)  # NoScoreNotify
    field(0)  # NoMap
    field(0)  # bNoAutoComplete
    field(0)  # bNoControlPanel
    field(0)  # bNoMouse
    field(0)  # Sound
    field(0)  # Graphics
    field(0)  # StatusBox
    field("")  # StatusBoxText
    field(3)  # SizeMultiple
    field(3)  # WeightMultiple
    field(1)  # Embedded

    # ROOMS
    field(1)
    field(short)
    field(long)
    for _ in range(8):
        field(0)  # absent ROOM_EXIT
    field(0)  # Alts count
    field(0)  # HideOnMap

    for _ in range(9):
        field(0)  # Objects .. ALRs, CustomFont
    field("2026")  # CompileDate
    return ("\r\n".join(lines) + "\r\n").encode("latin-1")


def main() -> None:
    for name, (intro, short, long) in FIXTURES.items():
        body = build_body(intro, short, long)
        taf = SIGNATURE + bytes(8) + zlib.compress(body, 9)
        out = HERE / (name + ".taf")
        out.write_bytes(taf)
        print(f"wrote {out.name}: {len(taf)} bytes")


if __name__ == "__main__":
    main()
