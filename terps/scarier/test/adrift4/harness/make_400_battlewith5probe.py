#!/usr/bin/env python3
"""ADRIFT 4.0 probe: which object does dobattle's " with " term really bind?

make_400_battlewith{2,3,4}probe.py measured what run400 does with the text
after " with " and left two cases unexplained: the Cool Sword is refused even
though it is held, flagged Weapon and the object the refusal itself names, and
the tie on `with awesome sword` swings in those probes where the structurally
identical tie in Illegal Socks dies in the character catch-all.  This series
settles both with fourteen variants, each one change from the p4BATTLEW3
baseline that swings.  Variants are selected by argv[1]:

    A  the two swords alone, no third weapon held
    B  Illegal Socks' surface: Capitalised Prefix/Short/Alias, second person
    C  a namesake NPC pair beside the gargoyle, as Illegal Socks has
    D  Capitalised Prefix/Short/Alias, third person as the baseline
    E  lowercase as the baseline, second person
    F  Capitalised Prefix only            "Cool"/"sword"/["sword"]
    G  Capitalised Short only             "cool"/"Sword"/["sword"]
    H  Capitalised Alias only             "cool"/"sword"/["Sword"]
    I  the same pair in the other order, the Awesome Sword first
    J  no Short shared at all             "cool"/"blade"/["blade1"]
    K  G's pair without the club, to reach `with sword` there
    L  three swords sharing the Short     + "plain"/"sword"/["sword3"]
    M  the Cool Sword with a private Alias "cool"/"sword"/["cool2"]
    N  the baseline pair plus a second object answering to "cool":
                                          "cool"/"shield"/[]

Measured with run400x under Wine, 2026-09-27, one seed, `attack gargoyle
with <term>` throughout (Adrift_305..309_battlew5{a..n}.txt).  A dash is a
refusal with no swing and no turn; (obj) is the object catch-all "I don't
understand what you want me to do with X.", (chr) the character catch-all
"I don't understand what you want to do with Gargoyle.":

    variant  sword     cool sword   awesome sword  other
    A        --        -- (obj)     awesome        sword2 -> awesome
    B        --        -- (obj)     -- (chr)       sword2 -> awesome
    C        --        --           awesome        "Which Quzar." to the pair
    D        --        -- (obj)     -- (chr)       sword2 -> awesome
    E        --        -- (obj)     awesome        sword2 -> awesome
    F        --        -- (obj)     awesome        sword2 -> awesome
    G        --        -- (obj)     -- (chr)       sword2 -> awesome
    H        --        -- (obj)     awesome        sword2 -> awesome
    I        -- (obj)  -- (obj)     awesome        sword2 -> awesome
    J        awesome   n/a          awesome        blade -> cool,
                                                   cool blade -> cool
    K        -- (obj)  -- (obj)     -- (chr)       sword2 -> awesome
    L        -- (obj)  -- (obj)     awesome        plain sword -> plain
    M        -- (chr)  COOL         awesome        cool2 -> cool
    N        n/a       -- (obj)     n/a            cool shield -> shield,
                                                   shield -> shield,
                                                   sword sword2 -> -- (chr)

B, D and G reproduce Illegal Socks exactly, E, F and H do not, so the one
property that matters is the CASE OF THE SHORT NAME: run400 compares an
object's Short to the (lower-cased) input case-sensitively, where it compares
the Aliases and the Prefix words without regard to case.  A Short stored
capitalised can therefore never be typed.  Illegal Socks agrees twice over
outside the battle path: `with armor` finds no object at all although the
Leather Armor is worn, while `with full suit of armor` reaches the Full Suit
through its three Prefix words alone (Adrift_305_socks6.txt).

I, J, L and M answer the other half.  I rules out object order -- the Cool
Sword is refused first or second -- and J shows nothing is wrong with the
object itself, since "cool"/"blade" binds on `with blade` and `with cool
blade`.  M is decisive, and was predicted before the drive: give the Cool
Sword the private Alias "cool2" and leave everything else alone, and `with
cool sword` SWINGS.  What disqualifies it in every other variant is that its
Alias "sword" is the Awesome Sword's Short.

Fitting all 68 measured outcomes (64 here plus the four W2/W3/W4 terms) leaves
one rule for the weapon bind, and no other combination of a scoring rule, a
candidate rule, a tie rule and an ambiguity gate survives them:

  * a word matches a Short case-sensitively, an Alias or a Prefix word without
    case, always as a whole word;
  * an object is a candidate only if its Short or one of its Aliases matched --
    a Prefix alone does not make one;
  * its score is the number of DISTINCT input words it answers to, so a word
    that matches both the Short and an Alias still counts once;
  * the highest score wins and a tie goes to the last such object in object
    order (which is why `axe pike` and `pike axe` both bind the pike);
  * the winner still has to get past an ambiguity gate, and is refused if
      - every word it matched is also answered by some other object
        (`with sword`, where "sword" alone never singles either sword out), or
      - it matched through an Alias and that Alias is another object's name
        (`with cool sword` in every variant but M and J), or
      - it matched through an Alias while another object is also a candidate
        (`with sword sword2`, which refuses although the Awesome Sword scores
        2 to the Cool Sword's 1);
  * and then the ordinary checks: the winner must be a Weapon and be held.

The catch-all that follows a refusal comes from a SECOND, ordinary resolver,
which is why the message and the bind can disagree.  That one scores as
Scarier's lib_verb_object_name_score() already does -- Short 1 (still
case-sensitive), first matching Alias +1, +1 per matching Prefix word, with a
Prefix match alone enough to make a candidate -- and names its strict winner,
or nothing at all on a tie, which is what turns the message into the character
catch-all.  All 29 measured messages fit it.

Two corollaries for Illegal Socks, whose swords are exactly G's:

  * `attack dr myanus hurts with awesome sword` cannot work in run400.  With
    the Shorts unreachable, the Awesome Sword is not even a candidate (its
    Alias "Sword2" does not match, and a Prefix cannot make one), so the Cool
    Sword wins on its Alias and the gate throws it out.  `with sword2` is the
    only phrasing that reaches the Awesome Sword, which is what the harness
    row now uses.
  * Scarier swings there instead because it matches the Short without regard
    to case and has no such gate, so it binds a weapon run400 never finds.
    lib_battle_scan_with() is left as the measured 3.90 walk (thesorc); this
    rule is 4.0 only and is not implemented.

Commands: v4_full_rerun_cmds/battlewith5a.txt, with 5c/5i/5j/5k/5l/5m/5n.txt
for the variants that name other objects or another NPC; 5b and 5d..5h reuse
5a.txt.

Build (and likewise for every other variant):
    python3 make_400_battlewith5probe.py A p4BATTLEW5A.plain
    python3 taftool.py pack p4BATTLEW5A.plain p4TAKE.taf p4BATTLEW5A.taf
"""
import sys

SEP = "\xbd\xd0"

variant = (sys.argv[1] if len(sys.argv) > 1 else "A").upper()
if variant not in "ABCDEFGHIJKLMN" or len(variant) != 1:
    sys.exit("variant must be one of A-N")

# (capitalise prefix, short, alias), Perspective, keep the club, namesake pair
CONFIG = {
    "A": ((0, 0, 0), 2, False, False),
    "B": ((1, 1, 1), 1, True,  False),
    "C": ((0, 0, 0), 2, True,  True),
    "D": ((1, 1, 1), 2, True,  False),
    "E": ((0, 0, 0), 1, True,  False),
    "F": ((1, 0, 0), 2, True,  False),
    "G": ((0, 1, 0), 2, True,  False),
    "H": ((0, 0, 1), 2, True,  False),
    "I": ((0, 0, 0), 2, False, False),
    "J": ((0, 0, 0), 2, False, False),
    "K": ((0, 1, 0), 2, False, False),
    "L": ((0, 0, 0), 2, False, False),
    "M": ((0, 0, 0), 2, False, False),
    "N": ((0, 0, 0), 2, False, False),
}
caps, perspective, keep_club, namesake = CONFIG[variant]

L = []
def s(x):  L.append(str(x))
def ml(x): L.append(x); L.append(SEP)

# ---- HEADER ----
ml("Battle with-tie probe %s." % variant)
s(0)                                        # StartRoom
ml("Won.")

# ---- GLOBAL ----
s("Battle With Tie Probe %s" % variant)
s("Scarier probe")
s("I don't understand.")
s(perspective)                              # Perspective (1 = second/"You are", 2 = third)
s(1)                                        # ShowExits
s(0)                                        # WaitTurns
s(1)                                        # DispFirstRoom
s(1)                                        # BattleSystem ON
s(0)                                        # MaxScore
s("Player")
s(0)                                        # PromptName
s("A test fighter.")
s(0)                                        # Task
s(0)                                        # Position
s(0)                                        # ParentObject
s(0)                                        # PlayerGender
s(100)                                      # MaxSize
s(100)                                      # MaxWt
s(500); s(500)                              # StaminaLo/Hi
s(10);  s(10)                               # StrengthLo/Hi
s(60);  s(60)                               # AccuracyLo/Hi
s(5);   s(5)                                # DefenseLo/Hi
s(5);   s(5)                                # AgilityLo/Hi
s(0)                                        # Recovery
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

# ---- ROOMS ----
s(1)
s("Main Hall")
s("A massive hall with four stone pillars.")
for _ in range(8):
    s(0)
s(0)                                        # Alts count
s(0)                                        # bHideOnMap

# ---- OBJECTS ----
def obj(prefix, short, aliases, descr, weapon, method, hit):
    s(prefix); s(short)
    s(len(aliases))
    for alias in aliases:
        s(alias)
    s(0)                                    # Static
    s(descr); s(1); s(0); s(0); s("")       # Descr InitialPosition Task TaskNotDone AltDesc
    s(0); s(0); s(0); s(0)                  # Container Surface Capacity Wearable
    s(2); s(0)                              # SizeWeight Parent
    s(0); s(0); s(0); s(0)                  # Openable SitLie Edible Readable
    s(weapon); s(0); s(0)                   # Weapon CurrentState ListFlag
    s(0); s(hit); s(method); s(20)          # Protection HitValue Method Accuracy
    s(""); s(0)                             # InRoomDesc OnlyWhenNotMoved

def cap(word, flag):                        # Illegal Socks capitalises every field
    return word[:1].upper() + word[1:] if flag else word

def names(prefix, short, alias):
    return (cap(prefix, caps[0]), cap(short, caps[1]),
            [cap(alias, caps[2])] if alias else [])

COOL    = ("cool", "sword", "sword", "A cool sword.", 30)
AWESOME = ("awesome", "sword", "sword2", "An awesome sword.", 40)
SWORDS = {                                  # (prefix, short, alias, descr, hit)
    "I": [AWESOME, COOL],                   # the same pair, the other way round
    "J": [("cool", "blade", "blade1", "A cool blade.", 30), AWESOME],
    "L": [COOL, AWESOME,                    # three objects sharing one Short
          ("plain", "sword", "sword3", "A plain sword.", 50)],
    "M": [("cool", "sword", "cool2", "A cool sword.", 30), AWESOME],
    "N": [COOL, AWESOME,                    # a second object answering to "cool"
          ("cool", "shield", None, "A cool shield.", 60)],
}

objects = [names(prefix, short, alias) + (descr, hit)
           for prefix, short, alias, descr, hit in SWORDS.get(variant, [COOL, AWESOME])]
if keep_club:
    objects.append(names("a", "club", None) + ("A plain club.", 50))

s(len(objects))
for prefix, short, aliases, descr, hit in objects:
    obj(prefix, short, aliases, descr, 1, 2, hit)

# ---- TASKS / EVENTS ----
s(0)
s(0)

# ---- NPCS ----
def npc(name, prefix, aliases, descr, here):
    s(name); s(prefix)
    s(len(aliases))
    for alias in aliases:
        s(alias)
    s(descr)
    s(1)                                    # StartRoom (room 0)
    s(""); s(0); s(0); s(0); s(0)           # AltText Task Topics Walks ShowEnterExit
    s(here)
    s(2)                                    # Gender (it)
    s(1)                                    # Attitude (1 = neutral)
    s(9999); s(9999)                        # Stamina (survives every swing)
    s(8);  s(8)                             # Strength
    s(10); s(10)                            # Accuracy
    s(3);  s(3)                             # Defense
    s(4);  s(4)                             # Agility
    s(0); s(0); s(0); s(0)                  # Speed KilledTask Recovery StaminaTask

npcs = [("Gargoyle", "", [], "A stone gargoyle.", "Gargoyle is here.")]
if namesake:                                # Illegal Socks' namesake NPC pair
    npcs += [("Quzar", "Your", ["Friend"], "Your pal Quzar.", "Quzar is here.")] * 2
s(len(npcs))
for name, prefix, aliases, descr, here in npcs:
    npc(name, prefix, aliases, descr, here)

# ---- tail ----
s(0); s(0); s(0); s(0); s(0)                # RoomGroups Synonyms Variables ALRs CustomFont
s("2026")

body = ("\r\n".join(L) + "\r\n").encode("latin-1")
out = sys.argv[2] if len(sys.argv) > 2 else "p4BATTLEW5%s.plain" % variant
open(out, "wb").write(body)
print("wrote %s (%d bytes, %d lines)" % (out, len(body), len(L)))
