#!/usr/bin/env python3
"""Bucket the out/*.diff hunks produced by compare.py into known divergences.

compare.py says *that* geas and QuestViva disagree; this says *how often* each
kind of disagreement happens across the corpus, so a one-line engine fix can be
told from a one-game oddity.  Anything that matches no bucket is listed under
"unclassified" -- that is the pile worth reading by hand.

Usage: triage.py [label ...]
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")

# (bucket, side, regex).  side is '-' for a geas-only line, '+' for a qv4-only
# one; a hunk line matching any rule is credited to that bucket.
RULES = [
    # The two blanket "You are in ..." rules that used to sit here are gone
    # with finding 1: now that geas prints the stop, a rule matching every
    # qv4 room line swallowed whatever else the twins disagreed about -- a
    # room name of Quest's that geas never printed, an escape geas ate.  The
    # stop is credited in classify() instead, and only against a twin that is
    # the same line give or take that one character.
    # There used to be four rules here for `wait <message>` prompts that only
    # QuestViva printed -- every wording of "press a key" an author might use.
    # They are gone because the asymmetry is: the runner's `wait_keypress` now
    # prints the message, the way GeasGlk and Quest do, and the 525 lines the
    # rules used to absorb went with it when the goldens were re-blessed.
    # A rule for the pre-2.80 characters line (finding 11) used to be here, and
    # two more in classify() for the two object lines it changes the shape of.
    # All three are gone: the finding is fixed, geas prints the characters line
    # itself, and the bucket emptied when the goldens were re-blessed.
    ("exits: out and dirs split", "+", r"^You can go out to .*\. You can go .*\.$"),
    # The 4.10 line folds directions and places together, so it has both a
    # place ("to ...") and a separator.  The negative lookahead keeps the
    # pre-4.10 two-sentence line above from being taken for it.
    ("exits: 4.10 single line", "+",
     r"^(?!.*\. You can go )You can go (?=.*\bto )(?=.*(,| or )).*\.$"),
    ("exits: Oxford comma", "-", r"^You can go to .*, or .*\.$"),
    # geas drops an empty `place <>`; Quest keeps it and lists it under its own
    # empty name, so the exits line stops after the "to" (finding 46).
    ("46, empty place dropped", "+", r"^You can go to \.$"),
    # MaDbRiT's type library -- the only one that says "move" rather than "go"
    # -- had a rule here, and two more in classify() for the room line and the
    # exits line its `description { do <TLProomDescription> }` override
    # rewrites.  Finding 19 is fixed: the library's `!addto game` block is read
    # now, geas runs the same override, and the bucket is empty.
    ("container contents not listed", "+", r"^(It contains |[A-Z].* contains ).*\.$"),
    ("badthing/baditem hard-coded", "-", r"^You don't see any .*\.$"),
    # Two `take: already-have answer` rules stood here for findings 22 and 27,
    # which are fixed; both wordings now come out of the same engine branch.
    ("disambiguation menu wording", "-", r"^Which .* do you mean\?$"),
    ("disambiguation menu wording", "+", r"^Please select which .* you mean:$"),
    ("cp1252 text not transcoded", "-", r"�"),
    # Two rules for finding 51/32 (`$$` stands for a literal `$` in Quest and
    # vanished in geas) used to match Pure Chaos's `msg <You search through the
    # $$1000000 …>`, the corpus's only one.  Both are fixed and gone.
    ("65, inventory order below 2.80", "+", r"^You are not carrying anything\.$"),
    ("QuestViva defect", "+", r"^\[An internal error occurred\]"),
    # Quest replaces a whole parameter with "<ERROR>" when a conversion
    # character has no partner.  geas's runtime evaluator does the same, but
    # static_eval cannot: the result is re-emitted as a `properties <...>` line
    # and next_token would truncate "<ERROR>" at its '>'.  Documented at
    # geasfile.cc:899-923.
    ("<ERROR> not produced at load time (known)", "+", r"^<ERROR"),
    ("take: implied remove (QV defect)", "-", r"^\(first removing .* from .*\)$"),
    ("take: no container-access gate", "+", r"^You can't take .* - inside closed .*\.$"),
    ("error <defaultake> is spelt with one t", "+", r"^You pick .* up\.$"),
    # A rule for finding 33 -- Quest prints an unrecognised `|` code as a
    # literal bar and keeps reading from the next character, where geas dropped
    # both -- lived here, with its `eat_pipes` helper and a second rule in
    # classify() for the geas side of each pair.  Fixed, and the bucket emptied
    # with it; barcodes.asl is the fixture that keeps it fixed.
]
RULES = [(b, s, re.compile(r)) for b, s, r in RULES]


# A line that a hunk deletes and re-adds unchanged has not changed at all: it
# has moved past its neighbours.  Both exit lines do that when the places line
# and the directions line come out in the opposite order.  Both halves of the
# move are credited, as everywhere else here: the counts are raw diff lines, not
# distinct texts.
MOVED = re.compile(r"^You can go (to )?\w.*\.$")

def maskhi(text):
    """The text as geas's raw cp1252 bytes read back as UTF-8: one U+FFFD each."""
    return "".join("\ufffd" if ord(c) > 127 else c for c in text)


# Quest's stock BadThing / BadItem text, the counterpart of geas's hard-coded
# "You don't see any <noun>."
BADTHING = re.compile(r"^I can't see that (here|anywhere)\.$")

# An exits line that lists `out` as one of the directions, which is the 4.10
# form -- "You can go out.", "You can go out or down.", "You can go north, out
# or to the Hall.".
OUTWORD = re.compile(r"^You can go (.*[ ,])?out\b")

# The default contents listing: "It contains a and b." and its `article` forms.
LOOKCMD = re.compile(r"^> (open|look at|look in|x|examine) ")

CONTAINS = re.compile(r"^(It|They|[A-Z].*) contains? .*\.$")

# Quest 4 put a `picture <file; caption>` in a popup window with the caption as
# its title; QuestViva has no popup window, so ShowPicture prints the caption
# into the transcript instead (V4Game.Part2.cs:3660-3685, and its comment says
# as much).  geas shows the image and no caption, which is what Quest did.  The
# ten lines below are every caption in the corpus that reaches a diff; they are
# listed rather than pattern-matched because a caption is arbitrary prose, and
# they are tested before the desync tables so that a game that later parts
# company still has them counted here.
PICTURE_CAPTIONS = frozenset((
    "Oriental Rug",                                     # Burglary
    "Defenders of Gondor",                              # DefendersOfGondor
    "The Great River, looking eastwards towards Mordor.",
    "The Crossroads",
    "Minis Morgul, city of the Ringwraiths...",
    "First clue",                                       # The Lazst Resort
    "Second clue",
    "Third clue",
    "Barroom dartboard",
    "Kids' crossword",
))


# Finding 14, in its local form: the object is only reachable in geas, so the
# turn that names it answers in geas and refuses in Quest, and the transcripts
# pick up again on the next command.  Each entry lists the ASL *names* the
# walkthrough types for an object that has an `alias` -- names Quest never
# matches, because its exact pass looks at the alias and the alt-names only and
# its abbreviation pass falls back to the alias too.  Derived from the decoded
# sources: every object with an alias whose own name is neither one of its
# valid names, nor reachable as a word-initial abbreviation of any valid name
# in the game (finding 48), and which the walkthrough actually types.
LOOSE_NAMES = {
    "BirthdayAssignment": ("bartender", "kevin"),
    "BrinyBlue": ("rock pile",),
    # `define object <Card> alias <Chloé's buisness card>`: geas answers to the
    # definition name, Quest at asl 310 only to the alias.
    "BrokenMirror": ("card",),
    "Enterprising": ("useless card",),
    "PureChaos": ("box2", "dws"),
    "ShadowMasters": ("door to hotel", "door to men's restroom"),
    "SocialStudies": ("letter",),
    "Sutekh": ("sute stonehouse",),
    "TreasureHunt": ("silver bracelet",),
    "YouAreATiger": ("phone",),
}


# Finding 50/18 -- geas recomputing `quest.lookdesc` and
# `quest.formatroom` only on arrival, so a `property <that room;
# look=...>` applied while standing in it was not seen until the player
# left and came back -- had its pair of Beam wordings tabulated here.
# Fixed: every room display rebuilds them, as ShowRoomInfo does.


# Whole-game desyncs.  Each of these transcripts parts company on one command,
# for one reason already run to ground, and never comes back: from there on the
# two engines are playing different games, so every later line is that single
# finding played out rather than a divergence of its own.  Crediting the tail to
# its cause is what keeps "unclassified" a list of things still unexplained --
# five games were 60% of it.  Only games whose cause has actually been proven
# belong here; the anchor is the first command whose answers disagree -- or, when
# that command falls outside the diff's context lines, the first line of the hunk
# it opens.
DESYNC = {
    "SomethingBoutAHex": ("> take 9volt battery",
                          "desync: 14, names matched too loosely"),
    "Nearco": ("> use casco on prisma",
               "desync: 14, names matched too loosely"),
    "Shipwrecked": ("> take ceramic tile41",
                    "desync: 14, names matched too loosely"),
    "Wizard": ("> take yellow bead",
               "desync: 22, lookup not scoped per verb"),
    "BlightOfElantria": ("> take bronze key",
                         "desync: 45, open marks a container seen"),
    "MetalSonicsQuest": ("> take door",
                         "desync: 37, an object name is trimmed"),
    # Kingdom used to part company here, on the `$rand(10;20)$` its army and
    # its treasury are rolled from (finding 17).  Both are evaluated at load
    # now, and its transcript is identical to Quest's from the first line to
    # the last, so there is nothing left to anchor.
    # QuestViva ticks the timers at the turn's first *suspension* -- a menu, a
    # `wait`, a sync `playwav` -- so a `timeron` reached after that point sets
    # BypassThisTurn too late and loses a turn.  ZombiesAttack arms its timer
    # inside a menu choice; On The Far Blue arms it from the room script of a
    # `goto` that follows a `wait <Press any key.>`.
    "ZombiesAttack": ("The man grunts something inhuman and pulls himself up. "
                      "He launches himself at you, you barely miss it. "
                      "Something is messed up with him, you have to attack him "
                      "with something!",
                      "desync: timer armed after the turn's tick"),
    "OnTheFarBlue": ("A huge shark attack's your raft! stop him now before "
                     "it's to late!.",
                     "desync: timer armed after the turn's tick"),
    # `use cocktail` opens the bartender menu; choice 6 runs `do <jack make a
    # drink begin>`, which ends in `timeron <drinktimer>` (interval 30).  The
    # menu is the turn's suspension, so QuestViva has already ticked by the time
    # the timer is armed and the drink arrives one turn late -- and every timer
    # line after it is off by that turn.
    # Four timers hand off to one another inside the machine, and the first of
    # them is armed from a room script that has already suspended on a
    # `pause <3000>` -- so QuestViva's BypassThisTurn is set after that turn's
    # tick and survives into the next one.  The whole chain runs a turn late,
    # and by the closing `out` geas has landed (playerwin) while Quest is still
    # in the atmosphere (playerlose).
    # Anchored on the machine's own description, which is the head of the first
    # hunk: that is where the extra `look` shows the chain has already slipped a
    # turn, several screens before the deceleration that makes it fatal.
    "Beam": ("You are inside the container docking machine under a mess of "
             "unidentifiable equipment in a crawlspace that probably was not "
             "intended as such. There is an opening near your head.",
             "desync: timer armed after the turn's tick"),
    # The party's guest list is a chain of five-turn timers, and the first is
    # armed by `procedure <party>` immediately after its `wait <press a key>` --
    # so QuestViva arms it after that turn's tick and the whole party runs a
    # turn behind.  Every `give drinks to <guest>` in the walkthrough is then
    # aimed at whoever was in the room a turn ago, the drinks tally comes out
    # lower, and the tuxedo it pays for is never won.
    "ShiverswordTales": ("> give drinks to sir dwane",
                         "desync: timer armed after the turn's tick"),
    "The Lazst Resort": ("> use cocktail",
                         "desync: timer armed after the turn's tick"),
    # The turn before this one is `shine light on spider`, which is
    # `exec <use flashlight on ...>` behind a stun flag; the extra afterturn
    # burns the stun a turn early, the spider gets a free move, and it catches
    # and kills the player two turns later.  The `> northwest` that actually
    # diverges is outside the hunk's context, so anchor on the hunk's head.
    "Things": ("You are in The Smelting Plant (the center)",
               "desync: 47, `exec` re-runs the turn scripts"),
    # `door to men's restroom` is an ASL name behind an alias (finding 14), and
    # prying it open is how the walkthrough is handed its room key.  Without the
    # key Quest is locked out of the hotel for the rest of the game.
    "ShadowMasters": ("> use crowbar on door to men's restroom",
                      "desync: 14, names matched too loosely"),
    # One of the eight treasures is `define object <Silver Bracelet>` behind
    # `alias <Silver>` (finding 14).  Quest never picks it up, so the safe room
    # is one treasure short, the rainbow key is never handed over, and the
    # second half of the maze is closed to it.
    "TreasureHunt": ("> take silver bracelet",
                     "desync: 14, names matched too loosely"),
    # `dws` -- `define object <dws>` behind `alias <disc with square>` (finding
    # 14) -- is the key to the observatory door, and Quest cannot see it.  The
    # observatory holds the telescope and the floor symbol, so everything the
    # walkthrough does up there answers "I can't see that here."
    "PureChaos": ("> use dws on observatory door",
                  "desync: 14, names matched too loosely"),
    "GhostLight": ("> use chalk on grave",
                   "desync: 48, abbreviations match word-initially"),
    # Finding 48 compounds this one a dozen lines later -- Quest's abbreviation
    # pass reads "post" as "Poster" too and puts up a menu that eats the next
    # command -- but the tube is where the two transcripts first disagree.
    "SirLoin2": ("> take tube3",
                 "desync: 14, names matched too loosely"),
    # `x bartender` is the first of two objects this walkthrough names by their
    # ASL name (finding 14).  Quest cannot see either of them, so it never gets
    # the bartender's menu -- and `punch bartender`, the script that walks the
    # player out of the tavern, does not run either, so from here the two
    # transcripts are in different rooms.
    "BirthdayAssignment": ("He's cleaning the glasses.",
                           "desync: 14, names matched too loosely"),
    "MichaelsGame": ("> look at louvre key",
                     "desync: 14, names matched too loosely"),
    # The Jane Eyre book -- inside the goth, who is a closed container -- used
    # to be taken by geas and refused by Quest (finding 26), and parted the two
    # transcripts a third of the way in.  With the gate in place they run
    # together until `drop marzipan`, which is where Sutekh is destroyed in
    # geas and merely littered on in Quest: the blob's `action <drop>` runs in
    # geas, and Quest's ExecDrop never looks at an action (finding 72).  The
    # trapdoor stays shut, so the whole of the game's last scene -- the
    # drawing-room, the scroll, the record player, the ending -- is geas's
    # alone.
    "PyramidOfTerror": ("> drop marzipan",
                        "desync: 72, `action <drop>` runs where Quest drops"),
    # Five rooms all called some spelling of "Asteroid Surface", chained by
    # `place <Asteroid Surface2>` and aliased back to "Asteroid Surface".  At
    # asl-version 210 Quest lists and answers to the raw room name (finding 54),
    # so every `go to asteroid surface` in the walkthrough is refused and the
    # game stays on the first surface while geas walks the chain to the end.
    "Uranus": ("You can go to Asteroid Surface.",
               "desync: 54, no pre-2.80 room display"),
    # `place <Science Lab>` into a room aliased "Sciece Lab" (the author's typo).
    # geas answers to the alias, Quest at 210 to the name, and the walkthrough
    # types the alias.
    "Space": ("You are in the Sciece Lab.",
              "desync: 54, no pre-2.80 room display"),
    # `place <the; Sword and Staff Inn>` and a scripted `place <the village;
    # Sorcerers House>`; the walkthrough's `southeast` out of the plant room is
    # where the two engines' idea of the exits first costs a move.
    "MagicSword": ("The plant sprouts more tendrils!",
                   "desync: 54, no pre-2.80 room display"),
    # `place <THE ENGLISH>` into a room aliased "The english pub".  BrokenMirror
    # is asl-version 310, below the 311 that turns the substitution on, so Quest
    # advertises and answers to "THE ENGLISH" and the walkthrough's `go to the
    # english pub` never gets in.
    "BrokenMirror": ("You can go to The english pub.",
                     "desync: 53, place alias not version-gated"),
    # `sinking in mire` steps its own interval 15 -> 10 -> 5, and geas re-arms
    # from the old value before running the action (finding 52), so every
    # message in the bog is a cycle late and the rescue arrives four turns after
    # Quest's -- late enough that the walkthrough's last four `struggle`s are
    # answered from a different room.
    "PilgrimsProgress": ("Because of the burden that is on your back, you "
                         "begin to sink in the mire ...",
                         "desync: 52, `set interval` applies a cycle late"),
    # Sir Loin 3 used to part company on `count steps`, whose answer is a
    # `value <$rand(...)$>` geas read as 0 (finding 49, the same bug as 17).
    # Fixed: the count, the lock's combination and every roll after them agree,
    # and the two lines still in its diff are the bees, one cut-scene apart --
    # finding 57, in WAIT_TICK below.
}


def desync_at(label, lines):
    """The diff line number this game parts company on, or None."""
    anchor = DESYNC.get(label)
    if not anchor:
        return None
    for n, line in enumerate(lines):
        if line[1:].strip() == anchor[0]:
            return n
    return None


# ARTICLE/TOWORD and the noart() and inserted_to() helpers stood here: they
# normalised the two halves of an exits line that differed only by the
# destination room's prefix, for the pair of finding 24 rules further down.
# Both rules are gone, and nothing else needed them.


# The 4.10 one-liner: directions and places in a single sentence.
FOURTEN = re.compile(r"^(?!.*\. You can go )You can go (?=.*\bto )"
                     r"(?=.*(,| or )).*\.$")


# "You can go north, northeast or northwest." -- the direction list itself, so
# that a line whose directions are the same set in a different order can be told
# from one that gained or lost an exit.
DIRLINE = re.compile(r"^You can go ([^.]*)\.$")


def dirlist(text):
    m = DIRLINE.match(text)
    if not m:
        return None
    return [d.strip() for d in m.group(1).replace(" or ", ", ").split(",")
            if d.strip()]


PICKUP = re.compile(r"^You pick .* up\.$")
OPTION = re.compile(r"^(\d+\) )")
CHOICE_ECHO = re.compile(r"^> \d+$")
WHICH = re.compile(r"^Which .* do you mean\?$")


# The corpus games that declare an asl-version below 280 and so are shown
# through Quest's ShowRoomInfoV2 (finding 54).
PRE280 = frozenset((
    "BlackForest", "CertainOscar", "DevilsBargain", "DreamWeaver", "EasyMoney",
    "FadeToWhite", "Hobbit", "Koww", "Lovesong", "MagicSword", "RomanticMusic",
    "Space", "Uranus",
))

# Names whose prefix or article a `type <...>` line overwrites in Quest because
# the type is included after the tag that set it (finding 56).  Both spellings
# are listed: the geas one and the Quest one.
TYPE_ORDER = {
    "Sleepover": ("a black sweater", "a sweater", "a Supergirl cami", "a cami",
                  "red panties", "a pair of panties",
                  "a pair of red shorts", "a pair of shorts"),
}

# `|w` waits without ending the line in geas, so the sentence after the code is
# stuck onto the one before it, where Quest starts a paragraph (finding 64).
# The pair is (what geas runs together, what Quest puts on either side of the
# break) -- both qv4 halves, because the prompt this one waits on is inside the
# `msg` string rather than a `wait <...>` of its own, so printing the runner's
# wait messages does not reunite them.
BARW = {
    "MagicSword": (("The leader laughs at you, and says something to the "
                    "others. They all start pulsing red, and their forms "
                    "starts changing. Press any key to continue They grow "
                    "taller, and wings sprout from their backs.",
                    ("The leader laughs at you, and says something to the "
                     "others. They all start pulsing red, and their forms "
                     "starts changing. Press any key to continue",
                     "They grow taller, and wings sprout from their "
                     "backs.")),),
}

# `msg nospeak <...>`: Quest ignores the modifier and prints, geas rejects the
# statement and drops the line (finding 42).  One of the corpus's five sites is
# on a replayed path.
NOSPEAK = {
    "TheFormer": ("That? Hmm.... Well, it has Dectyne-type wiring, "
                  "so this had to have come from a 7.",),
}

# Games whose timer-driven cut-scenes land a turn apart because Quest ticks at
# the turn's first `wait` and geas ticks at the end of the turn (finding 57).
WAIT_TICK = frozenset(("KingsQuestV", "SirLoin3"))

# The two lines the same shift moves without leaving a twin in the hunk: the
# rope the rat gnaws free a turn apart, and the wolf that takes Cedric in the
# middle of the harp scene rather than at the end of the turn before it.
WAIT_TICK_LINES = {
    "KingsQuestV": ("The coil of rope lies on the basement floor.",
                    "While Graham is trying to figure a way down the snowy "
                    "slope, a wolf comes running"),
}

# Lines the game itself prints from `#quest.formatobjects#` after changing what
# is visible, so that Quest's copy is the flat comma list UpdateObjectList left
# behind rather than the punctuated one (finding 15).
FORMATOBJECTS = {
    "Sleepover": ("electrical outlets",),
}

# Room listings that differ by one object because a lookup finding 14 let
# through ran the `show`/`reveal` that put it there.  Social Studies: `open
# letter` names the object, not its `Envelope` alias, so qv4 never reveals the
# Note.
LOOSE_KNOCKON = {
    "SocialStudies": ("Big Desk, an Envelope",),
}

# Descriptions only geas can reach, because below ASL 280 Quest resolves a
# look/examine noun by an exact, room-scoped, alias-blind definition-name match
# (finding 62).  Blackforest's boiler is `define object <Boiler >` and its abyss
# is `define object <The Abyss>`; both are unreachable in qv4.
PRE280_LOOK = {
    "BlackForest": ("A black and green copper and cast-iron behemoth",
                    "The edge of the world breaks off jaggedly"),
}

# Refusals from a `place locked <dest; message>` exit geas never parsed
# (finding 63): geas has no such exit, so `go to <dest>` is `badplace`, and the
# room's exits line is one destination short.  Each entry is (the refusal Quest
# prints, the destination missing from geas's exits line).
PLACE_LOCKED = {
    "SkateUrAssOff": ("You have yet to unlock this Skate Zone.",),
}
PLACE_LOCKED_DEST = {
    "SkateUrAssOff": ("Half Pipe Central",),
}

# Object lists a custom room display built from `#quest.objects#`, which geas
# joins with " and " where Quest uses a comma (finding 15).  Only the qv4 half
# needs naming here -- the geas half carries a cp1252 byte and is caught above.
QUEST_OBJECTS = {
    "Nearco": ("Aquí hay:",),
}

# Output geas suppressed because it obeyed a parameterised `outputoff <>`, which
# Quest does not recognise at all (finding 40).  Mysts's Cave bounces the player
# back to the Plateau between the two statements, so Quest reprints the room.
OUTPUTOFF_PARAM = {
    "Mysts": ("A small plateau in the side of the cliff.",),
}

# A `use on <Name >` whose partner object geas registered trimmed, so the handler
# never matches and `defaultuse` answers instead (finding 37).  The pair is
# (geas's refusal, the script Quest ran).
USE_ON_PADDED = {
    "LondePerplex": (("You can't use that here.",
                      "You have used your rage against the defenceless warrior."),),
}

# Turn-script output Quest printed a second time because an `exec <...>` re-
# entered ExecCommand (finding 47).  Dark Hills' living room answers `if ask <Do
# you want to save?>` with `exec <save; normal>`, so its `beforeturn` runs again
# inside the `n` turn; Things doubles a `take`/`spray` wrapper's room scripts.
EXEC_TURNSCRIPT = {
    "DarkHills": ("A new exit has become available!",),
    "Things": ("The vine pulls you closer to the gypsum pile.",
               "The bear rakes you with it's claw",
               "The bear bites at you and draws it's head back"),
}

# Output from an `if property <obj; name=value>` that came out true in geas and
# is always false in Quest (finding 43).  Blade Sentinel's droid menu assigns the
# very pair its `afterturn` asks about.
PROPERTY_PAIR = {
    "BladeSentinel": ('A man voice says: "Hey Skippy, will you open for me?"',),
}

# Output geas printed and QuestViva lost to one of its `_objs[0]` null
# dereferences (see "QuestViva's own defects").  Freshman Fantastic's Tom writes
# `#name:#`, a property shortcut naming an object that does not exist.
QV_NULL_OBJ0 = {
    # The third line is the unset-string sentinel of finding 28, which the
    # game's own script prints on the end of Tom's speech; it is only in the
    # diff because QuestViva threw instead of speaking at all.
    "FreshmanFantastic": ('"Hey there, that Dimplebottom\'s something, huh?"',
                          '"Say, do me a favor? This homework is kicking',
                          '!'),
}

V2_SHAPES = tuple(re.compile(p) for p in (
    r"^You can see .* here\.$",
    r"^There is .* here\.$",
    r"^There is nobody here\.$",
    r"^You can go [^.]*\.$",
    r"^You can go to .*\.$",
    r"^You can go out to .*\.$",
))


# Set per file by main(): true when the diff adds and deletes the same number
# of `> ` echoes, so no echo has actually gone missing.
ECHOES_BALANCED = False

# Set per file by main(): {(side, text): [line number, ...]} over the whole
# diff, so that a rule can ask for a line's twin outside its own hunk.
POSITIONS = {}


def twinned(side, text, lineno, window=40):
    """True when the other side prints this same line close by.

    A shift that moves a whole block past a cut-scene leaves the block on both
    sides of the diff; when the two halves land in one hunk `others` finds the
    twin, and when diff splits them apart this does.  The window keeps it to a
    local shift: a line the game merely prints twice, far apart, is not one.
    """
    flip = "-" if side == "+" else "+"
    return any(abs(n - lineno) <= window
               for n in POSITIONS.get((flip, text), ()))


def classify(line, mates, in_inventory, others=(), prev="", cmd="",
             same=(), aborted=False, onesided=False, label="", lineno=0):
    side, text = line[0], line[1:].strip()
    if not text:
        return "blank-line placement"
    # Both engines echo every command in the script, so the two transcripts
    # always hold the same echoes in the same order.  When one shows up in a
    # diff anyway -- and the counts still balance -- it is diff alignment, not
    # output: a cut-scene that arrives a turn apart pushes the echo around it
    # onto one side and its twin further down onto the other.
    if (ECHOES_BALANCED and text.startswith("> ")
            and not CHOICE_ECHO.match(text)):
        return "command echo realigned by a shifted cut-scene"
    # Once QuestViva has thrown, the rest of that game's transcript is its own
    # wreckage, not a geas difference: the geas output that never arrives, and
    # equally the leftover script lines Quest re-reads as commands because the
    # menus that should have eaten them are gone.  Both sides, all of it -- bar
    # the error line itself, which is the finding.
    if aborted and text != "[An internal error occurred]":
        return "after a QuestViva internal error"
    # geas's implied removal reports itself twice: the parenthetical, then the
    # `defaultremove` text.  All 174 of these follow one.
    # geas's implied removal reports itself twice: the parenthetical, then the
    # `defaultremove` text -- or, where the container carries a `remove <…>` tag
    # of its own, whatever that says (Sir Loin's nosebag: `you take it`).
    if side == "-" and prev.startswith("(first removing "):
        return "take: implied remove (QV defect)"
    # Quest routes an unrecognised command that starts with "the " into ExecOops,
    # which does nothing at all when no correction is pending; geas parses it as
    # an ordinary command and reaches the bad-command error.
    if (side == "-" and cmd.lower().startswith("> the ")
            and text.startswith("I don't understand your command.")):
        return "oops/the: Quest answers nothing"
    # The geas half of the <ERROR> pair: geas prints the author's text where
    # Quest stored the literal "<ERROR>" at load time.
    # The empty-inventory answer belongs with the list itself.
    if text in ("You are carrying nothing.", "You are not carrying anything."):
        return "65, inventory order below 2.80"
    if side == "-" and any(o.startswith("<ERROR") for o in others):
        return "<ERROR> not produced at load time (known)"
    # Two rules for finding 28 -- geas answering "!" for an unset string
    # variable or an out-of-range array element where Quest answers "" -- used
    # to pair the two halves of each such hunk here.  Fixed; the sentinel no
    # longer reaches game text.
    # The other side of the cp1252 rule below: the qv4 line is the same text with
    # the character decoded, so masking its non-ASCII back out reproduces the
    # geas line exactly.
    if side == "+" and any(c > "\x7f" for c in text) and maskhi(text) in others:
        return "cp1252 text not transcoded"
    if MOVED.match(text) and text in (mates if side == "-" else others):
        return "exits: places after directions"
    # Quest prints the out-exit and the directions as one line, geas as two, so
    # the rule above only sees the qv4 half.  Credit the two geas lines it was
    # built from: each is a piece of that one line.
    if side == "-" and text.startswith("You can go ") and any(
            o.startswith(text + " ") or o.endswith(" " + text)
            for o in others if o.startswith("You can go out to ")):
        return "exits: out and dirs split"
    # Two rules for finding 24 -- an exits line whose two halves differ only by
    # the destination room's prefix, either an article (noart) or something
    # longer like The Things That Go Bump In The Night's `prefix <A large>`
    # (inserted_to) -- lived here.  Fixed, along with the out-exit prefix King's
    # Quest V showed once the type library started rendering its rooms.
    # From 4.10 `out` is a bare compass word inside the one exits line, where
    # geas still writes the pre-4.10 "out to <room>" line of its own.  Pair the
    # two halves within a hunk; the ". You can go" test keeps the pre-4.10 shape
    # handled just above from being swallowed here.
    if (side == "-" and text.startswith("You can go out to ")
            and any(OUTWORD.match(o) and ". You can go" not in o
                    for o in others)):
        return "exits: 4.10 single line"
    if (side == "+" and OUTWORD.match(text) and ". You can go" not in text
            and any(o.startswith("You can go out to ") for o in others)):
        return "exits: 4.10 single line"
    # An `out { ... }` script has no destination, so geas -- which still writes
    # the pre-4.10 "You can go out to <room>." line -- has nothing to write and
    # leaves the exit out of the transcript altogether.  Quest at 4.10 names the
    # direction, not the room, so it lists `out` like any other compass word.
    # The signature is one line against one line, identical but for the `out`.
    if (dirlist(text) is not None
            and any(dirlist(o) is not None
                    and [d for d in dirlist(o) if d != "out"] == dirlist(text)
                    and "out" in dirlist(o) for o in others)):
        return "exits: 4.10 single line"
    if ("out" in (dirlist(text) or ())
            and any(dirlist(o) is not None
                    and [d for d in dirlist(text) if d != "out"] == dirlist(o)
                    for o in others)):
        return "exits: 4.10 single line"
    # geas's directions line and its places line against the one 4.10 line that
    # replaces both.  Two geas lines to one qv4 line is the signature; the qv4
    # line is left to the rule above.
    if (side == "-" and text.startswith("You can go ")
            and sum(1 for m in same if m.startswith("You can go ")) > 1
            and any(FOURTEN.match(o)
                    or (OUTWORD.match(o) and ". You can go" not in o)
                    for o in others)):
        return "exits: 4.10 single line"
    # The other side of the same answer, but only where the geas line it
    # replaces is in the same hunk: once a game has diverged, Quest answers
    # "I can't see that here." to half the walkthrough, and that is cascade
    # rather than this finding.
    if (side == "+" and BADTHING.match(text)
            and any(o.startswith("You don't see any ") for o in others)):
        return "badthing/baditem hard-coded"
    # Findings 18/50 (a room's look text cached on arrival, with its
    # STALE_ROOM_LOOK table of Beam's two wordings), 22/27 (the already-have
    # answer, on both sides) and 11 (the pre-2.80 characters line, on both
    # sides) had five rules here between them.  All three are fixed and all
    # three buckets are empty, so the rules are gone.
    # Finding 1: Quest's default room line ends in a full stop and geas's did
    # not.  Credited only against the twin that is the same line plus that one
    # character -- including the room whose own name ends in a stop, which
    # comes out of Quest with two ("You are in Cheese Blvd..").
    if text.startswith("You are in "):
        if side == "-" and (text + ".") in others:
            return "room line: no full stop"
        if side == "+" and text.endswith(".") and text[:-1] in others:
            return "room line: no full stop"
        # Two more rules lived here for a day: findings 66 (a blank
        # `indescription` swallowing the room line, TARDIS Escape) and 67 (the
        # `##` escape eaten in a room alias, RiddleRun).  Both are fixed, both
        # buckets emptied when the goldens were re-blessed, and RiddleRun's
        # transcript is now identical to Quest's.  They are worth remembering
        # as the two bugs that had been hiding inside finding 1's bucket.
    # A rule for finding 20 (same exits, different sequence: geas used its own
    # compass order where Quest uses the order the room's tags appear in, below
    # ASL 2.80 and from 4.10 on) stood here, and two more for the typelib room
    # description of finding 19.  Both findings are fixed.
    # Both engines head the list with "You are carrying:" and, since finding 6,
    # both write one prose sentence under it; what is left is which things go in
    # it and in what order (finding 65).  No pattern of its own identifies those
    # lines, so everything changed between that header and the next prompt is
    # the same finding.
    if in_inventory:
        return "65, inventory order below 2.80"
    # A turn that names an aliased object by its ASL name: geas resolves it and
    # answers, Quest cannot see it (finding 14).  Everything the turn prints on
    # either side belongs to that one difference, so take the whole turn --
    # ahead of the generic rules, which would otherwise pick off the refusal.
    low = cmd.lower()
    if any(" " + n in low for n in LOOSE_NAMES.get(label, ())):
        return "14, names matched too loosely"
    # A tag whose value holds a semicolon, in a game old enough that Quest never
    # mirrors it into the object's property list (finding 55): geas rewrites the
    # line into `properties <look=...>` whatever the version, and the property
    # parser ends the value at the first `;`.  The qv4 line is then the geas line
    # with a semicolon and the rest of the value still attached -- and anything
    # else in the hunk is the tail of the same value.
    minus = same if side == "-" else others
    if any(p.startswith(m + ";") for p in mates for m in minus if m):
        return "tag value truncated at `;`"
    # The geas half of finding 30: the game's own `error <defaulttake; ...>`
    # text, whatever it says, against Quest's built-in "You pick it up."
    if (side == "-" and cmd.lower().startswith(("> take", "> get", "> pick up"))
            and any(PICKUP.match(o) for o in others)):
        return "error <defaultake> is spelt with one t"
    # Quest routes a game declared below asl-version 280 into a room display of
    # its own -- ShowRoomInfoV2, V4Game.Part2.cs:1723-2178 -- which geas does not
    # have (finding 54).  Everything it prints is laid out differently: the
    # characters get their own "You can see ... here." line and say so even when
    # there are none, the objects get a second one, the exits keep their comma
    # before "or" and come out in source order, and `out` prints whatever text
    # its line's first <...> holds.  Any room-display-shaped line in one of those
    # games is that one missing path.
    if label in PRE280 and any(rx.match(text) for rx in V2_SHAPES):
        return "room display: no pre-2.80 path"
    # An object that sets a property and then includes a type that sets the same
    # one: Quest applies the type where it stands, so the type wins, while geas
    # always lets the object's own tag win (finding 56).  What shows is the
    # prefix or the article of one of a handful of garments.
    if any(n in text for n in TYPE_ORDER.get(label, ())):
        return "56, type applied out of source order"
    if side == "+" and text in NOSPEAK.get(label, ()):
        return "42, `msg nospeak` dropped"
    if side == "-" and any(text.startswith(w)
                           for w in QV_NULL_OBJ0.get(label, ())):
        return "QuestViva defect"
    # A container with no `look` of its own: Quest's DoLook prints the contents
    # listing, geas the stock "nothing out of the ordinary".  The listing itself
    # is caught by the CONTAINS rule; this is the line it replaces.
    if (side == "-" and text == "Nothing out of the ordinary."
            and any(CONTAINS.match(o) for o in others)):
        return "container contents not listed"
    if side == "-" and text in PROPERTY_PAIR.get(label, ()):
        return "43, `property <obj; name=value>` true in geas"
    if side == "+" and any(text.startswith(w)
                           for w in EXEC_TURNSCRIPT.get(label, ())):
        return "47, `exec` runs the turn scripts again"
    for refusal, ran in USE_ON_PADDED.get(label, ()):
        if (side == "-" and text == refusal) or (side == "+"
                                                 and text.startswith(ran)):
            return "37, `use on <name >` not trimmed"
    if side == "+" and any(text.startswith(w)
                           for w in OUTPUTOFF_PARAM.get(label, ())):
        return "40, `outputoff <>` obeyed"
    # Nearco's `#quest.doorways.dirs#` line was credited to finding 25 here --
    # the pre-4.10 doorway variables geas kept filling in a 4.10 game.  Fixed:
    # geas leaves them unset from 4.10 on, as Quest does, and Nearco's custom
    # room display now prints the same empty list on both sides.
    for head in QUEST_OBJECTS.get(label, ()):
        if text.startswith(head):
            return "15, quest.objects joins with ` and `"
    for joined, halves in BARW.get(label, ()):
        if (side == "-" and text == joined) or (side == "+" and text in halves):
            return "64, `|w` does not end the line"
    # The options of a disambiguation menu (finding 21): Quest falls back to
    # `Prefix + ObjectAlias` where geas prints the bare alias, so option n reads
    # differently on the two sides while the numbering stays put.
    m = OPTION.match(text)
    if m and any(o.startswith(m.group(1)) for o in others):
        return "disambiguation menu wording"
    # A menu geas offered and Quest did not, because Quest scopes the lookup to
    # one place and found only one candidate (finding 22).  The harness answers
    # geas's menu from the script, so Quest reads that answer as a command.
    if ((side == "-" and (OPTION.match(text) or text.startswith("[choice] ")))
            or (side == "+" and CHOICE_ECHO.match(text))
            or (side == "+" and text.startswith("I don't understand")
                and CHOICE_ECHO.match(prev))):
        if any(WHICH.match(o)
               for o in list(mates) + list(same) + list(others)):
            return "22, lookup not scoped per verb"
        # The same shape, but the menu was the game's own `choose <...>`: the
        # turn that would have opened it failed on the qv4 side (a finding-14
        # lookup, usually), so its `[choice] N` answer arrives at the prompt as
        # a bare command.  Both engines are behaving; only the script is
        # single-threaded across the two runs.
        if any(o.startswith("[choice] ") for o in list(same) + list(others)):
            return "menu skipped, choice typed as a command"
    # Quest ticks its timers the first time a turn suspends, so a cut-scene that
    # a `wait` runs through comes out interleaved and a timer armed after the
    # wait misses one more tick than it does in geas (finding 57).  What that
    # looks like in a diff is a block of turn output that both sides printed,
    # one cut-scene apart: the same text on the other side of the hunk, or --
    # where the shift is wide enough that diff splits its two halves into
    # separate hunks -- the same text on the other side a few lines away.
    if label in WAIT_TICK and (text in others or twinned(side, text, lineno)
            or any(text.startswith(n) for n in WAIT_TICK_LINES.get(label, ()))):
        return "57, timers tick at the wait"
    # The unpunctuated `#quest.formatobjects#` (finding 15).
    if any(n in text for n in FORMATOBJECTS.get(label, ())):
        return "quest.formatobjects: flat comma list"
    # A room listing one object short of geas's because finding 14 let the
    # command that revealed it through.
    if any(n in text for n in LOOSE_KNOCKON.get(label, ())):
        return "14, names matched too loosely"
    # Tardis Escape's three takes of something nested in a carried container
    # were credited to finding 61 here.  Fixed with finding 27: `is_held` walks
    # the container chain, so geas answers `alreadytaken` too.
    # A locked place exit geas dropped at load (finding 63): the refusal, and
    # the room's exits line with the destination missing from it.
    locks = PLACE_LOCKED.get(label, ())
    if locks and (text in locks
                  or (text == "You can't go there."
                      and any(o in locks for o in others))):
        return "63, place locked exit dropped"
    if (side == "-" and text.startswith("You can go ")
            and any(d in o for o in others
                    for d in PLACE_LOCKED_DEST.get(label, ()))):
        return "63, place locked exit dropped"
    # A description qv4 could not reach because the pre-280 look path matches
    # the definition name exactly (finding 62).
    opens = PRE280_LOOK.get(label, ())
    if opens and (any(text.startswith(n) for n in opens)
                  or (text == "I can't see that here."
                      and any(o.startswith(n) for o in others for n in opens))):
        return "62, pre-280 look name lookup"
    # Sutekh's two verbs were credited to finding 60 here -- a bare
    # `action <...>` line with no script, which geas registered and Quest
    # rejects.  Fixed: the like-named property answers on both sides now.
    for bucket, want, rx in RULES:
        if side == want and rx.search(text):
            return bucket
    # Last: `open` runs the whole of DoLook, so Quest prints the object's
    # description and its contents listing; `look at` prints the listing after
    # the description.  Either way geas prints neither, and the listing of a
    # shut container is the game's own `list closed` text, which no pattern can
    # recognise -- so credit output Quest added under a look/open that geas
    # never wrote at all, once every other rule has passed on it.
    if (side == "+" and onesided and LOOKCMD.match(cmd.lower())
            and not text.startswith(("> ", "I can't see", "[An internal"))):
        return "container contents not listed"
    return None


def went_silent(label):
    """Did QuestViva stop producing output after its first internal error?

    Most of the eight games that hit one carry on regardless; Barbarian's
    engine state is wrecked and every later command prints nothing, so the
    rest of that diff is QuestViva's wreckage rather than a geas difference.
    """
    path = os.path.join(OUT, label + ".qv4")
    if not os.path.exists(path):
        return False
    lines = open(path, encoding="utf-8", errors="replace").read().split("\n")
    for i, l in enumerate(lines):
        if l.strip() == "[An internal error occurred]":
            tail = lines[i + 1:]
            cmds = sum(1 for x in tail if x.startswith("> "))
            out = sum(1 for x in tail if x.strip() and not x.startswith("> "))
            return cmds > 20 and out < cmds / 2
    return False


def main():
    want = set(sys.argv[1:])
    counts, games, unclassified, first = {}, {}, [], {}
    for name in sorted(os.listdir(OUT)):
        if not name.endswith(".diff"):
            continue
        label = name[:-5]
        if want and label not in want:
            continue
        hunks, cur, inv, lastcmd = [], [], False, ""
        aborted, dead = False, went_silent(label)
        alllines = open(os.path.join(OUT, name), encoding="utf-8",
                        errors="replace").read().split("\n")
        parted = desync_at(label, alllines)
        global ECHOES_BALANCED, POSITIONS
        POSITIONS = {}
        for n, l in enumerate(alllines):
            if l[:1] in ("+", "-") and l[:2] not in ("++", "--"):
                POSITIONS.setdefault((l[0], l[1:].strip()), []).append(n)
        # A menu answer typed at the prompt is a real extra echo, not an
        # alignment shift, so leave those out of the count.
        echoes = [l for l in alllines
                  if l[:3] in ("+> ", "-> ")
                  and not CHOICE_ECHO.match(l[1:].strip())]
        ECHOES_BALANCED = (sum(1 for l in echoes if l[0] == "+")
                           == sum(1 for l in echoes if l[0] == "-"))
        for lineno, line in enumerate(alllines):
            if line.startswith("@@"):
                hunks.append(cur)
                cur, inv = [], False
                continue
            text = line[1:].strip()
            if text.startswith("> ") or text.startswith("You are in "):
                inv = False
            elif text == "You are carrying:":
                inv = True
            if text.startswith("> "):
                lastcmd = text
            if line and line[0] in "+-" and line[:2] not in ("++", "--"):
                cur.append((line, inv, lastcmd, lineno))
        hunks.append(cur)
        for hunk in hunks:
            added = {l[1:].strip() for l, _, _, _ in hunk if l[0] == "+"}
            # A change is "one-sided" when neither diff line touching it comes
            # from the other transcript: pure added or pure deleted output,
            # rather than one line rewritten as another.
            at = {n: l[0] for l, _, _, n in hunk}
            prev = ""
            for line, in_inv, cmd, lineno in hunk:
                text0 = line[1:].strip()
                # Lines on the *other* side of this hunk, for rules that compare
                # a geas line with its qv4 counterpart rather than test it alone.
                other = {l[1:].strip() for l, _, _, _ in hunk
                         if l[0] != line[0]}
                mine = {l[1:].strip() for l, _, _, _ in hunk
                        if l[0] == line[0]}
                flip = line[0] == "+" and "-" or "+"
                onesided = (at.get(lineno - 1) != flip
                            and at.get(lineno + 1) != flip)
                if (dead and line[0] == "+"
                        and text0 == "[An internal error occurred]"):
                    aborted = True
                if line[0] == "+" and text0 in PICTURE_CAPTIONS:
                    bucket = "picture caption printed as text"
                elif parted is not None and lineno >= parted:
                    bucket = DESYNC[label][1]
                else:
                    bucket = classify(line, added, in_inv, other, prev, cmd,
                                      mine, aborted, onesided, label, lineno)
                prev = line[1:].strip()
                if bucket is None:
                    unclassified.append((label, line))
                    bucket = "unclassified"
                counts[bucket] = counts.get(bucket, 0) + 1
                games.setdefault(bucket, set()).add(label)
                first.setdefault(label, bucket)

    firsts = {}
    for bucket in first.values():
        firsts[bucket] = firsts.get(bucket, 0) + 1
    for bucket, n in sorted(counts.items(), key=lambda kv: -kv[1]):
        print("%5d lines  %3d games  %3s first  %s"
              % (n, len(games[bucket]), firsts.get(bucket, "") or "", bucket))
    if unclassified and os.environ.get("SHOW"):
        print()
        for label, line in unclassified:
            print("%-22s %s" % (label, line[:140]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
