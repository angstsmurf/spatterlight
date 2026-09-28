#!/usr/bin/env python3
"""ADRIFT 4.0 probe: what does run400 do when a task moves an NPC to "the same
room as the referenced character" and no character was ever referenced?

run400 48CEA9 (execute_action, Move NPC, Var2 = 2, Var3 = 1) indexes the NPC
array with MemVar_49420A, the referenced character, with no test for its
unset value &HFF -- the moved-NPC selector (Var1 = 1) has one (48CDB8, Exit
Sub).  Scarier's task_run_move_npc_action asserts in gs_npc_location(-1).

  rooms   Alpha (start) -> east -> Bravo
  NPCs    0 Alice in Bravo, 1 Bob in Alpha
  tasks   summon            Alice -> same room as referenced   (unset)
          fetch %character% Alice -> same room as referenced   (control)
          banish            Alice -> Bravo
          shift             referenced -> same room as player  (unset mover)
          twice             summon's action, then Alice -> Alpha
          jump              player -> same room as referenced  (unset)

Usage:
    python3 make_400_mvrefprobe.py p4MVREF.plain
    python3 taftool.py pack p4MVREF.plain p4TAKE.taf p4MVREF.taf
"""
import sys

SEP = "\xbd\xd0"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# HEADER
ml("A synthetic 4.0 move-NPC-to-referenced probe.")
s(0)                     # StartRoom: Alpha (0-based)
ml("You have won.")

# GLOBAL
s("Probe 4MVREF")
s("SCARE probe")
s("NO IDEA.")            # DontUnderstand
s(1)                     # Perspective: second person
s(1)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(0)                     # BattleSystem
s(0)                     # MaxScore
s("Player"); s(0); s("A test subject.")
s(0)                     # Task
s(0); s(0); s(0)         # Position, ParentObject, PlayerGender
s(104); s(103)           # MaxSize, MaxWt
s(0)                     # EightPointCompass
s(0); s(0); s(0)         # NoDebug, NoScoreNotify, NoMap
s(0); s(0); s(0)         # NoAutoComplete, NoControlPanel, NoMouse
s(0); s(0)               # Sound, Graphics
s(0); s("")              # StatusBox, StatusBoxText
s(3); s(3)               # SizeMultiple, WeightMultiple
s(0)                     # Embedded

def room(short, long_, exits):
    s(short); s(long_)
    for i in range(8):
        if i in exits:
            s(exits[i]); s(0); s(0); s(0)
        else:
            s(0)
    s(0)                 # Alts
    s(0)                 # HideOnMap

s(2)
room("Alpha", "The first room.",  {1: 2})
room("Bravo", "The second room.", {3: 1})

s(0)                     # Objects

# Raw action tuples: (1, Var1, Var2, Var3) moves a character -- Var1 0 =
# player, 1 = referenced, n+2 = NPC n; Var2 0 = to room (Var3 1-based),
# 2 = same room as (Var3 0 = player, 1 = referenced, n+2 = NPC n).
ACTS = [
    ("summon",            [(1, 2, 2, 1)]),
    ("fetch %character%", [(1, 2, 2, 1)]),
    ("banish",            [(1, 2, 0, 2)]),
    ("shift",             [(1, 1, 2, 0)]),
    ("twice",             [(1, 2, 2, 1), (1, 2, 0, 1)]),
    ("jump",              [(1, 0, 2, 1)]),
]

def task(cmd, text, actions):
    s(1); s(cmd)
    s(text)                  # CompleteText
    s(""); s(""); s("")      # ReverseMessage RepeatText AdditionalMessage
    s(0)                     # ShowRoomDesc
    s(1)                     # Repeatable
    s(0)                     # Reversible
    s(0)                     # V$ReverseCommand count
    s(3)                     # Where: type 3 = all rooms
    s("")                    # Question
    s(0)                     # Restrictions
    s(len(actions))          # Actions
    for a in actions:
        for f in a: s(f)
    s("")                    # RestrMask

s(len(ACTS))
for cmd, acts in ACTS:
    task(cmd, "%s FIRED." % cmd.split()[0].upper(), acts)

# EVENTS -- none.
s(0)

def npc(name, descr, startroom, inroom):
    s(name); s(""); s(0)     # Name Prefix V$Alias count
    s(descr)
    s(startroom)             # StartRoom (1-based)
    s("")                    # AltText
    s(0)                     # Task
    s(0)                     # Topics
    s(0)                     # Walks
    s(1)                     # ShowEnterExit
    s("wanders in"); s("wanders off")
    s(inroom)                # InRoomText
    s(0)                     # Gender

s(2)
npc("Alice", "A quiet woman.", 2, "Alice is here.")
npc("Bob",   "A quiet man.",   1, "Bob is here.")

s(0); s(0)               # RoomGroups, Synonyms
s(0)                     # Variables
s(0)                     # ALRs
s(0)                     # CustomFont
s("2026")                # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[1] if len(sys.argv) > 1 else "p4MVREF.plain"
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
