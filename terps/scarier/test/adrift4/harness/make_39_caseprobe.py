#!/usr/bin/env python3
"""ADRIFT 3.9 twin of make_400_burnprobe.py's last two rounds: is %object%
case-sensitive one Runner down?

4.0 was measured 2026-08-25: it Replace()s the object's Short or Alias into a
`%object%` task command VERBATIM and compares the result to the lower-cased
input, so a capitalised Short can never bind (see make_400_burnprobe.py, and
the FIXED section in notes/WINE-TRANSCRIPTS-TODO.md).  The port is gated at
TAF_VERSION_400 because 3.90 implements the substitution somewhere else
entirely -- run390 Form1.frm:13991ff, through c() plus the seen byte at
.global_44, REWRITING the task command in place with the substituted name --
and nothing about that path had been measured.  run370 and run380 have no
"%object%" literal at all, so 3.90 is the only other Runner that can answer.

This is the 4.0 round-three/round-four cell table in the 3.90 file layout, so
the two transcripts can be read side by side:

    task `PX %object%`  vs Short `coin`          px coin / PX coin
    task `pa %object%`  vs Short `Widget`        pa widget / pa Widget
    task `pa %object%`  vs Short `brass key`     pa brass key / pa key /
                        behind Prefix `a small`  pa a brass key /
                                                 pa the brass key /
                                                 pa small brass key
    task `pa %object%`  vs Short `gem`,          pa gem / pa jewel /
                        Alias `jewel`            pa a gem / pa the gem
    task `PY`  (literal)                         py / PY
    task `PZ *` (wildcard)                       pz coin / PZ coin

MEASURED 2026-08-25, Adrift_1_p39case.txt, all 19 commands echoed.  The answer
is *both*: 3.90 binds just as strictly as 4.0 -- `pa a gem`, `pa the gem`,
`pa a brass key`, `pa the brass key` and `pa small brass key` are all refused,
and `pa key` does not even parse -- but it DOES fold case, so `pa Widget` and
`pa widget` both run where 4.0 refuses both.  So the port is gated at
TAF_VERSION_390 for the binding and at TAF_VERSION_400 for the case
sensitivity.  Scarier had five of these rows wrong (all five of the article /
Prefix rows) and no golden moved when they were fixed.

Every object is dynamic and loose so the room listing SEES it: an unlisted
object is not referenceable at all, and that gate would swallow the answer
(notes/WINE-TRANSCRIPTS-TODO.md, the object seen model).  `probe` is the
control: it must answer "PROBE OK." or the file is not wired.

The 3.9 object record carries ONE alias slot ([1]$Alias, sctafpar.cpp:214),
not 4.0's counted V$Alias array, which is why `gem` gets exactly one.

Usage:   python3 make_39_caseprobe.py [out.taf]
Session: sh measure.sh p39CASE.taf cmdfile_p39case.txt run390.exe
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 object-name case probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL -- byte for byte the shape make_39_examprobe.py's file loaded with.
s("Probe 39CASE")        # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective (second person)
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test subject.")     # PlayerDesc
s(0)                     # Task
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(102)                   # MaxSize
s(102)                   # MaxWt
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(3)                     # SizeMultiple
s(3)                     # WeightMultiple

# ROOMS -- one is enough; nothing here moves.
def room(short, long_, exits=()):
    s(short); s(long_); s("")
    for slot in range(8):
        dest = dict(exits).get(slot)
        if dest is None:
            s(0)
        else:
            s(dest); s(0); s(0)
    s(""); s(0); s(""); s(0); s(0); s(""); s(0)   # the 3.9 inline Alts block
    s(0)                                          # HideOnMap

s(1)
room("Alpha", "The first room.")

# OBJECTS
def obj(prefix, short, alias, desc="A trinket."):
    s(prefix); s(short); s(alias)
    s(0)                             # Static
    s(desc)                          # Description
    s(4)                             # InitialPosition: 4 + room 0
    s(0); s(0); s("")                # Task TaskNotDone AltDesc
    s(0); s(0); s(0)                 # Container Surface Capacity
    s(0); s(0); s(0)                 # Wearable SizeWeight Parent
    s(0)                             # Openable
    s(0); s(0); s(0); s(0)           # SitLie Edible Readable Weapon

s(4)
obj("a", "coin", "")
obj("a", "Widget", "")               # a CAPITALISED Short
obj("a small", "brass key", "")      # two-word Short behind a two-word Prefix
obj("a", "gem", "jewel")             # one alias

# TASKS
def task(cmd, text):
    s(0); s(cmd)         # W$Command: count 0 -> one command
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions

s(5)
task("PX %object%", "PX PASS.")
task("pa %object%", "PA PASS.")
task("PY",          "PY PASS.")
task("PZ *",        "PZ PASS.")
task("probe",       "PROBE OK.")

# EVENTS / NPCS
s(0)
s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate
s("    Wild    ")        # sPassword

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p39CASE.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
