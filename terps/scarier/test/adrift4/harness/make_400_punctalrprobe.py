#!/usr/bin/env python3
"""ADRIFT 4.0 probe: does punctuation in an ALR change how it is applied?

The lead was sophie.taf (4.00, the comp release), whose 461 ALRs include

    [, and] -> [:]

which the author deleted again in the post-comp sa.taf.  It looked for a while
as though run400 ignored the rule: the run400 transcripts named
Adrift_41..45_sophie.txt keep all six of their ", and" while ALRs of 3, 4, 6
and 7 characters fire in the same strings.  THAT WAS A MISPAIRING -- every one
of those runs was launched on sa.taf, which has no such rule, so there is no
divergence and sophie.taf has never been measured.  See the CLOSED 2026-08-25
sophie section in notes/WINE-TRANSCRIPTS-TODO.md; the author's own two fixup
ALRs (#455/#458, colons in their Originals where the raw text has ", and")
say run400 does apply it.

The general question is still unmeasured, and is what this probe asks: does an
ALR whose Original begins with punctuation, or whose Replacement is nothing
but punctuation, behave any differently from a word-for-word rule?  The
listing says no.  Proc_21_20_44C7DC (General.bas:4829), the Runner's ALR walk,
is a plain

    For i = 0 To count - 1
      p = ALR(i)
      If InStr(1, s, p.Original, 0) > 0 Then
        If s = p.Replacement Then Exit Sub
        s = Replace(s, p.Original, ALR(p.Replacement, 0), 1, -1, 0)
        If InStr(1, p.Original, p.Replacement, 0) > 0 Then      ' empty body
      End If
    Next

over a table pre-sorted by Len(Original) descending at load
(mdlSpreadTheLoad.bas loc_4925A0), and nothing in it inspects a character.
Measure it anyway before believing it.

Seven cells, one ALR each, every token nonsense so no cell can touch another:

    alpha   "AA, zz BB."   [, zz]  -> [:]      leading comma, colon repl
                                               -- the sophie shape exactly
    beta    "CC zz DD."    [zz DD] -> [:]      colon repl, no leading punct
    gamma   "EE, yy FF."   [, yy]  -> [GOT3]   leading comma, word repl
    delta   "GG yy HH."    [yy HH] -> [GOT4]   control: plain both ends
    epsilon "II ww JJ."    [ ww]   -> [GOT5]   leading space
    zeta    "KK: vv LL."   [: vv]  -> [GOT6]   leading colon
    eta     "MM tt NN."    [tt]    -> [,]      comma-only replacement
    ping    "PING FIRED."  none                proves the probe is wired

Read the answers off the transcript: a cell that fires prints its replacement,
a cell the Runner refuses prints its text verbatim.  Scarier fires all seven.

Output is the PLAIN body only; produce a Runner-valid .taf with:

    python3 make_400_punctalrprobe.py p4PALR.taf.plain
    python3 taftool.py pack p4PALR.taf.plain ../games/sophie.taf p4PALR.taf

(taftool.py sits next to this script; the donor supplies the "Wild" password
trailer run400 validates.)  Staged for Wine as
~/adrift-battle/runner/wine/pfx/drive_c/adrift/p4PALR.taf, driven by the
existing cmdfile_alr.txt, which already lists these eight cells in order.
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("Punctuation ALR probe.")
s(0)                     # StartRoom (0-based)
ml("You have won.")

# GLOBAL
s("Punct ALR Probe 400")
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
task("alpha",   "AA, zz BB.")
task("beta",    "CC zz DD.")
task("gamma",   "EE, yy FF.")
task("delta",   "GG yy HH.")
task("epsilon", "II ww JJ.")
task("zeta",    "KK: vv LL.")
task("eta",     "MM tt NN.")
task("ping",    "PING FIRED.")

# EVENTS, NPCS
s(0)
s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables

ALRS = [(", zz",  ":"),
        ("zz DD", ":"),
        (", yy",  "GOT3"),
        ("yy HH", "GOT4"),
        (" ww",   "GOT5"),
        (": vv",  "GOT6"),
        ("tt",    ",")]
s(len(ALRS))
for orig, repl in ALRS:
    s(orig)              # Original
    s(repl)              # Replacement

s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4PALR.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
