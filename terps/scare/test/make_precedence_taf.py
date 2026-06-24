#!/usr/bin/env python3
"""
Generate a synthetic ADRIFT 4.0 TAF for the abbreviation-precedence regression
test (test/precedence_test.c).  No copyrighted game data required.

The game is two rooms joined north/south, with two single-letter tasks whose
command letters collide with the Glk port's abbreviation table:

  * task "p"  (would expand to "open")   -- runnable only in the West room
  * task "k"  (would expand to "attack") -- runnable only in the East room

Each task's "Where" is a SOME_ROOMS list, so the letter is a live game command
in exactly one room.  This lets the test confirm that sc_does_command_match()
reports a letter as game-claimed precisely where the author uses it, which is
what lets the front end suppress its own expansion there.

The TAF is emitted exactly as SCARE's reader (sctaffil.c / sctafpar.c) expects:
a CRLF-joined list of field lines, zlib-compressed, behind the 14-byte v4.0
signature + 8 header bytes.  Battle System is OFF, so there are no battle blocks.

Run:  python3 make_precedence_taf.py precedence_test.taf
"""
import sys, zlib

# v4.0 multiline-string separator (sctafpar.c V400_SEPARATOR = bd d0 00).
SEP = "\xbd\xd0"
# v4.0 signature (14 bytes) + 8 header bytes SCARE reads but does not validate.
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,
             0x93,0x45,0x3e,0x61,0x39,0xfa])
HEADER_EXTRA = bytes(8)

L = []  # the list of TAF field lines, in strict schema order
def s(x):  L.append(str(x))               # string / integer / boolean line
def ml(x): L.append(x); L.append(SEP)     # M-multiline: content then separator

# Room exit order for the 8-exit layout: N E S W U D In Out.
def no_exit():    s(0)
def exit_to(dest):                         # dest is 1-based (room index + 1)
    s(dest); s(0); s(0); s(0)              # #Dest #Var1 #Var2 #Var3

# ---- HEADER:  MStartupText #StartRoom MWinText ----
ml("A synthetic two-room arena for testing command precedence.")
s(0)                                        # StartRoom (0-based room index)
ml("You have finished the precedence test.")

# ---- GLOBAL ----
s("Precedence Test")                        # GameName
s("SCARE regression")                       # GameAuthor
s("I don't understand.")                     # DontUnderstand
s(2)                                        # Perspective (2 = second person)
s(1)                                        # ShowExits
s(0)                                        # WaitTurns
s(1)                                        # DispFirstRoom
s(0)                                        # BattleSystem  <-- OFF
s(0)                                        # MaxScore
s("Player")                                 # PlayerName
s(0)                                        # PromptName
s("A test fighter.")                         # PlayerDesc
s(0)                                        # Task  (==0 -> AltDesc is skipped)
s(0)                                        # Position
s(0)                                        # ParentObject
s(0)                                        # PlayerGender
s(100)                                      # MaxSize
s(100)                                      # MaxWt
# BattleSystem off -> no <BATTLE>Battle block here.
s(0)                                        # EightPointCompass (0 -> 8 exits)
s(0)                                        # bNoDebug
s(0)                                        # NoScoreNotify
s(0)                                        # NoMap
s(0)                                        # bNoAutoComplete
s(0)                                        # bNoControlPanel
s(0)                                        # bNoMouse
s(0)                                        # Sound    (off -> no resource lines)
s(0)                                        # Graphics (off -> no resource lines)
# IntroRes, WinRes: RESOURCE empty (sound/graphics off).
s(0)                                        # StatusBox
s("")                                       # StatusBoxText
s(0)                                        # iUnk1
s(0)                                        # iUnk2
s(0)                                        # Embedded

# ---- ROOMS: V<ROOM>Rooms, count = 2 ----
s(2)
# Room 0 -- West, with a north exit to room 1.
s("Arena West")                             # Short
s("The west arena.  Here the letter 'p' is a punch.")  # Long
exit_to(2)                                  # N -> room 1 (1-based)
no_exit(); no_exit(); no_exit()             # E S W
no_exit(); no_exit(); no_exit(); no_exit()  # U D In Out
s(0)                                        # Alts count
s(0)                                        # bHideOnMap
# Room 1 -- East, with a south exit back to room 0.
s("Arena East")                             # Short
s("The east arena.  Here the letter 'k' is a kick.")   # Long
no_exit(); no_exit()                        # N E
exit_to(1)                                  # S -> room 0 (1-based)
no_exit()                                   # W
no_exit(); no_exit(); no_exit(); no_exit()  # U D In Out
s(0)                                        # Alts count
s(0)                                        # bHideOnMap

# ---- OBJECTS: V<OBJECT>Objects, count = 0 ----
s(0)

# ---- TASKS: V<TASK>Tasks, count = 2 ----
s(2)
def task(letter, complete, rooms):
    s(1); s(letter)                         # V$Command: count + the one command
    s(complete)                             # CompleteText
    s("")                                   # ReverseMessage
    s("")                                   # RepeatText
    s("")                                   # AdditionalMessage
    s(0)                                    # ShowRoomDesc
    s(1)                                    # Repeatable
    s(0)                                    # Reversible
    s(0)                                    # V$ReverseCommand count
    s(2)                                    # Where #Type = SOME_ROOMS
    for flag in rooms:                      # one boolean per room (count = 2)
        s(flag)
    s("")                                   # Question ("" -> no Hint1/Hint2)
    s(0)                                    # Restrictions count
    s(0)                                    # Actions count
    s("")                                   # RestrMask
    # Res: RESOURCE empty (sound/graphics off).
task("p", "You throw a punch.", [1, 0])     # West only
task("k", "You throw a kick.",  [0, 1])     # East only

# ---- tail: Events, NPCs, RoomGroups, Synonyms, Variables, ALRs, ... ----
s(0)                                        # Events count
s(0)                                        # NPCs count
s(0)                                        # RoomGroups count
s(0)                                        # Synonyms count
s(0)                                        # Variables count
s(0)                                        # ALRs count
s(0)                                        # CustomFont (-> no FontNameSize)
s("2026")                                   # CompileDate

# ---- serialize: CRLF-join, trailing CRLF, zlib, prepend header ----
body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = SIG + HEADER_EXTRA + zlib.compress(body, 9)
path = sys.argv[1] if len(sys.argv) > 1 else "precedence_test.taf"
open(path, "wb").write(out)
print(f"wrote {path}: {len(out)} bytes ({len(L)} lines)")
