#!/usr/bin/env python3
"""Minimal ADRIFT 3.9 arena probe: player + shoot weapon + enemy robot.
Emits both the obfuscated .taf (for run390) and the plain body (debug)."""
import sys

L = []
def s(x): L.append(str(x))

# HEADER: MStartupText #StartRoom MWinText   (M = content lines + "**" line)
s("A synthetic 3.9 arena probe."); s("**")
s(0)
s("You have won."); s("**")

# GLOBAL
s("Probe 39")           # GameName
s("SCARE probe")        # GameAuthor
s("I don't understand.")# DontUnderstand
s(2)                    # Perspective
s(1)                    # ShowExits
s(0)                    # WaitTurns
s(1)                    # DispFirstRoom
s(1)                    # BattleSystem
s(0)                    # MaxScore
s("Player")             # PlayerName
s(0)                    # PromptName
s("A test fighter.")    # PlayerDesc
s(0)                    # Task (0 -> no AltDesc)
s(0)                    # Position
s(0)                    # ParentObject
s(0)                    # PlayerGender
s(100)                  # MaxSize
s(100)                  # MaxWt
# BATTLE (3.9 player): #Stamina #Strength #Defense
s(100); s(10); s(0)
s(0)                    # EightPointCompass
s(0)                    # bNoDebug (ignored, consumed)
s(0)                    # NoScoreNotify
s(0)                    # NoMap
s(0)                    # bNoAutoComplete
s(0)                    # bNoControlPanel
s(0)                    # bNoMouse
s(0)                    # Sound
s(0)                    # Graphics
# IntroRes WinRes: nothing (sound+graphics off); FStatusBox EStatusBoxText: no lines
s(0)                    # iUnk1
s(0)                    # iUnk2
# FEmbedded: no line

# ROOMS
s(1)
s("Test Arena")         # Short
s("A bare arena.")      # Long
s("")                   # LastDesc
for _ in range(8): s(0) # exits (compass off -> 8)
s("")                   # AddDesc1
s(0)                    # Task1
s("")                   # AddDesc2
s(0)                    # Task2
s(0)                    # Obj
s("")                   # AltDesc
s(0)                    # TypeHideObjects
# Res/LastRes/Task1Res/Task2Res/AltRes: nothing; NoMap==0 -> BHideOnMap
s(0)                    # HideOnMap

# OBJECTS
s(1)
s("a")                  # Prefix
s("blaster")            # Short
s("")                   # [1] Alias
s(0)                    # Static (dynamic)
s("A laser blaster.")   # Description
s(1)                    # InitialPosition (1 = held by player)
s(0)                    # Task
s(0)                    # TaskNotDone
s("")                   # AltDesc
s(0)                    # Container
s(0)                    # Surface
s(0)                    # Capacity
s(0)                    # Wearable
s(2)                    # SizeWeight
s(0)                    # Parent
s(0)                    # Openable
s(0)                    # SitLie
s(0)                    # Edible
s(0)                    # Readable
s(1)                    # Weapon
# ZCurrentState, FListFlag: no lines; Res1 Res2: nothing
# OBJ_BATTLE (3.9): #ProtectionValue #HitValue #Method
s(0); s(30); s(3)
# EInRoomDesc, ZOnlyWhenNotMoved: no lines

# TASKS, EVENTS
s(0); s(0)

# NPCS
s(1)
s("Robot")              # Name
s("a")                  # Prefix
s("")                   # [1] Alias
s("A hostile robot.")   # Descr
s(1)                    # StartRoom (1-based)
s("")                   # AltText
s(0)                    # Task
s(0)                    # Topics
s(0)                    # Walks
s(0)                    # ShowEnterExit
s("Robot is here.")     # InRoomText
s(0)                    # Gender
# [4] Res: nothing
# NPC_BATTLE (3.9): #Attitude #Stamina #Strength #Defense #Speed #KilledTask
s(2); s(35); s(0); s(0); s(0); s(0)

# tail
s(0)                    # RoomGroups
s(0)                    # Synonyms
s(0)                    # Variables
s(0)                    # ALRs
s(0)                    # CustomFont
s("2026")               # CompileDate
s("    Wild    ")       # sPassword: pw[0:4]+"Wild"+pw[4:8], Runner checks Mid(5,4)

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

out = sys.argv[1] if len(sys.argv) > 1 else "p39.taf"
open(out, "wb").write(SIG + obf)
open(out + ".plain", "wb").write(body)
print("wrote %s (%d bytes)" % (out, 14 + len(obf)))
