#!/usr/bin/env python3
"""Generate comparable ADRIFT 4.0 and 3.9 rich-text/color probes.

The old TAF formats are positional CRLF records rather than Adventure XML.
Both generated games contain one room; LOOK reprints the sample.

Usage:
    python3 make_rich_text_taf.py
    python3 make_rich_text_taf.py 400 [output.taf]
    python3 make_rich_text_taf.py 390 [output.taf]
"""

from pathlib import Path
import sys
import zlib


HERE = Path(__file__).resolve().parent
V400_SEP = "\xbd\xd0"

V400_SIGNATURE = bytes(
    [0x3C, 0x42, 0x3F, 0xC9, 0x6A, 0x87, 0xC2,
     0xCF, 0x93, 0x45, 0x3E, 0x61, 0x39, 0xFA]
)
V390_SIGNATURE = bytes(
    [0x3C, 0x42, 0x3F, 0xC9, 0x6A, 0x87, 0xC2,
     0xCF, 0x94, 0x45, 0x37, 0x61, 0x39, 0xFA]
)


# A $Long field is one physical TAF line, so visible line breaks use <br>.
# Probe both British and American spellings because old ADRIFT content uses
# both even though the Generator UI generally writes "colour".
RICH_TEXT = "".join(
    [
        '<bgcolour="black">',
        "<b>Typography Lab</b><br>",
        "  -- Basic styles --<br>",
        "Normal text.<br>",
        "<b>Bold text.</b><br>",
        "<i>Italic text.</i><br>",
        "<u>Underlined text.</u><br>",
        "<b><i>Bold italic.</i></b><br>",
        "<b><u>Bold underline.</u></b><br>",
        "<i><u>Italic underline.</u></i><br>",
        "<c>Command / secondary colour (c).</c><br><br>",
        "  -- Alignment --<br>",
        "Left-aligned paragraph.<br>",
        "<center>Centered paragraph (center).</center><br>",
        "<centre>Centred paragraph (centre).</centre><br>",
        "<right>Right-aligned paragraph.</right><br><br>",
        "  -- Named colours (colour=) --<br>",
        '<font colour="red">red</font> ',
        '<font colour="blue">blue</font> ',
        '<font colour="green">green</font> ',
        '<font colour="purple">purple</font> ',
        '<font colour="orange">orange</font> ',
        '<font colour="teal">teal</font><br><br>',
        "  -- American spelling (color=) --<br>",
        '<font color="red">red</font> ',
        '<font color="blue">blue</font> ',
        '<font color="green">green</font><br><br>',
        "  -- Hex colours --<br>",
        '<font colour="#ff0000">#ff0000</font> ',
        '<font colour="#00aa00">#00aa00</font> ',
        '<font colour="#0066cc">#0066cc</font> ',
        '<font colour="ff00ff">ff00ff (no hash)</font><br><br>',
        "  -- Combined --<br>",
        '<font face="Courier" size=16 colour="purple">',
        "Courier, larger, purple.</font><br>",
        '<font face="Arial" colour="yellow"><b>',
        "Arial bold yellow (bold inside font).</b></font><br>",
        '<b><font face="Courier" colour="cyan">',
        "Courier bold cyan (font inside bold).</font></b><br>",
        '<center><font face="Times New Roman" size=16 colour="maroon"><i>',
        "Centered Times italic maroon.</i></font></center><br>",
        "<right><i>Right-aligned italic.</i></right><br><br>",
        "Type LOOK to reprint this sample.",
    ]
)


def build_body(version: int) -> bytes:
    """Build the uncompressed/deobfuscated positional TAF body."""
    if version not in (390, 400):
        raise ValueError(f"unsupported ADRIFT version: {version}")

    v390 = version == 390
    lines: list[str] = []

    def field(value: object) -> None:
        lines.append(str(value))

    def multiline(value: str) -> None:
        lines.append(value)
        lines.append("**" if v390 else V400_SEP)

    # HEADER: MStartupText #StartRoom MWinText
    multiline(
        "This game exercises ADRIFT 3/4 rich-text markup. "
        "Type LOOK to reprint the sample."
    )
    field(0)  # StartRoom (zero-based)
    multiline("You have won the rich-text probe.")

    # GLOBAL
    field(f"Rich Text Test {version // 100}.{version % 100:02d}")
    field("SCARE regression")
    field("I don't understand.")
    field(2)  # Perspective: second person
    field(0)  # ShowExits
    field(0)  # WaitTurns
    field(1)  # DispFirstRoom
    field(0)  # BattleSystem
    field(0)  # MaxScore
    field("Player")
    field(0)  # PromptName
    field("A typography test subject.")
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
    # IntroRes / WinRes contain no fields while Sound and Graphics are false.
    if v390:
        # FStatusBox / EStatusBoxText consume no lines in 3.9.
        field(3)  # SizeMultiple
        field(3)  # WeightMultiple
        # FEmbedded consumes no line in 3.9.
    else:
        field(0)  # StatusBox
        field("")  # StatusBoxText
        field(3)  # SizeMultiple
        field(3)  # WeightMultiple
        field(1)  # Embedded

    # ROOMS
    field(1)
    field("Typography Lab")  # Short
    field(RICH_TEXT)  # Long
    if v390:
        field("")  # LastDesc
    for _ in range(8):
        field(0)  # absent ROOM_EXIT
    if v390:
        field("")  # AddDesc1
        field(0)  # Task1
        field("")  # AddDesc2
        field(0)  # Task2
        field(0)  # Obj
        field("")  # AltDesc
        field(0)  # TypeHideObjects
        # Res, LastRes, Task1Res, Task2Res and AltRes contain no fields.
        field(0)  # HideOnMap
    else:
        # Res contains no fields; room has no alternatives.
        field(0)  # Alts count
        field(0)  # HideOnMap

    # Empty collections and tail.
    field(0)  # Objects
    field(0)  # Tasks
    field(0)  # Events
    field(0)  # NPCs
    field(0)  # RoomGroups
    field(0)  # Synonyms
    field(0)  # Variables
    field(0)  # ALRs
    field(0)  # CustomFont
    field("2026")  # CompileDate
    if v390:
        # The 3.9 Runner validates Mid(password, 5, 4) == "Wild".
        field("    Wild    ")

    return ("\r\n".join(lines) + "\r\n").encode("latin-1")


def vb_random_bytes(data: bytes) -> bytes:
    """Apply the ADRIFT 3.9 VB6-PRNG XOR stream."""
    state = 0x00A09E86

    def draw() -> int:
        nonlocal state
        state = (state * 0x43FD43FD + 0x00C39EC3) & 0x00FFFFFF
        return (255 * state) // 0x1000000

    # The signature accounts for the first fourteen draws.
    for _ in range(14):
        draw()
    return bytes(byte ^ draw() for byte in data)


def generate(version: int, output: Path) -> None:
    body = build_body(version)
    if version == 400:
        # This is the same SCARE-readable v4 envelope used by
        # make_battle_taf.py: signature, eight header bytes, zlib body.
        taf = V400_SIGNATURE + bytes(8) + zlib.compress(body, 9)
    else:
        taf = V390_SIGNATURE + vb_random_bytes(body)
    output.write_bytes(taf)
    print(
        f"wrote {output}: {len(taf)} bytes "
        f"({len(body)}-byte plain body, ADRIFT {version / 100:.2f})"
    )


def main() -> None:
    if len(sys.argv) == 1:
        generate(400, HERE / "rich_text_400.taf")
        generate(390, HERE / "rich_text_390.taf")
        return

    aliases = {"4": 400, "40": 400, "400": 400,
               "3": 390, "39": 390, "390": 390}
    try:
        version = aliases[sys.argv[1]]
    except KeyError:
        raise SystemExit("version must be 400 or 390")
    default = HERE / f"rich_text_{version}.taf"
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else default
    generate(version, output)


if __name__ == "__main__":
    main()
