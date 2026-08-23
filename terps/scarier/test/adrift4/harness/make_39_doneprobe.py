#!/usr/bin/env python3
"""ADRIFT 3.9 twin of the 4.0 `DONE` arena probe (make_arena_probe.py).

The 4.0 answer was measured in run400 (probe DONE, transcript Adrift_14.txt):
a task that has already been done still has its restrictions evaluated, so a
FAILING restriction prints its message and eats the command -- and that holds
on the library-callback path too (`get gem` answers "BLOCK-GEM." rather than
taking the gem).  A spent task whose restrictions PASS falls through to the
library verb, or to "I don't understand."

Two v4-corpus goldens that still disagree with the engine after that fix --
cybercow_win and melbourne_beach -- are both 3.90 games, so the open question
is whether run390 behaves the same way or whether the engine needs a version
gate.  This probe asks run390 directly.

Cells (all restrictions are "holding the stone", so each task can be completed
once with the stone in hand and re-tried after dropping it):

  scroll: a SILENT non-repeatable task (no CompleteText) whose own action
          moves the stone out of the player's hands, i.e. it turns its own
          restriction false within the very command that ran it.  This is the
          "Shadow of the Past" pattern: run400 answers the FIRST `x scroll`
          with the library description and only a SECOND one with the
          restriction message.
  alpha:  spent task alone, custom command
  beta:   spent task 2 vs runnable task 3, both matching, both failing
  book:   spent task vs library examine (book has a description)
  gem:    spent `* get * gem *` vs the system take (library callback path)
  gamma:  spent task WITH RepeatText, restriction pass and fail cells
  mmm/s:  spent task with an `* s` alt command and no south exit

Suggested session (see done39_cmds.txt):

    x scroll / i / x scroll / take stone
    alpha / beta / x book / get gem / gamma / mmm          (complete them)
    alpha / beta / x book / get gem / gamma / s            (spent, restr PASS)
    drop stone
    alpha / beta / x book / get gem / gamma / s            (spent, restr FAIL)

Usage: python3 make_39_doneprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("A synthetic 3.9 done-task probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39DONE")        # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(1)                     # Perspective (second person, as the 4.0 DONE probe)
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test player.")      # PlayerDesc
s(0)                     # Task (0 -> no AltDesc)
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(200)                   # MaxSize
s(200)                   # MaxWt
# no BATTLE block: BattleSystem is off
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(0)                     # SizeMultiple
s(0)                     # WeightMultiple

# ROOMS -- one bare arena, no exits at all (so the `* s` cell has no south).
s(1)
s("Test Arena")          # Short
s("A bare arena.")       # Long
s("")                    # LastDesc
for _ in range(8): s(0)  # exits
s("")                    # AddDesc1
s(0)                     # Task1
s("")                    # AddDesc2
s(0)                     # Task2
s(0)                     # Obj
s("")                    # AltDesc
s(0)                     # TypeHideObjects
s(0)                     # HideOnMap

# OBJECTS -- dynamic index order matters: the restrictions name 3 + index.
def obj(prefix, short, desc, initpos):
    s(prefix)            # Prefix
    s(short)             # Short
    s("")                # [1] Alias
    s(0)                 # Static (dynamic)
    s(desc)              # Description
    s(initpos)           # InitialPosition (1 = held, 4 = room 1)
    s(0)                 # Task
    s(0)                 # TaskNotDone
    s("")                # AltDesc
    s(0)                 # Container
    s(0)                 # Surface
    s(0)                 # Capacity
    s(0)                 # Wearable
    s(1)                 # SizeWeight
    s(0)                 # Parent
    s(0)                 # Openable
    s(0)                 # SitLie
    s(0)                 # Edible
    s(0)                 # Readable
    s(0)                 # Weapon

s(5)
obj("a",  "stone",  "A grey stone.",     1)   # dynamic 0 -> restr Var1 3
obj("a",  "ball",   "A red ball.",       4)   # dynamic 1 -> restr Var1 4
obj("a",  "book",   "A dusty tome.",     4)   # dynamic 2
obj("a",  "gem",    "A glittering gem.", 4)   # dynamic 3
obj("a",  "scroll", "A yellowed scroll.",4)   # dynamic 4 -> restr Var1 7

# TASKS
def task(cmds, complete, restrs, repeatable=0, repeat="", actions=()):
    s(len(cmds) - 1)     # W$Command: count, then count+1 alternatives
    for c in cmds: s(c)
    s(complete)          # CompleteText
    s("")                # ReverseMessage
    s(repeat)            # RepeatText
    s("")                # AdditionalMessage
    s(0)                 # ShowRoomDesc
    s(repeatable)        # Repeatable
    s(0)                 # Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: ROOM_LIST0 Type 3 = all rooms
    s("")                # Question (no hints follow)
    s(len(restrs))       # Restrictions
    for (v1, v2, v3, fail) in restrs:
        s(0); s(v1); s(v2); s(v3); s(fail)   # Type 0 = object location
    s(len(actions))      # Actions
    for a in actions:
        for f in a: s(f)
    # RestrMask is a parse fixup in 3.9 (no input field); Res: nothing

HOLD_STONE = 3           # restriction Var1 for dynamic object 0
HOLD_BALL  = 4           # ... dynamic object 1

s(8)
# The silent self-defeating task: no CompleteText, and its single action moves
# the stone (dynamic 0) to room 1 (Var2 0 = to room, Var3 = room + 1).
task(["* x * scroll *", "* examine * scroll *"], "",
     [(HOLD_STONE, 1, 0, "SCROLL-DUST.")], actions=[(0, 3, 0, 1)])
task(["alpha"], "ALPHA-RAN.", [(HOLD_STONE, 1, 0, "BLOCK-ALPHA.")])
task(["beta"],  "BETA1-RAN.", [(HOLD_STONE, 1, 0, "BLOCK-BETA1.")])
task(["beta"],  "BETA2-RAN.", [(HOLD_BALL,  1, 0, "BLOCK-BETA2.")], repeatable=1)
task(["* x * book *", "* examine * book *"], "BOOK-CRUMBLES.",
     [(HOLD_STONE, 1, 0, "BOOK-DUST.")])
task(["* get * gem *"], "GEM-TASK-RAN.", [(HOLD_STONE, 1, 0, "BLOCK-GEM.")])
task(["gamma"], "GAMMA-RAN.", [(HOLD_STONE, 1, 0, "BLOCK-GAMMA.")],
     repeat="GAMMA-REPEAT.")
task(["mmm", "* s"], "MOVE-TASK-RAN.", [(HOLD_STONE, 1, 0, "BLOCK-MOVE.")])

# EVENTS
s(0)

# NPCS
s(0)

# tail
s(0)                     # RoomGroups
s(0)                     # Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate
s("    Wild    ")        # sPassword: pw[0:4]+"Wild"+pw[4:8]

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

# VB PRNG stream, skipping the 14 signature draws
state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p39done.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
