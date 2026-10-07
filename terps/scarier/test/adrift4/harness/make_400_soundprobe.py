#!/usr/bin/env python3
"""ADRIFT 4.0 sound probe: WAV, MP3 and MIDI resources embedded in the TAF.

One room.  The introduction plays a WAV; tasks play it again, an MP3, a
looping MIDI, and stop the sound:

    wav    Res = oof.wav, back-reference -1 to the intro's copy
    mp3    Res = oof.mp3
    midi   Res = variation_iv.mid## (a trailing "##" loops)
    stop   Res = "##" (stop), length 0

ADRIFT 4.0 has one sound channel, so each new sound replaces the last.

The file is laid out as the 4.0 Generator writes an "Embedded" game (checked
against the corpus, e.g. Church.taf): the 14-byte signature, eight ASCII
digits giving the resource base (one past the end of the zlib stream), the
zlib body, then each resource preceded by a 0x00 byte, in order of first
appearance.  A resource's first appearance carries its length, written
VB-style (" 10334 "); later ones carry "-N", entry N of that order.  SCARE's
offsets (parse_get_v400_resource_offset) are the resource base plus the sum
of the earlier resources' lengths plus one per earlier resource.

Media:
    oof.wav, oof.mp3   ../../adrift5/probes/: "Man oof.wav" by xtrgamr,
                       https://freesound.org/people/xtrgamr/sounds/257780/,
                       CC BY 4.0 (the MP3 is a conversion of it)
    variation_iv.mid   beside this script: "Variation IV" from "Concerto for
                       Two Pianos" by Igor Stravinsky, arranged by Wildbill
                       hitchcock, https://commons.wikimedia.org/wiki/
                       File:%22Variation_IV%22_from_%22Concerto_for_Two_Pianos
                       %22,_by_Igor_Stravinsky.mid, CC BY-SA 3.0 / GFDL 1.2+

Usage:
    python3 make_400_soundprobe.py [output.taf]     (default: sound_400.taf)
"""
from pathlib import Path
import sys
import zlib

HERE = Path(__file__).resolve().parent
A5_PROBES = HERE.parent.parent / "adrift5" / "probes"

SEP = "\xbd\xd0"
SIGNATURE = bytes(
    [0x3C, 0x42, 0x3F, 0xC9, 0x6A, 0x87, 0xC2,
     0xCF, 0x93, 0x45, 0x3E, 0x61, 0x39, 0xFA]
)

AUTHOR_DIR = "C:\\Program Files\\ADRIFT\\Sound Test\\"
WAV = (AUTHOR_DIR + "oof.wav", A5_PROBES / "oof.wav")
MP3 = (AUTHOR_DIR + "oof.mp3", A5_PROBES / "oof.mp3")
MID = (AUTHOR_DIR + "variation_iv.mid", HERE / "variation_iv.mid")
STOP = "##"

L: list[str] = []
embedded: list[tuple[str, bytes]] = []


def s(x: object) -> None:
    L.append(str(x))


def ml(x: str) -> None:
    L.append(x)
    L.append(SEP)


def vb_int(n: int) -> str:
    """An integer as VB's Str() writes it: a leading space for the sign of a
    non-negative number, and the Generator's trailing space."""
    return (f" {n}" if n >= 0 else str(n)) + " "


def res(sound: tuple[str, Path] | str | None, loop: bool = False) -> None:
    """A RESOURCE with Sound on and Graphics off: $SoundFile #SoundLen."""
    if sound is None:
        s("")
        s(0)
        return
    if sound == STOP:
        s(STOP)
        s(0)
        return
    name, path = sound
    s(name + ("##" if loop else ""))
    for index, (known, _) in enumerate(embedded, start=1):
        if known == name:
            s(vb_int(-index))
            return
    data = path.read_bytes()
    embedded.append((name, data))
    s(vb_int(len(data)))


def task(command: str, text: str, sound, loop: bool = False) -> None:
    s(1)
    s(command)               # V$Command
    s(text)                  # CompleteText
    s("")                    # ReverseMessage
    s("")                    # RepeatText
    s("")                    # AdditionalMessage
    s(0)                     # ShowRoomDesc
    s(1)                     # Repeatable
    s(0)                     # Reversible
    s(0)                     # V$ReverseCommand count
    s(3)                     # Where: all rooms
    s("")                    # Question
    s(0)                     # Restrictions
    s(0)                     # Actions
    s("")                    # RestrMask
    res(sound, loop)         # Res


# HEADER
ml("Welcome to the Sound Test.<br><br>"
   "You should hear a man say \"oof\" (WAV).")
s(0)                         # StartRoom (0-based)
ml("You have won the sound probe.")

# GLOBAL
s("Sound Test 4.00")
s("SCARE regression")
s("I don't understand.")
s(2)                         # Perspective: second person
s(0)                         # ShowExits
s(0)                         # WaitTurns
s(1)                         # DispFirstRoom
s(0)                         # BattleSystem
s(0)                         # MaxScore
s("Player")
s(0)                         # PromptName
s("A test subject.")
s(0); s(0); s(0); s(0)       # Task, Position, ParentObject, PlayerGender
s(100); s(100)               # MaxSize, MaxWt
s(0)                         # EightPointCompass
s(0)                         # bNoDebug
s(0)                         # NoScoreNotify
s(0)                         # NoMap
s(0)                         # bNoAutoComplete
s(0)                         # bNoControlPanel
s(0)                         # bNoMouse
s(1)                         # Sound
s(0)                         # Graphics
res(WAV)                     # IntroRes
res(None)                    # WinRes
s(0)                         # StatusBox
s("")                        # StatusBoxText
s(3); s(3)                   # SizeMultiple, WeightMultiple
s(1)                         # Embedded

# ROOMS
s(1)
s("Studio")
s("A quiet room for testing sounds. Type wav, mp3, midi or stop.")
for _ in range(8):
    s(0)                     # absent ROOM_EXIT
res(None)                    # Res
s(0)                         # Alts count
s(0)                         # HideOnMap

# OBJECTS
s(0)

# TASKS
s(4)
task("wav", "Playing the WAV again.", WAV)
task("mp3", "Playing the MP3.", MP3)
task("midi", "Looping the MIDI. Type stop to stop it.", MID, loop=True)
task("stop", "Stopping the sound.", STOP)

# EVENTS, NPCS, tail
s(0)                         # Events
s(0)                         # NPCs
s(0)                         # RoomGroups
s(0)                         # Synonyms
s(0)                         # Variables
s(0)                         # ALRs
s(0)                         # CustomFont
s("2026")                    # CompileDate


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "sound_400.taf"
    body = ("\r\n".join(L) + "\r\n").encode("latin-1")
    compressed = zlib.compress(body, 9)
    resource_base = len(SIGNATURE) + 8 + len(compressed) + 1
    taf = bytearray(SIGNATURE + b"%08d" % resource_base + compressed)
    for _, data in embedded:
        taf += b"\0" + data
    out.write_bytes(bytes(taf))
    offset = resource_base
    for name, data in embedded:
        print(f"  {name.rsplit(chr(92), 1)[-1]}: offset {offset}, length {len(data)}")
        offset += len(data) + 1
    print(f"wrote {out}: {len(taf)} bytes ({len(body)}-byte plain body, "
          f"{len(embedded)} embedded resources)")


if __name__ == "__main__":
    main()
