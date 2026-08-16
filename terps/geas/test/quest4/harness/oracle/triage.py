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
    # A rule for the out/directions line split (finding 8) used to be here, and
    # a second in classify() to credit the two geas halves it was built from.
    # Both are gone: geas joins them from 2.80 up now, as Quest does, and the
    # bucket emptied on the next re-bless.
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
    # A rule for the contents listing stood here, with two more in classify():
    # one for the "Nothing out of the ordinary." geas printed in its place, and
    # a catch-all for anything Quest added under a `look at`/`open` that geas
    # did not.  All three are gone with finding 77 -- a bare `surface` line
    # declares a container too, so the listings come out now -- and the last of
    # them was the broadest rule in the file, which is reason enough not to
    # leave it standing over a fixed bug.
    ("badthing/baditem hard-coded", "-", r"^You don't see any .*\.$"),
    # Two `take: already-have answer` rules stood here for findings 22 and 27,
    # which are fixed; both wordings now come out of the same engine branch.
    # Two rules for the disambiguation caption (finding 21) stood here, and one
    # in classify() for the option labels the prefix used to be missing from.
    # All three are fixed and gone; WHICH below now spells Quest's wording,
    # which is geas's too.
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
    # The implied removal's parenthetical used to be geas-only and had a rule
    # here.  It is not a divergence any more: QuestViva's ExecTake never set
    # isInContainer, so the whole removal was dead code, and patch_questviva.py
    # section 13 restores it, and finding 76 makes geas run the removal as the
    # same nested command Quest runs, so nothing is left of it at all.
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

# A LOOKCMD and a CONTAINS pattern stood here for the contents listing -- "It
# contains a and b." and its `article` forms, and the look/open commands that
# print it.  Gone with finding 77's rules above.

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
#
# Thirteen games used to be listed here and in DESYNC below, between them ten
# thousand diff lines, and all any of them proved was finding 14 over and over.
# Their scripts now say what a player would have to say -- TAKE SILVER, not
# TAKE SILVER BRACELET -- which costs nothing, because geas matches everything
# Quest matches and more, and buys the rest of each transcript as evidence.
# The finding itself is pinned by ../../fixtures/loosename.asl instead.
#
# The two left are the two that cannot be reworded.  Both name an object no
# player can reach: PureChaos's `dws` is aliased "disc with square", and USE
# DISC WITH SQUARE ON OBSERVATORY DOOR splits on " with " long before " on ";
# ShadowMasters' Room Key carries `use on <Door to hotel>` and no object of
# that name was ever declared.  geas's loose matching is the only thing that
# gets either game past its door, so a real Quest cannot finish them at all.
LOOSE_NAMES = {
    "PureChaos": ("box2", "dws"),
    "ShadowMasters": ("door to hotel",),
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
    # Nearco, Shipwrecked, SirLoin2, TreasureHunt, MichaelsGame and
    # BirthdayAssignment were all anchored on finding 14 here and are all
    # byte-identical now: their scripts name the objects the way a player would
    # have to (see LOOSE_NAMES).  Something 'Bout A Hex was too, and is the one
    # of the seven with something else underneath -- see below.
    #
    # After the DO NOT DISTURB sign is named by its alias, Hex parted company on
    # a turn that geas spent and Quest did not, twice over: `timeron
    # <christchurch>` (interval 2) from the Christ Church choice of
    # `leavemotellousiville`, then `timeron <get camcorder>` (interval 10) from
    # the `thanks.` choice of `oldbartender3`, both armed inside a `selection`.
    # Both were the tick artefact below, and the harness's deferred tick settled
    # both: the first 2 488 lines are identical now.
    #
    # Two more divergences were underneath them, both finding 14 again, both on
    # lines the rewording had missed because the desync was in front of them:
    # `take 19yellow98tag` (aliased `1 9 y e l l o w 9 8`, revealed at the end
    # of the dream) and `use Jim 2`/`use Jim 3` (both aliased `Jim-n-Ginger`,
    # which is the only name Quest will take and which has exactly one
    # candidate in the inventory each time).  Reworded like the rest, and the
    # game is byte-identical now -- the corpus's largest divergence, 5 333
    # lines at the start of this sitting, is gone without a change to geas.
    # Blight of Elantria used to be here, credited first to finding 45 and then
    # to QuestViva's dead implied-removal path.  Section 13 of
    # patch_questviva.py revives that path and Blight's thousand-line desync
    # went with it, so it has no entry any more.
    #
    # Wizard survived both, on its Misty White Portal: QuestViva used to reach
    # each colour change three turns before geas, so the two engines stepped
    # through into different worlds -- geas a Forest, QuestViva a Small ledge --
    # and never met again, 3 609 lines of it.  That was the harness's tick, not
    # either engine's timer.  `enter <var>` parks the turn and is answered by
    # the *next* SendCommand, so the oracle counted a mid-turn input as a turn
    # and ticked for it; Wizard's library desk asks for a row number and a book
    # letter, which put its 15-turn portal timer three ticks ahead for the rest
    # of the game.  Program.cs skips the tick while QvhAwaitingEnter is set
    # (patch_questviva.py section 14) and the whole cycle lines up.
    #
    # What was left parted company in the church, over the implied removal: the
    # Yellow Bead in the offering basket has an `action <remove>' that refuses
    # it until a donation is made, Quest ran that action and the take failed,
    # geas removed the bead from the basket itself and pocketed it, and from
    # there geas was carrying a bead Quest never handed over.  Finding 76 is
    # fixed -- the implied removal is a nested command now -- and the
    # walkthrough donates on a detour, so Wizard has no entry here either.  Its
    # six remaining lines are finding 77 and are classified one by one.
    # Finding 37's `use on'/`give to' half is fixed; this is its other half,
    # which stands on purpose.  geas trims a `define' header's name and Quest
    # does not, so `define object < door>' answers to `door' here and to nothing
    # a player can type there.  Trimming lets the game be finished, and the cost
    # is this desync.
    "MetalSonicsQuest": ("> take door",
                         "desync: 37, geas trims a declared name"),
    # Kingdom used to part company here, on the `$rand(10;20)$` its army and
    # its treasury are rolled from (finding 17).  Both are evaluated at load
    # now, and its transcript is identical to Quest's from the first line to
    # the last, so there is nothing left to anchor.
    # The whole "timer armed after the turn's tick" bucket used to live here:
    # ZombiesAttack, The Lazst Resort and Something 'Bout A Hex on menus, Beam,
    # ShiverswordTales and On The Far Blue on `wait`/`pause`.  QuestViva ticks
    # at the turn's first *suspension* -- a menu, a `wait`, a sync `playwav` --
    # so a `timeron` reached after that point set BypassThisTurn too late and
    # lost a turn.  geas mirrors the `wait`/`pause` half deliberately
    # (suspend_turn, geas-runner.cc:3742-3749), which settled the second three;
    # the menu half it cannot mirror, because make_choice answers from the
    # script inside the turn.  So the harness defers the tick past a menu
    # instead (Program.cs, `pendingTick`), and the first three settled too:
    # The Lazst Resort is byte-identical, ZombiesAttack is down to the two lines
    # below, and Hex's 5 333 lines fell to 3 604 with an older divergence
    # underneath them.  QV4_EARLY_TICK=1 puts the artefact back.
    #
    # What ZombiesAttack has left is a turn count: `You completed the game in
    # 143 turns` against Quest's 147, off one `afterturn` -- the game counts in
    # that hook -- four times in 143 turns.  Every other line of the transcript
    # is identical, which is what makes it hard to localise; the cause has not
    # been run to ground.  A probe settles that a `selection` is not it: qv4
    # runs one afterturn for a turn that opens a menu, as geas does.
    "ZombiesAttack": ("You completed the game in 143 turns, try to beat your "
                      "best!",
                      "afterturn: qv4 runs four more than geas"),
    # On The Far Blue's room is `define room <Eastern beach of forest island >`
    # and Quest keeps the space it was declared with.  Finding 37's other half,
    # the same one Metal Sonic's Quest is anchored on below.
    "OnTheFarBlue": ("You are in Eastern beach of forest island.",
                     "37, geas trims a declared name"),
    # Things was anchored here on finding 47 -- `shine light on spider` is
    # `exec <use flashlight on ...>` behind a stun flag, and geas ran the
    # afterturn once where Quest ran it twice, so the two engines burned the
    # stun a turn apart and the spider caught the player in one of them.  geas
    # runs the exec'd turn now, its walkthrough was re-derived against the new
    # clock, and the game is byte-identical, so it has no entry any more.
    # The Room Key's third handler is `use on <Door to hotel>`, and no object of
    # that name exists anywhere in the game -- the author wrote the tag and
    # never declared the door.  geas answers it anyway (finding 14 in its widest
    # form: the tag's target is enough of a name), unlocks Courtyard 2's east
    # exit and walks into the hotel.  Quest says "I can't see that here.", the
    # exit stays locked, and there is no other way through it, so a real Quest
    # cannot finish this game.  Nothing can be reworded here; the anchor stands.
    "ShadowMasters": ("> use room key on door to hotel",
                      "desync: 14, names matched too loosely"),
    # `dws` -- `define object <dws>` behind `alias <disc with square>` (finding
    # 14) -- is the key to the observatory door, and the alias cannot be typed:
    # USE DISC WITH SQUARE ON OBSERVATORY DOOR splits on " with " long before it
    # splits on " on ", and the header's own `use disc with square = use disc on
    # square` synonym rewrites the front of it first.  So the only command that
    # opens the door is the one only geas accepts.  The observatory holds the
    # telescope and the floor symbol, so everything the walkthrough does up
    # there answers "I can't see that here."
    "PureChaos": ("> use dws on observatory door",
                  "desync: 14, names matched too loosely"),
    "GhostLight": ("> use chalk on grave",
                   "desync: 48, abbreviations match word-initially"),
    # A PyramidOfTerror entry stood here for `drop marzipan`, where Sutekh was
    # destroyed in geas and merely littered on in Quest: the blob's
    # `action <drop>` ran in geas, and Quest's ExecDrop never looks at an
    # action (finding 72, confirmed in the real 4.1.5 runner).  The rule is
    # faithful and stays; the *walkthrough* was rerouted to THROW MARZIPAN --
    # the game-block `verb <throw>` both engines dispatch -- and the game is
    # byte-identical now.  (Its earlier desync, the Jane Eyre book inside the
    # goth, had already gone with finding 26's gate.)
    # Four entries stood here for the pre-2.80 room display and the place alias:
    # Uranus and Space, whose `place` chains geas walked and Quest refused
    # (finding 54); Magic Sword, whose plant-room exits cost it a move (also
    # 54); and BrokenMirror's `place <THE ENGLISH>` into a room aliased "The
    # english pub" (finding 53).  All four are fixed.  Uranus, Space and
    # BrokenMirror are byte-identical now; Magic Sword's transcripts stay in
    # step the whole way and are down to the three lines below.
    # Not a desync, and not yet a numbered finding: in the Oggy-Waggi plant
    # fight the turn script's `if has <Timer;=0> then ... else set <Timer;-1>`
    # (MagicSwordP1 procedure 201, reached from the afterturn block) reaches
    # zero a turn earlier in Quest, so "The plant sprouts more tendrils!" and
    # the extra attack it unlocks are one round out of phase.  The two sides
    # differ by how many times that block has run by the first blow, so the
    # question is which engine runs the afterturn script on the turn the player
    # walks in.
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
WHICH = re.compile(r"^Please select which .* you mean:$")


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

# A BARW table stood here for finding 64, holding Magic Sword's one `|w` line
# and the two halves Quest broke it into.  It is gone: `print_formatted` flushes
# at the code now, as Quest's Print does, and the bucket emptied on the
# re-bless.

# `msg nospeak <...>`: Quest ignores the modifier and prints, geas rejects the
# statement and drops the line (finding 42).  One of the corpus's five sites is
# on a replayed path.
NOSPEAK = {
    "TheFormer": ("That? Hmm.... Well, it has Dectyne-type wiring, "
                  "so this had to have come from a 7.",),
}

# Finding 77's other two halves, both in Wizard and both about a container's
# contents being somewhere geas does not think they are.  The room line is the
# Tavern's: `add <Label; Bottle of sludge>' happens while the bottle is still
# standing there and `give <Bottle of sludge>' moves only the bottle, so Quest
# goes on listing the label in the Tavern and geas takes it away with the
# bottle.  The bead is the church basket's, seen by a `for each object in
# <room>' whose `exists <...>' test Quest passes and geas's seen gate does not.
# A CONTAINER_ROOM table stood here for finding 77's other two lines -- the
# Wizard's Label, left standing in the Tavern by a `lose` that moved the object
# and not the `parent` property, and the church bead DETECT MAGIC skipped.  Both
# were geas conflating Quest's two answers to "where is this object"; fixed, and
# Wizard is byte-identical now.


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

# Finding 47 -- turn-script output Quest printed a second time because an
# `exec <...>` re-entered ExecCommand, tabulated here for Dark Hills' `exec
# <save; normal>` and Things' `take`/`spray` wrappers.  Fixed: geas runs the
# exec'd command as a turn of its own, hooks included, and both games are
# byte-identical.

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
    # A rule for the line geas used to print after "(first removing ...)" -- the
    # nested `remove' command's own answer, `Done.' 57 times in Wizard -- lived
    # here.  Finding 76 is fixed: the implied removal is a whole nested command
    # on both sides now, and the only "(first removing ...)" left in a diff is
    # Blight of Elantria's, where the whole passage is one-sided for a reason of
    # its own and belongs to that cascade rather than to a rule here.
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
    if side == "-" and text in PROPERTY_PAIR.get(label, ()):
        return "43, `property <obj; name=value>` true in geas"
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
