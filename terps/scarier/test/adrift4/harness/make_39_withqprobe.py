#!/usr/bin/env python3
"""ADRIFT 3.9 probe: the with-question rule and the weapon-question continuation.

run390's generaltasks tail (46058C-4605D2) tests the buffer for "With what?"
or a trailing "with?" and stores line & " with " as the prefix, but unlike
run400 48B4E3-48B530 it never sets the not-a-turn byte 468219.  So:

  * a task printing "What with?" / "...cut it with?" should TICK at 3.9
    (it does not at 4.0, p4WITHQ);
  * the next line should run as prefix & " " & line -- two spaces -- so a
    task command with the double space ("saw rope with  knife") tells the
    rerun from a plain catch-all;
  * dobattle's "What do you want to attack Robot with?" (44CE85) parks
    "attack robot with" but the same rule then overwrites it with the typed
    line & " with ", so `shoot robot` / `sword` should run as
    "shoot robot with  sword" (a method refusal), not "attack robot with sword".

Arena with a rope; the player holds a knife (not a weapon), a blaster
(Method 3 shoot) and a sword (Method 0); a hostile Robot with 9999 stamina and
Strength 1 marks every characters() run.  A length-1 self-restarting event
prints TICK. whenever events() ran.  BattleSystem on.

Usage: python3 make_39_withqprobe.py [out.taf]
"""
import sys

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 with-question probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39WITHQ")       # GameName
s("SCARE probe")         # GameAuthor
s("I don't understand.") # DontUnderstand
s(2)                     # Perspective
s(0)                     # ShowExits
s(0)                     # WaitTurns
s(1)                     # DispFirstRoom
s(1)                     # BattleSystem
s(0)                     # MaxScore
s("Player")              # PlayerName
s(0)                     # PromptName
s("A test fighter.")     # PlayerDesc
s(0)                     # Task
s(0)                     # Position
s(0)                     # ParentObject
s(0)                     # PlayerGender
s(100)                   # MaxSize
s(100)                   # MaxWt
# BATTLE (3.9 player): #Stamina #Strength #Defense
s(500); s(10); s(0)
s(0)                     # EightPointCompass
s(0)                     # bNoDebug
s(0)                     # NoScoreNotify
s(0)                     # NoMap
s(0)                     # bNoAutoComplete
s(0)                     # bNoControlPanel
s(0)                     # bNoMouse
s(0)                     # Sound
s(0)                     # Graphics
s(0)                     # iUnk1
s(0)                     # iUnk2

# ROOMS
s(1)
s("Arena"); s("A bare arena."); s("")
for _ in range(8): s(0)
s(""); s(0); s(""); s(0)  # AddDesc1 Task1 AddDesc2 Task2
s(0); s("")               # Obj AltDesc
s(0); s(0)                # TypeHideObjects HideOnMap

# OBJECTS -- all dynamic.
def obj(short, position, weapon=None):
    s("a"); s(short); s("")             # Prefix Short Alias
    s(0)                                # Static
    s("A probe object.")
    s(position)                         # 1 held, 4 room 0
    s(0); s(0); s("")                   # Task TaskNotDone AltDesc
    s(0); s(0); s(0)                    # Container Surface Capacity
    s(0); s(2); s(0)                    # Wearable SizeWeight Parent
    s(0); s(0); s(0); s(0)              # Openable SitLie Edible Readable
    s(0 if weapon is None else 1)       # Weapon
    # OBJ_BATTLE is read for EVERY object when BattleSystem is on.
    s(0); s(30 if weapon is not None else 0); s(weapon or 0)  # Prot Hit Method

s(4)
obj("rope", 4)
obj("knife", 1)
obj("blaster", 1, weapon=3)
obj("sword", 1, weapon=0)

# TASKS
def task(cmd, text):
    s(0); s(cmd)         # W$Command: one command
    s(text)              # CompleteText
    s(""); s(""); s("")  # Reverse/Repeat/Additional
    s(0)                 # ShowRoomDesc
    s(1)                 # Repeatable
    s(0)                 # Reversible
    s(0); s("")          # W$ReverseCommand
    s(3)                 # Where: all rooms
    s("")                # Question
    s(0)                 # Restrictions
    s(0)                 # Actions

s(10)
task("probe", "PROBE OK.")
task("saw rope", "What with?")
task("saw rope with knife", "T3 SAWN ONE SPACE.")
task("saw rope with  knife", "T4 SAWN TWO SPACES.")
task("cut rope", "What do you want to cut it with?")
task("cut rope with knife", "T6 CUT ONE SPACE.")
task("cut rope with  knife", "T7 CUT TWO SPACES.")
task("hum", "With what?")
task("hum with  knife", "T9 HUMMED TWO SPACES.")
task("whittle rope", "Whittle it with what?")

# EVENTS -- length 1, restarts immediately.
s(1)
s("Ticker")
s(1)                     # StarterType: immediate
s(1)                     # RestartType: immediately
s(0)                     # TaskFinished
s(1); s(1)               # Time1 Time2
s(""); s(""); s("TICK.") # StartText LookText FinishText
s(3)                     # Where: all rooms
s(0); s(0)               # PauseTask PauserCompleted
s(0); s("")              # PrefTime1 PrefText1
s(0); s(0)               # ResumeTask ResumerCompleted
s(0); s("")              # PrefTime2 PrefText2
s(0); s(0); s(0); s(0); s(0); s(0)  # Obj2/Dest Obj3/Dest Obj1/Dest
s(0)                     # TaskAffected

# NPCS
s(1)
s("Robot"); s("a"); s("")    # Name Prefix Alias
s("A hostile robot.")
s(1)                         # StartRoom (1-based)
s(""); s(0); s(0); s(0); s(0)  # AltText Task Topics Walks ShowEnterExit
s("Robot is here.")
s(0)                         # Gender
# NPC_BATTLE (3.9): #Attitude #Stamina #Strength #Defense #Speed #KilledTask
s(2); s(9999); s(1); s(0); s(1); s(0)

# tail
s(0); s(0); s(0); s(0); s(0)   # RoomGroups Synonyms Variables ALRs CustomFont
s("2026")
s("    Wild    ")

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,0x37,0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p39WITHQ.taf"
open(out, "wb").write(SIG + obf)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
