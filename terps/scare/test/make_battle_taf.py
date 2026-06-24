#!/usr/bin/env python3
"""
Generate a synthetic ADRIFT 4.0 TAF that exercises the SCARE Battle System
port, for an in-repo regression test (no copyrighted game data required).

The game is one room ("Test Arena") with the player and a hostile robot, and
three dynamic objects:
  * a "blaster"  -- a *shoot*  weapon (Method 3: replaces strength)
  * a "rock"     -- a *throw*  weapon (Method 5: adds HitValue)
  * a "vest"     -- worn armour (ProtectionValue 5, not a weapon)

It is both organically playable ("shoot robot with blaster") and drivable by a
C harness that calls the public battle API.  The TAF is emitted exactly as
SCARE's reader (sctaffil.c / sctafpar.c) expects: a CRLF-joined list of field
lines, zlib-compressed, behind the 14-byte v4.0 signature + 8 header bytes.

Run:  python3 make_battle_taf.py battle_test.taf
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

# ---- HEADER:  MStartupText #StartRoom MWinText ----
ml("A synthetic arena for exercising the Battle System.")
s(0)                                        # StartRoom (0-based room index)
ml("You have won the battle test.")

# ---- GLOBAL ----
s("Battle Test")                            # GameName
s("SCARE regression")                       # GameAuthor
s("I don't understand.")                     # DontUnderstand
s(2)                                        # Perspective (2 = second person)
s(1)                                        # ShowExits
s(0)                                        # WaitTurns
s(1)                                        # DispFirstRoom
s(1)                                        # BattleSystem  <-- ON
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
# BATTLE (player): Stamina/Strength/Accuracy/Defense/Agility Lo,Hi + Recovery
s(100); s(100)                              # StaminaLo/Hi
s(10);  s(10)                               # StrengthLo/Hi
s(60);  s(60)                               # AccuracyLo/Hi
s(5);   s(5)                                # DefenseLo/Hi
s(5);   s(5)                                # AgilityLo/Hi
s(0)                                        # Recovery
s(0)                                        # EightPointCompass (0 -> 8 exits)
s(0)                                        # bNoDebug
s(0)                                        # NoScoreNotify
s(0)                                        # NoMap
s(0)                                        # bNoAutoComplete
s(0)                                        # bNoControlPanel
s(0)                                        # bNoMouse
s(0)                                        # Sound    (off -> no resource lines)
s(0)                                        # Graphics (off -> no resource lines)
# IntroRes, WinRes: nothing (sound+graphics off)
s(0)                                        # StatusBox
s("")                                       # StatusBoxText
s(0)                                        # iUnk1
s(0)                                        # iUnk2
s(0)                                        # Embedded

# ---- ROOMS: V<ROOM>Rooms, count = 1 ----
s(1)
# Room 0
s("Test Arena")                             # Short
s("A bare arena for testing combat.")        # Long
for _ in range(8):                          # [8] exits, all absent (flag 0)
    s(0)
# Res: nothing.  Alts: V count = 0.  HideOnMap (!NoMap) boolean.
s(0)                                        # Alts count
s(0)                                        # bHideOnMap

# ---- OBJECTS: V<OBJECT>Objects, count = 3 ----
s(3)

def weapon(prefix, short, desc, init_pos, is_weapon,
           protection, hitvalue, method, accuracy, wearable=0):
    s(prefix)                               # Prefix
    s(short)                                # Short
    s(0)                                    # Alias count
    s(0)                                    # Static (0 = dynamic)
    s(desc)                                 # Description
    s(init_pos)                             # InitialPosition
    s(0)                                    # Task
    s(0)                                    # TaskNotDone
    s("")                                   # AltDesc
    # (not static -> no ROOM_LIST1)
    s(0)                                    # Container
    s(0)                                    # Surface
    s(0)                                    # Capacity
    s(wearable)                             # Wearable
    s(2)                                    # SizeWeight
    s(0)                                    # Parent
    # (not static -> no {OBJECT:#Parent})
    s(0)                                    # Openable (!=5/6/7 -> no Key)
    s(0)                                    # SitLie
    s(0)                                    # Edible
    s(0)                                    # Readable (-> no ReadText)
    s(is_weapon)                            # Weapon
    s(0)                                    # CurrentState (==0 -> no States)
    s(0)                                    # ListFlag
    # Res1, Res2: nothing
    # OBJ_BATTLE: ProtectionValue HitValue Method Accuracy
    s(protection); s(hitvalue); s(method); s(accuracy)
    s("")                                   # InRoomDesc
    s(0)                                    # OnlyWhenNotMoved

# blaster: shoot weapon (method 3), held by player (InitialPosition 1)
weapon("a", "blaster", "A laser blaster.", 1, 1, 0, 30, 3, 20)
# rock: throw weapon (method 5), in the arena (InitialPosition 4 = room 0)
weapon("a", "rock", "A heavy throwing rock.", 4, 1, 0, 12, 5, 10)
# vest: worn armour (InitialPosition 5 = worn by player), ProtectionValue 5
weapon("a", "vest", "A protective combat vest.", 5, 0, 5, 0, 0, 0, wearable=1)

# ---- TASKS / EVENTS: empty ----
s(0)                                        # Tasks count
s(0)                                        # Events count

# ---- NPCS: V<NPC>NPCs, count = 1 ----
s(1)
s("Robot")                                  # Name
s("a")                                      # Prefix
s(0)                                        # Alias count
s("A hostile combat robot.")                 # Descr
s(1)                                        # StartRoom (1 = room 0, co-located)
s("")                                       # AltText
s(0)                                        # Task
s(0)                                        # Topics count
s(0)                                        # Walks count
s(0)                                        # ShowEnterExit (-> no Enter/Exit)
s("A combat robot stands here, weapons ready.")  # InRoomText
s(0)                                        # Gender
# [4] Res: nothing (sound+graphics off)
# NPC_BATTLE: Attitude Stamina(Lo,Hi) Strength(Lo,Hi) Accuracy(Lo,Hi)
#             Defense(Lo,Hi) Agility(Lo,Hi) Speed KilledTask Recovery StaminaTask
s(2)                                        # Attitude (2 = enemy)
s(100); s(100)                             # StaminaLo/Hi
s(8);  s(8)                                # StrengthLo/Hi
s(10); s(10)                               # AccuracyLo/Hi (> player Agility 5)
s(3);  s(3)                                # DefenseLo/Hi
s(4);  s(4)                                # AgilityLo/Hi
s(0)                                        # Speed
s(0)                                        # KilledTask
s(0)                                        # Recovery
s(0)                                        # StaminaTask

# ---- tail: RoomGroups, Synonyms, Variables, ALRs, CustomFont, CompileDate ----
s(0)                                        # RoomGroups count
s(0)                                        # Synonyms count
s(0)                                        # Variables count
s(0)                                        # ALRs count
s(0)                                        # CustomFont (-> no FontNameSize)
s("2026")                                   # CompileDate

# ---- serialize: CRLF-join, trailing CRLF, zlib, prepend header ----
body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = SIG + HEADER_EXTRA + zlib.compress(body, 9)
path = sys.argv[1] if len(sys.argv) > 1 else "battle_test.taf"
open(path, "wb").write(out)
print(f"wrote {path}: {len(out)} bytes ({len(L)} lines)")
