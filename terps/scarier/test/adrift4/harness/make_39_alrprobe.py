#!/usr/bin/env python3
"""ADRIFT 3.9 twin of make_400_alrprobe.py: how many times does the pre-4.0
Runner apply an ALR?

make_400_alrprobe.py measured run400: the ALR list is walked in
length-descending order, replacing ALL occurrences, and the whole walk happens
TWICE -- `AAA -> qAAA` on "AAA." yields "qqAAA.".  run390's listing looks
different (a single `For i = 0 To Me(188)-1 : If InStr(text, p(0)) > 0 Then
text = Replace(text, p(0), p(4), 1, -1, 0)` loop at the tail of the output
filter Proc_2_28_45CBD0, run390_3.bas:55465), which would give ONE "q".  A
version split is only worth porting if it is measured, so this is the same
eight cells in a 3.90 file, to be run under run390.exe.

    alpha   "AAA."     ALR  AAA -> qAAA            count the q's = pass count
    beta    "BBB."     ALRs BBB -> CCC, CCC -> DDD    chain within one pass
    gamma   "ZZ ZZ."   ALR  ZZ -> z                   all occurrences, or one
    delta   "EEE."     ALR  EEE -> EEE EEE          count the EEEs (2 = once)
    epsilon "RRR."     ALRs PPPP -> QQ, RRR -> PPPP  "QQ." = a second walk
                                                     "PPPP." = only one walk
    zeta    "MMM."     ALRs MM -> short, MMM -> long   "long." = length sort
    eta     "UUU."     ALRs WWWWW -> done, VVVV -> WWWWW, UUU -> VVVV
                                                     three-deep backward chain
    ping    control, proves the probe is wired

MEASURED 2026-08-24, run390.exe in Wine, beside the run400.exe readings of the
same eight cells (make_400_alrprobe.py):

    cell     run400              run390
    alpha    qqAAA.              qAAA.
    beta     DDD.                DDD.
    gamma    z z.                z z.
    delta    EEE EEE EEE EEE.    EEE EEE.
    epsilon  QQ.                 PPPP.
    zeta     long.               long.
    eta      done.               VVVV.

So the listing reads true: version 3.9 makes exactly ONE plain walk of the ALR
list, in length-descending order, replacing every occurrence of each original.
A chain only runs in the direction of the walk -- "eta" stops one hop in, at
VVVV, because WWWWW was already behind the cursor when UUU produced it -- and a
self-containing ALR fires exactly once.  Version 4.0 instead repeats the walk
until a pass changes nothing, and repeats the whole thing once per task that
completes in the turn; see pf_filter_internal() and pf_refilter() in
scprintf.cpp, and make_400_alrsrcprobe.py for which text catches which walk.

Versions 3.8 and 3.7 cannot reach any of this -- neither schema in sctafpar.cpp
carries an ALRs section, so those games have no ALRs at all.

Usage: python3 make_39_alrprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("ALR probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("ALR Probe 39")        # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test subject.")     # PlayerDesc
s(0)                     # Task (0 -> no AltDesc)
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(100)                   # MaxSize
s(100)                   # MaxWt
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(1)                     # bNoAutoComplete -- the Runner's Auto complete
                         # rewrites scripted input before the echo, so the
                         # game turns it off for itself (see the Wine notes
                         # in RUNNER_TESTS_TODO.md).
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(0)                     # iUnk1
s(0)                     # iUnk2

# ROOMS -- one room.
s(1)
s("Probe Room")          # Short
s("LONG.")               # Long
s("")                    # LastDesc
for _ in range(8): s(0)  # exits (4-point compass -> 8 slots)
s("")                    # AddDesc1
s(0)                     # Task1
s("")                    # AddDesc2
s(0)                     # Task2
s(0)                     # Obj
s("")                    # AltDesc
s(0)                     # TypeHideObjects
s(0)                     # HideOnMap

# OBJECTS -- none.
s(0)

# TASKS.
def task(cmds, complete, score):
    s(len(cmds) - 1)                 # W$Command: count, then count+1 lines
    for c in cmds: s(c)
    s(complete)          # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: ROOM_LIST0 Type 3 = all rooms
    s("")                # Question (no hints follow)
    s(0)                 # Restrictions
    if score:
        s(1)             # Actions
        s(4); s(score)   # type 4 = change score, Var1 = points
    else:
        s(0)             # Actions

s(8)
task(["alpha"],   "AAA.",         0)
task(["beta"],    "BBB.",         0)
task(["gamma"],   "ZZ ZZ.",       0)
task(["delta"],   "EEE.",         0)
task(["epsilon"], "RRR.",         0)
task(["zeta"],    "MMM.",         0)
task(["eta"],     "UUU.",         0)
task(["ping"],    "PING FIRED.",  0)

# EVENTS, NPCS
s(0); s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables

ALRS = [("AAA", "qAAA"),
        ("BBB", "CCC"),
        ("CCC", "DDD"),
        ("ZZ",  "z"),
        ("EEE", "EEE EEE"),
        ("PPPP", "QQ"),
        ("RRR", "PPPP"),
        ("MM",  "short"),
        ("MMM", "long"),
        ("WWWWW", "done"),
        ("VVVV", "WWWWW"),
        ("UUU", "VVVV")]
s(len(ALRS))             # ALRs: $Original $Replacement
for orig, repl in ALRS:
    s(orig); s(repl)
s(0)                     # CustomFont
s("2026")                # CompileDate
s("    Wild    ")        # sPassword: pw[0:4]+"Wild"+pw[4:8]

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p39ALR.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
