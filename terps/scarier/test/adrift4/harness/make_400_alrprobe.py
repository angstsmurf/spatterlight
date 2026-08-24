#!/usr/bin/env python3
"""ADRIFT 4.0 probe: how many times does the Runner apply an ALR?

The lead is humbug (4.00), command 217 `Put sweet on plinth`.  The task's
CompleteText is "I put the sweet on the plinth." and the game carries the ALR

    [I put ] -> [Okay.  I put ]

whose replacement CONTAINS its own original.  Scarier answered with one
"Okay.", run400 answers with two (Adrift_30_humbug.txt:841) -- so the Runner
applies that ALR twice where Scarier applied it once.  scprintf.cpp's
pf_replace_alrs() used to mark every ALR "used up" the first time it fired and
never let it fire again;
run390's ALR pass (loc_45BD43, the `For i = 0 To Me(188)-1 : If InStr(text,
p(0))>0 Then text = Replace(text, p(0), p(4), 1, -1, 0)` loop at the tail of
the output filter Proc_2_28_45CBD0) has no such flag, but it is also a single
pass -- which would give ONE "Okay.", not two.  So the count is not readable
off the listing, and this probe measures it instead.

Eight tasks, one room, no objects.  Every ALR original is a nonsense token
that cannot occur in engine wording, so nothing but the probe text is touched:

    alpha   -> "AAA."        ALR  AAA -> qAAA          self-containing prefix
    beta    -> "BBB."        ALRs BBB -> CCC, CCC -> DDD         two-step chain
    gamma   -> "ZZ ZZ."      ALR  ZZ  -> z          two occurrences, one pass
    delta   -> "EEE."        ALR  EEE -> EEE EEE          self-containing dup
    epsilon -> "RRR."        ALRs RRR -> PPPP, PPPP -> QQ    chain the WRONG
                             way round for one length-descending pass: PPPP
                             is tried (and misses) before RRR makes it
    zeta    -> "MMM."        ALRs MM -> short, MMM -> long    which of two
                             overlapping originals wins
    eta     -> "UUU."        ALRs UUU -> VVVV -> WWWWW -> done   three hops,
                             again in descending-length order, so a single
                             pass can only manage the first
    ping    -> "PING FIRED." control, proves the probe is wired

MEASURED 2026-08-24, run400.exe (Adrift_2/3/4.txt) and run390.exe
(Adrift_5.txt), both on the p4ALR.taf this script builds:

    cell     run400              run390            what it says
    alpha    qqAAA.              qAAA.             4.0 walks the list twice,
                                                   3.9 once; and a
                                                   self-containing ALR fires
                                                   only ONCE per walk
    beta     DDD.                DDD.              a chain the ordering
                                                   favours runs either way
    gamma    z z.                z z.              every occurrence, not just
                                                   the first
    delta    EEE EEE EEE EEE.    EEE EEE.          doubling once per walk: two
                                                   walks in 4.0, one in 3.9
    epsilon  QQ.                 PPPP.             4.0 repeats the pass until
                                                   nothing changes; 3.9 stops
                                                   dead after one
    zeta     long.               long.             longest original first
    eta      done.               VVVV.             same as epsilon, three deep

So the 4.0 filter is "repeat a full length-descending pass until a pass
changes nothing", with each self-containing ALR retired for the rest of the
walk it fired in, and 3.9 is exactly one such pass.  That is what
pf_filter_internal() and pf_replace_alrs() implement.  It leaves open how many
WALKS a turn's text gets, which is not two-always -- see
make_400_alrsrcprobe.py and make_400_walkcountprobe.py.

Output is the PLAIN body only; produce a Runner-valid .taf with:

    python3 make_400_alrprobe.py p4ALR.plain
    python3 taftool.py pack p4ALR.plain <any valid 4.0 donor.taf> p4ALR.taf

(taftool.py sits next to this script; the donor supplies the "Wild" password
trailer run400 validates.)
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("ALR probe.")
s(0)                     # StartRoom (0-based)
ml("You have won.")

# GLOBAL
s("ALR Probe 400")
s("SCARE probe")
s("I don't understand.")
s(2)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")
s(0)                     # PromptName
s("A test subject.")
s(0); s(0); s(0); s(0)   # Task, Position, ParentObject, PlayerGender
s(100); s(100)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(0)                     # StatusBox
s("")                    # StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

# ROOMS -- one.
s(1)
s("Probe Room")
s("LONG.")
for _ in range(8):
    s(0)
s(0)                     # Alts count
s(0)                     # HideOnMap

# OBJECTS
s(0)

# TASKS
def task(cmd, text):
    s(1); s(cmd)         # V$Command
    s(text)              # CompleteText
    s("")                # ReverseMessage
    s("")                # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0)                 # V$ReverseCommand count
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions
    s("")                # RestrMask

s(8)
task("alpha", "AAA.")
task("beta",  "BBB.")
task("gamma", "ZZ ZZ.")
task("delta", "EEE.")
task("epsilon", "RRR.")
task("zeta",  "MMM.")
task("eta",   "UUU.")
task("ping",  "PING FIRED.")

# EVENTS, NPCS
s(0)
s(0)

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
s(len(ALRS))
for orig, repl in ALRS:
    s(orig)              # Original
    s(repl)              # Replacement

s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4ALR.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
