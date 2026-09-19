#!/usr/bin/env python3
"""ADRIFT 3.9 probe: which built-in commands are administrative in run390,
and does `save` / `restore` tick the event clock?

run390 generaltasks sets its not-a-turn flag MemVar_468219 (read by the
characters()+events() guard at 460669) only for status (44C52B, battle),
history/past (45F52F), score (45F6C7), count/num (45F7D6),
about/info/author/information (45FB3D), quit/bye/end (45FB6E) and turns
(45FD4D); it is cleared per line at 45EC74.  Scarier's lib_set_admin() also
marks hint, help, clear, license and statusline administrative from 3.90, and
a dozen 4.0-era verbs (again, verbose, brief, notify, time, version, ...)
unconditionally.  The TODO's second lead: FarFromHome's event clock moved one
tick per echoed `save` under run390.

One room, one control task, and a length-1 immediately-restarting event whose
FinishText "TICK." marks every turn on which events() ran.

Usage: python3 make_39_adminprobe.py [out.taf] [37|38]

With "38" or "37" the file is a 3.80 or 3.70 twin (make_38_darkprobe.py /
make_37_darkprobe.py shapes; the event record is the same) for run380 and
run370, whose generaltasks answer clear/cls/clr by emptying the RichTextBox
and setting "Screen cleared." (run380 442250, run370 43B9E8) without leaving
the routine, so the end-of-turn tick still runs.
"""
import sys

VER = sys.argv[2] if len(sys.argv) > 2 else "39"

L = []
def s(x): L.append(str(x))

# HEADER
s("A synthetic 3.9 admin probe."); s("**")
s(0)
s("You have won."); s("**")

if VER in ("37", "38"):
    if VER == "37":
        s(-1)                # WinTask: none
    # GLOBAL, ROOMS, OBJECTS, TASKS in the 3.8 shapes of make_38_darkprobe.py.
    s("Probe %sADMIN" % VER); s("SCARE probe"); s(10); s("I don't understand.")
    s(1); s(0); s(0)                    # Perspective ShowExits WaitTurns
    s(1)
    s("Test Arena"); s("A bare arena."); s("")
    for _ in range(8): s(0)
    s(""); s(0); s(""); s(0)            # AddDesc1 Task1 AddDesc2 Task2
    s(0); s(""); s(0)                   # Obj AltDesc TypeHideObjects
    s(0)
    s(1)
    s(0); s("probe")
    s("PROBE OK."); s(""); s(""); s("")
    s(0); s(1); s(0); s(0)              # ShowRoomDesc Repeatable Score SingleScore
    for _ in range(6):                  # Movements: 3.7 pairs, 3.8 triples
        s(0); s(0)
        if VER == "38": s(0)
    s(0); s(0); s("")                   # Reversible W$ReverseCommand
    s(0); s(0); s(0); s(0); s(0)        # WearObj1/2 HoldObj1/2/3
    s(0); s(0); s(0)                    # Obj1 Task TaskNotDone
    s(""); s(""); s(""); s("")          # TaskMsg HoldMsg WearMsg CompanyMsg
    s(0); s(0); s(""); s(0)             # NotInSameRoom NPC Obj1Msg Obj1Room
    s(3); s(0); s(0); s("")             # Where KillsPlayer HoldingSameRoom Question
    s(0)                                # Obj2
    if VER == "38": s(0)                # WinGame (3.7 keeps it in the header)
else:
    # GLOBAL
    s("Probe 39ADMIN")       # GameName
    s("SCARE probe")         # GameAuthor
    s("I don't understand.") # DontUnderstand
    s(1)                     # Perspective
    s(0)                     # ShowExits
    s(0)                     # WaitTurns
    s(1)                     # DispFirstRoom
    s(0)                     # BattleSystem
    s(8)                     # MaxScore
    s("Player")              # PlayerName
    s(0)                     # PromptName
    s("A test player.")      # PlayerDesc
    s(0)                     # Task
    s(0)                     # Position
    s(0)                     # ParentObject
    s(0)                     # PlayerGender
    s(100)                   # MaxSize
    s(100)                   # MaxWt
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

    # OBJECTS
    s(0)

    # TASKS
    s(1)
    s(0); s("probe")         # W$Command
    s("PROBE OK.")           # CompleteText
    s(""); s(""); s("")      # Reverse/Repeat/Additional
    s(0)                     # ShowRoomDesc
    s(1)                     # Repeatable
    s(0)                     # Reversible
    s(0); s("")              # W$ReverseCommand
    s(3)                     # Where: all rooms
    s("")                    # Question
    s(0)                     # Restrictions
    s(0)                     # Actions

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

# NPCS, tail
s(0)
if VER == "39":
    s(0); s(0); s(0); s(0); s(0)
elif VER == "38":
    s(0); s(0)               # RoomGroups, synonyms
else:
    s(0)                     # RoomGroups
    for w in ("north", "east", "south", "west", "up", "down", "in", "out",
              "look", "inventory", "examine", "pick up", "put down", "wear",
              "remove", "goto", "help"):
        s(w)                 # 3.7's seventeen command words
s("2026")
s("    Wild    ")

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,0x94,0x45,{"37": 0x39, "38": 0x36}.get(VER, 0x37),0x61,0x39,0xfa])

state = 0x00a09e86
def draw():
    global state
    state = (state * 0x43fd43fd + 0x00c39ec3) & 0x00ffffff
    return (255 * state) // 0x1000000
for _ in range(14): draw()
obf = bytes(b ^ draw() for b in body)

out = sys.argv[1] if len(sys.argv) > 1 else "p%sADMIN.taf" % VER
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
