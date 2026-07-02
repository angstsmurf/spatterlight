#!/usr/bin/env python3
"""
Generate a synthetic ADRIFT 4.0 TAF for exercising pronoun ("it"/"them"/
"him"/"her"/"he"/"she") resolution, both in SCARIER and in the original
Windows Runner (run400.exe), so their behaviour can be diffed directly.

One room ("Test Room") with the player, three takeable objects
(coin / key / cup) and three co-located NPCs of each gender
(man = male, woman = female, blob = neuter).  Battle System is OFF, so no
battle blocks are emitted (they are gated on ?GBattleSystem: in the schema).

Run:  python3 make_pronoun_taf.py pronoun_test.taf
"""
import sys, zlib

SEP = "\xbd\xd0"
SIG = bytes([0x3c,0x42,0x3f,0xc9,0x6a,0x87,0xc2,0xcf,
             0x93,0x45,0x3e,0x61,0x39,0xfa])
# The 8 ASCII digits after the signature are the offset just past the compressed
# game stream: field = len(zlib stream) + 23 (= 22-byte header + 1).  The Runner
# validates this; SCARE ignores it.  (Confirmed across every ADRIFT 4 game
# checked -- the diff is always exactly 23.)
#
# The file ends with a 15-byte footer: 00 + 12 bytes + CRLF.  The Runner's loader
# (Sub_20_6) does Randomize 1976, burns (LOF-14) Rnd() values, then XOR-decrypts
# those 12 bytes and REQUIRES chars 5..8 == "Wild" (else "Not an Adventure
# file").  The 12 bytes decrypt to "    Wild    ".  The VB6 PRNG state right
# after Rnd(-1);Randomize 1976 is 0x00a09e86 (== SCARE's taf_random_state); the
# advance is seed=(seed*0xFD43FD+0xC39EC3) mod 2^24, keystream byte Int(Rnd*255).
_A, _C, _M, _S0 = 0xFD43FD, 0xC39EC3, 0x1000000, 0xa09e86

def _footer(total_len):
    """The 15-byte ADRIFT v4 trailer for a file of length total_len."""
    plain = b"    Wild    "
    a, c = 1, 0
    ks = []
    for _ in range(total_len - 2):           # advance up to the last needed index
        a = (_A * a) % _M; c = (_A * c + _C) % _M
        ks.append(int(((a * _S0 + c) % _M) / _M * 255))
    mid = bytes(plain[i] ^ ks[total_len - 13 + i - 1] for i in range(12))
    return b"\x00" + mid + b"\x0d\x0a"

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# ---- HEADER:  MStartupText #StartRoom MWinText ----
ml("A room built for testing pronouns.")
s(0)
ml("You have won the pronoun test.")

# ---- GLOBAL ----
s("Pronoun Test")                           # GameName
s("SCARIER regression")                     # GameAuthor
s("I don't understand.")                    # DontUnderstand
s(2)                                        # Perspective (2 = second person)
s(1)                                        # ShowExits
s(0)                                        # WaitTurns
s(1)                                        # DispFirstRoom
s(0)                                        # BattleSystem  <-- OFF
s(0)                                        # MaxScore
s("Player")                                 # PlayerName
s(0)                                        # PromptName
s("A test subject.")                        # PlayerDesc
s(0)                                        # Task
s(0)                                        # Position
s(0)                                        # ParentObject
s(0)                                        # PlayerGender
s(100)                                      # MaxSize
s(100)                                      # MaxWt
# (battle off -> no player BATTLE block)
s(0)                                        # EightPointCompass
s(0)                                        # bNoDebug
s(0)                                        # NoScoreNotify
s(0)                                        # NoMap
s(0)                                        # bNoAutoComplete
s(0)                                        # bNoControlPanel
s(0)                                        # bNoMouse
s(0)                                        # Sound
s(0)                                        # Graphics
s(0)                                        # StatusBox
s("")                                       # StatusBoxText
s(0)                                        # iUnk1
s(0)                                        # iUnk2
s(0)                                        # Embedded

# ---- ROOMS: count = 1 ----
s(1)
s("Test Room")                              # Short
s("A bare room for testing pronouns.")      # Long
for _ in range(8):                          # 8 exits, all absent
    s(0)
s(0)                                        # Alts count
s(0)                                        # bHideOnMap

# ---- OBJECTS: count = 3 ----
s(3)

def obj(prefix, short, desc):
    s(prefix)                               # Prefix
    s(short)                                # Short
    s(0)                                    # Alias count
    s(0)                                    # Static (dynamic)
    s(desc)                                 # Description
    s(4)                                    # InitialPosition (4 = room 0)
    s(0)                                    # Task
    s(0)                                    # TaskNotDone
    s("")                                   # AltDesc
    s(0)                                    # Container
    s(0)                                    # Surface
    s(0)                                    # Capacity
    s(0)                                    # Wearable
    s(0)                                    # SizeWeight (weightless: carry all)
    s(0)                                    # Parent
    s(0)                                    # Openable
    s(0)                                    # SitLie
    s(0)                                    # Edible
    s(0)                                    # Readable
    s(0)                                    # Weapon
    s(0)                                    # CurrentState
    s(0)                                    # ListFlag
    s("")                                   # InRoomDesc
    s(0)                                    # OnlyWhenNotMoved

obj("a", "coin", "A shiny gold coin.")
obj("a", "key",  "A rusty iron key.")
obj("a", "cup",  "A chipped tea cup.")

# ---- TASKS / EVENTS: empty ----
s(0)                                        # Tasks count
s(0)                                        # Events count

# ---- NPCS: count = 3 ----
s(3)

def npc(name, prefix, desc, inroom, gender):
    s(name)                                 # Name
    s(prefix)                               # Prefix
    s(0)                                    # Alias count
    s(desc)                                 # Descr
    s(1)                                    # StartRoom (1 = room 0)
    s("")                                   # AltText
    s(0)                                    # Task
    s(0)                                    # Topics count
    s(0)                                    # Walks count
    s(0)                                    # ShowEnterExit
    s(inroom)                               # InRoomText
    s(gender)                               # Gender (0=male,1=female,2=neuter)
    # [4] Res: nothing (sound+graphics off).  Battle off -> no NPC_BATTLE.

npc("man",   "a", "An ordinary man.",     "A man stands here.",    0)
npc("woman", "a", "An ordinary woman.",   "A woman stands here.",  1)
npc("blob",  "a", "A featureless blob.",  "A blob sits here.",     2)

# ---- tail ----
s(0)                                        # RoomGroups count
s(0)                                        # Synonyms count
s(0)                                        # Variables count
s(0)                                        # ALRs count
s(0)                                        # CustomFont
s("2026")                                   # CompileDate

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
comp = zlib.compress(body)                   # default level -> 78 9c, like real games
field = f"{len(comp) + 23:08d}".encode("ascii")
total = len(SIG) + len(field) + len(comp) + 15
out = SIG + field + comp + _footer(total)
path = sys.argv[1] if len(sys.argv) > 1 else "pronoun_test.taf"
open(path, "wb").write(out)
print(f"wrote {path}: {len(out)} bytes ({len(L)} lines)")
