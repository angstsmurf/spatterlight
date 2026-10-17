#!/usr/bin/env python3
"""Write games/rare-opcodes/SYNTH.DAT, a made-up database for the Plus
interpreter that runs the action opcodes, system commands and condition
guards no real game reaches (README.md has the list), and two variants of it
for the plain text loader: games/loader-edges/EDGES.DAT, with an empty
dictionary slot and a message too long to keep whole, and
games/prefix-title/PREFIX.DAT, whose title is only the start of a known one.

  mkdatabase.py           write the databases
  mkdatabase.py --check   fail if those on disk are not what this writes

The format is that of the MS-DOS games (LoadDatabasePlaintext() in
loaddatabase.c): text, one number or string per line. The loader only takes a
title it knows, so this one calls itself X-Men, the game without pictures.

Almost every test prints what it expects before what it got, so that the
transcript can be read: "ABS expect 5 7 got 5 7".
"""

import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "games", "rare-opcodes", "SYNTH.DAT")
EDGES = os.path.join(HERE, "games", "loader-edges", "EDGES.DAT")
PREFIX = os.path.join(HERE, "games", "prefix-title", "PREFIX.DAT")
TITLE = "X-MEN "
MAX_STRING_LENGTH = 1000       # of a string in the plain text loader
MANIFEST = os.path.join(HERE, "games.manifest.tsv")

CARRIED = 255
NOUN, NOUN2, WITH = 998, 999, 997   # operands that stand for a word of the command
MAX_COMMANDS = 16              # command bytes of one action

# Condition 31, the logic control flags
OR, XOR, NOT, LPAREN, RPAREN, FUZZY, STRICT = [(31, n) for n in (1, 2, 3, 6, 7, 9, 10)]
AND, NOT_ALL, AT_END = [(31, n) for n in (0, 4, 8)]

verbs = ["AUTO"]
nouns = ["ANY"]
messages = ["TESTER",          # message 0 is the name in the prompt
            "low"]             # one that is sure to be among the first 51
actions = []                   # (verb, noun, extra words, conditions, command bytes)


def verb(word):
    if word not in verbs:
        verbs.append(word)
    return verbs.index(word)


def noun(word):
    if word not in nouns:
        nouns.append(word)
    return nouns.index(word)


def val(n):
    """A number that may be 128 or more: two bytes then, high bit set."""
    return [n] if n < 128 else wide(n)


def wide(n):
    """A number in two bytes, whether it needs them or not."""
    return [0x80 | (n >> 8), n & 0xFF]


def msg(text):
    """Print a message: numbers 1-51 are the first messages, and those
    from 128 on, which take two bytes, the rest."""
    if text not in messages:
        messages.append(text)
    n = messages.index(text)
    return [n] if n < 52 else wide(n + 76)


def setc(counter, n):
    return [99] + val(n) + [counter]


def addc(counter, n):
    return [100] + val(n) + [counter]


def printc(counter):
    return [102, counter]


def tok(n):
    """Print the number n (through counter 9)."""
    return setc(9, n) + printc(9)


CONTINUE, LOOP, LOOPBACK, NOP = [73], [112], [113], [0]


def action(v, n, conds, instrs, words=()):
    commands = [b for i in instrs for b in i] or NOP
    assert len(commands) <= MAX_COMMANDS, (v, n, commands)
    actions.append((v, n, list(words), list(conds), commands))


def cont(conds, *instrs):
    """A line that runs after the one before it."""
    action(0, 0, conds, instrs)


def pack(v, n, instrs, words=(), more=False):
    """An unconditional run of commands, over as many lines as it takes."""
    lines, line = [], []
    for i in instrs:
        if sum(map(len, line)) + len(i) > MAX_COMMANDS - 1:
            lines.append(line)
            line = []
        line.append(i)
    lines.append(line)
    for k, line in enumerate(lines):
        first = k == 0
        if first and (more or len(lines) > 1):
            line = line + [CONTINUE]
        action(v if first else 0, n if first else 0, [], line, words if first else ())


def test(name, *instrs, more=False):
    pack(verb("TEST"), noun(name), instrs, more=more)


def on_item(word, *instrs):
    """A command with any noun: VERB LAMP."""
    pack(verb(word), 0, instrs)


# --- The world ---------------------------------------------------------------

LAB, STORE, LIMBO = 1, 2, 3
rooms = [                      # exits n s e w u d, description, picture
    ([0] * 6, None),
    ([0, 0, STORE, 0, 0, 0], "laboratory"),
    ([0, 0, 0, LAB, 0, 0], "storeroom"),
    ([0] * 6, "*I am in limbo"),     # the last room is where the dead go
]

items = [                      # text, room, noun, flags (1 takeable, 2 light)
    ("", 0, 0, 0),             # item 0 is not counted by action 52
    ("lamp", LAB, noun("LAMP"), 3),
    ("coin", STORE, noun("COIN"), 1),
    ("red gem", CARRIED, noun("GEM#RED"), 1),
    ("blue gem", LAB, noun("GEM#BLUE"), 1),
    ("robot", LAB, noun("ROBOT"), 0),
    ("rock", CARRIED, noun("ROCK"), 1),
    ("egg", STORE, noun("EGG"), 1),
    ("key", CARRIED, noun("KEY"), 1),
]
LAMP, COIN, REDGEM, BLUEGEM, ROBOT, ROCK, EGG, KEY = range(1, 9)

BAD_ITEM = 200                 # more than there are items
BAD_SMALL_ITEM = 100           # the same, written in one byte
BAD_COUNTER = 100              # there are 64 counters
BAD_BIT = 100                  # and 64 flags
BAD_ROOM = 100                 # and four rooms
ROOM_COUNTER = 32              # the player's room

adverbs = ["ANY", "NONE", "QUICKLY"]
preps = ["ANY", "NONE", "WITH", "AT", "IN"]   # 2 and 3 start a "with" list

# --- The actions -------------------------------------------------------------

# One automatic action, so that the explicit ones are not first in the table
action(0, 100, [], [NOP])

# Counter arithmetic: 103 ABS, 105 AND, 106 OR, 107 XOR
test("ABS", msg("ABS expect 5 7 got"),
     setc(1, 5), [104, 1], [103, 1], setc(2, 7), [103, 2], [103, BAD_COUNTER],
     printc(1), printc(2))
for name, opcode, want in (("AND", 105, 8), ("OR", 106, 14), ("XOR", 107, 6)):
    test(name, msg(f"{name} of 12 and 10 expect {want} got"),
         setc(1, 12), setc(2, 10), [opcode, 1, 2], [opcode, 1, BAD_COUNTER], printc(2))

# 108: a random number from 0 to 99 (the generator has a fixed seed)
test("RANDOM", msg("RANDOM expect 1 and a number got"), [108, 1], [108, BAD_COUNTER],
     more=True)
cont([(21, 1), (0, 0), NOT, (21, 1), (0, 100)], tok(1))
cont([], printc(1))

# 70 clear screen, 88 and 93 delays (the tests run without delays)
test("CLS", [70], msg("CLS done"))
test("DELAY", [88], setc(1, 2), [93, 1], msg("DELAY done"))

# An opcode that does not exist is skipped, and so is a message that does
# not exist
test("UNKNOWN", [96], [87], wide(400), msg("UNKNOWN done"))

# Every number of a line can be written in two bytes: the opcode (99, 102,
# 109), a counter, a value, and one of the first messages
test("WIDE", msg("WIDE expect 7 0 low got"),
     wide(99), wide(7), wide(1), wide(102), wide(1),
     wide(109), wide(1), [102] + wide(1), wide(messages.index("low")))

# 52 when the hands are full: says so and ends the line
test("CARRY", msg("CARRY expect a complaint then 0 1 got"), setc(42, 2), more=True)
cont([], [52, LAMP], tok(99))
cont([NOT, (1, LAMP)], tok(0))
cont([], setc(42, 5), [52, LAMP])
cont([(1, LAMP)], tok(1), [53, LAMP])

# Operands of 128 and more on the opcodes that read their last operand
# themselves: the item is the one the noun stands for
on_item("WHERE", msg("is in room"), [80] + val(NOUN) + [1], printc(1))
on_item("SEND", [62] + val(NOUN) + [STORE], msg("SEND expect 2 got"),
        [80] + val(NOUN) + [1], printc(1))
# The second operand of 62, 72 and 75 can be two bytes as well: X-Men puts
# items in room 500.
on_item("BANISH", [62] + val(NOUN) + val(500), msg("BANISH expect 500 got"),
        [80] + val(NOUN) + [1], printc(1))
on_item("RETURN", [72] + val(NOUN) + val(NOUN), [75] + val(NOUN) + val(NOUN),
        [62] + val(NOUN) + [LAB], msg("RETURN expect 1 got"),
        [80] + val(NOUN) + [1], printc(1))
on_item("HOLD", [74] + val(NOUN), msg("HOLD done"))
on_item("DROP", [53] + val(NOUN), msg("DROP done"))
on_item("GET", [52] + val(NOUN), msg("GET done"))
on_item("LOSE", [59] + val(NOUN), msg("LOSE done"))
on_item("TRADE", [72] + val(NOUN) + [COIN], msg("TRADE with the coin, they are now in"),
        [80] + val(NOUN) + [1], printc(1), [80, COIN, 2], printc(2))
on_item("JOIN", [75] + val(NOUN) + [ROBOT], msg("JOIN the robot, expect 1 got"),
        [80] + val(NOUN) + [1], printc(1))
on_item("NAME", msg("NAME:"), [85] + val(NOUN), msg("/"), [122] + val(NOUN))
test("NAMES", msg("NAMES expect LAMP and COIN got"), [86, LAMP], [86, COIN],
     [86, BAD_SMALL_ITEM])

# 119: the name in the prompt, as a message below 128 and as one above
test("PROMPT", [119, len(messages) + 1], msg("PROMPT done"))
messages.append("SHORT NAME")
assert messages.index("SHORT NAME") < 52
while len(messages) < 60:
    messages.append("unused %d" % len(messages))
messages.append("LONG NAME")
test("PROMPT2", [119, 128, messages.index("LONG NAME") + 76], msg("PROMPT2 done"))
# The same number written in two bytes although it would fit in one
test("PROMPT4", [119, 128, messages.index("SHORT NAME") + 76], msg("PROMPT4 done"))
test("PROMPT3", [119, 0], msg("PROMPT3 done"))

# 110-9: swap the flags of an item with counter 63. The second operand of
# 110 can be two bytes as well.
on_item("FLAG", msg("FLAG expect 3 8 8 8 got"),
        setc(63, 8), [110, 9] + val(NOUN), printc(63), CONTINUE)
cont([(30, LAMP), (0, 8)], tok(8))
cont([], [110, 9, LAMP], printc(63), [110, 9] + val(BAD_ITEM), printc(63))

# 110-12: the next item at a place goes in counter 30, for a loop.
# Mode 1 the room in counter 63, 2 here, 4 carried.
# These four come first in the script, while everything is where it starts.
for name, mode, want in (("THERE", 1, "coin egg"),
                         ("HERE", 2, "lamp blue gem robot"),
                         ("HELD", 4, "red gem rock key")):
    test(name, msg(f"{name} expect {want} got"), [109, 30], setc(63, STORE), LOOP)
    cont([], [110, 12, mode])
    cont([NOT, (22, 30)], [85] + val(NOUN), addc(30, 1))
    cont([], LOOPBACK, msg("/ end of list"))

# 110-12 mode 3 looks at one item, and has the action table scanned again
# from the top when that one is neither here nor carried. 111 does the same
# after an item is printed, so the first line counts through the items.
test("EITHER", addc(30, 1), more=True)
cont([], [110, 12, 3])
cont([NOT, (22, 30)], [85] + val(NOUN), [111])
cont([(22, 30)], msg("/ end, expect lamp red gem blue gem robot rock key"))

# 115 in a loop inside a loop: leave at the next loopback
test("NEST", msg("NEST expect 1 1 / 1 2 / 2 1 / 2 2 / got"), setc(1, 0), LOOP)
cont([], addc(1, 1), setc(2, 0), LOOP)
cont([], addc(2, 1), printc(1), printc(2), msg("/"))
cont([(21, 2), (0, 2)], [115])
cont([], LOOPBACK)
cont([(21, 1), (0, 2)], [115])
cont([], LOOPBACK, msg("end of both loops"))

# 116 takes the match back, 117 and 118 give the two halves of it again
test("UNMATCH", msg("UNMATCH (116) expect not understood:"), [116])
test("REMATCH", msg("REMATCH (116 117) expect cannot do that yet:"), [116], [117])
test("UNDONE", msg("UNDONE (116 118) expect not understood:"), [116], [118])
test("REDONE", msg("REDONE (116 117 118) expect no complaint"), [116], [117], [118])

# The other system commands. Without pictures the drawing ones do nothing.
test("SYSTEM", msg("SYSTEM"), setc(1, 1), [110, 13, 0], [110, 12, 5], [110, 3, 0],
     [110, 5, 1], [110, 4, 1], [110, 2, 1], [110, 10, 1], [110, 11, 1], [110, 8, 1],
     msg("done"))

# Conditions that name an item, counter, room or flag that does not exist
# are false. Each line negates one, and prints its number.
test("GUARDS", msg("GUARDS expect 1 2 3 5 55 8 13 14 17 18 118 20 21 22 23 123 26 29 30 got"),
     setc(1, 200), more=True)
for n, conds in (
        (1, [(1, BAD_ITEM)]),
        (2, [(2, BAD_ITEM)]),
        (3, [(3, BAD_ITEM)]),
        (5, [(5, BAD_COUNTER), (0, 0)]),
        (55, [(5, 1), (0, 0)]),             # counter 1 is not a room
        (8, [(8, BAD_BIT)]),
        (13, [(13, BAD_ITEM)]),
        (14, [(14, BAD_ITEM)]),
        (17, [(17, BAD_COUNTER), (0, 2)]),
        (18, [(18, BAD_COUNTER), (0, 2)]),
        (118, [(18, 2), (0, BAD_COUNTER)]),
        (20, [(20, BAD_COUNTER), (0, 0)]),
        (21, [(21, BAD_COUNTER), (0, 0)]),
        (22, [(22, BAD_COUNTER)]),
        (23, [(23, BAD_ITEM), (0, 2)]),
        (123, [(23, LAMP), (0, BAD_COUNTER)]),
        (26, [(26, BAD_ITEM), (0, 1)]),
        (29, [(29, BAD_ITEM), (0, 1)]),
        (30, [(30, BAD_ITEM), (0, 1)])):
    cont([NOT] + conds, tok(n))

# Commands with such operands do nothing. A GOTO to a room that does not
# exist is one of them (the player stays in the laboratory, room 1). A
# counter command may write any value into counter 32, the room, as long
# as it is a room again before anything looks at it (BADROOM below).
test("BADARGS", msg("BADARGS expect 7 7 1 100 got"), setc(1, 7), setc(2, 200), printc(1),
     [52] + val(BAD_ITEM), [53] + val(BAD_ITEM), [59] + val(BAD_ITEM),
     [74] + val(BAD_ITEM), [62] + val(BAD_ITEM) + [LAB],
     [72] + val(BAD_ITEM) + [LAMP], [72, LAMP] + val(BAD_ITEM),
     [75] + val(BAD_ITEM) + [LAMP], [75, LAMP] + val(BAD_ITEM),
     [80] + val(BAD_ITEM) + [1], [80, LAMP, BAD_COUNTER],
     [85] + val(BAD_ITEM), [122] + val(BAD_ITEM),
     [58, BAD_BIT], [60, BAD_BIT], [125, BAD_BIT, 1], [125, 1, BAD_BIT],
     [64, 5, BAD_COUNTER], [65, 1, BAD_COUNTER], [81, BAD_COUNTER],
     [91, 1, BAD_COUNTER, 1], [91, 1, 1, BAD_COUNTER],
     [94, BAD_COUNTER, 1, 1], [94, 1, 2, 1],        # counter 2 is not a room
     [95, 2, 1, 1],
     [97, BAD_COUNTER, 1], [97, 1, BAD_COUNTER], [98, 1, BAD_COUNTER],
     [99, 5, BAD_COUNTER], [100, 5, BAD_COUNTER], [101, 5, BAD_COUNTER],
     [102, BAD_COUNTER], [104, BAD_COUNTER], [109, BAD_COUNTER],
     [123, 1, BAD_COUNTER], [126, BAD_COUNTER, 1], [126, 1, 2],
     [127, BAD_COUNTER, 1], [127, 1, 2],
     printc(1),
     [54, BAD_ROOM], printc(ROOM_COUNTER),
     setc(ROOM_COUNTER, BAD_ROOM), printc(ROOM_COUNTER), setc(ROOM_COUNTER, LAB))

# Logic control: 2 XOR, 9 fuzzy and 10 strict noun matching, one that does
# not exist, and a condition that does not exist (true)
test("LOGIC", msg("LOGIC expect 1 2 3 4 5 6 got"), [58, 1], [60, 2], more=True)
cont([(8, 1), XOR, (9, 2)], tok(91))
cont([(8, 1), XOR, (8, 2)], tok(1))
cont([XOR, (8, 1)], tok(2))
cont([XOR, (8, 2)], tok(92))
cont([FUZZY, (28, noun("GEM#RED")), (0, noun("GEM#BLUE"))], tok(3))
cont([FUZZY, STRICT, NOT, (28, noun("GEM#RED")), (0, noun("GEM#BLUE"))], tok(4))
cont([(31, 11), (8, 1)], tok(5))
cont([(6, 0)], tok(6))

# How results are combined, as the MS-DOS interpreters do it. A line fails
# at its first false result unless an OR (or 31.8) came before, so an OR in
# the middle is too late. The first result after a left parenthesis starts
# the group afresh, and the result of the group is combined, and negated,
# like that of a single condition. A group that comes first in an OR line
# is combined with the "true" every line starts from.
test("GROUPS", msg("GROUPS expect 1 2 3 4 5 got"), [58, 1], [60, 2], more=True)
cont([(8, 2), OR, (8, 1)], tok(91))
cont([OR, (8, 2), (8, 1)], tok(1))
cont([(8, 1), LPAREN, OR, (8, 2), (8, 2), AND, RPAREN], tok(92))
cont([(8, 1), LPAREN, OR, (8, 2), (8, 1), AND, RPAREN], tok(2))
cont([NOT_ALL, LPAREN, (8, 2), RPAREN], tok(93))
cont([AT_END, (8, 2), OR, (8, 1)], tok(3))
cont([AT_END, (8, 2)], tok(94))
cont([OR, LPAREN, (8, 2), RPAREN, (8, 2)], tok(4))
cont([(8, 1), LPAREN, (8, 2), RPAREN, OR, (8, 1)], tok(95))
cont([NOT, LPAREN, (8, 1), (8, 1), RPAREN], tok(96))
cont([AT_END, XOR, (8, 1), LPAREN, (8, 1), (8, 1), RPAREN], tok(5))

# The "with" list: HIT ROBOT WITH ROCK KEY. 997 is the noun of the list
# that counter 44 points at, and as an item the one in counter 45. 999 is
# the second noun of the command, the first of the list.
pack(verb("HIT"), 0,
     [msg("WITH expect 2 1 2 key rock got"), printc(46), setc(44, 2), setc(45, KEY)],
     words=[2 << 6, 0], more=True)
cont([(28, WITH), (0, noun("KEY"))], tok(1))
cont([(1, WITH)], tok(2))
cont([], [85] + val(WITH), [85] + val(NOUN2))

# 71 save and 110-1 restore. The scripts give no file name.
test("SAVE", msg("SAVE"), [71], msg("done"))
test("LOAD", msg("LOAD"), [110, 1, 0], msg("done"))

# 61: dead, which is not the end
test("DIE", msg("DIE"), [61], msg("expect room 3 got"), printc(32))
test("HOME", [54, LAB], msg("HOME"))

# Errors that stop the interpreter, each for a script of its own
test("PARENS")
actions[-1][3][:] = [LPAREN] * 6
test("UNPAREN")
actions[-1][3][:] = [RPAREN]
test("OVERFLOW", LOOP)
for _ in range(32):
    cont([], LOOP)
# A room that does not exist, left in counter 32 when the room is described
test("BADROOM", setc(ROOM_COUNTER, BAD_ROOM))

test("END", [63])

# --- Writing it out ----------------------------------------------------------


def condition_numbers(conds):
    out = []
    conds = conds or [(0, 0)]
    for k, (code, arg) in enumerate(conds):
        assert 0 <= code < 32 and 0 <= arg < 1024, (code, arg)
        raw = code | arg << 5 | (0x8000 if k == len(conds) - 1 else 0)
        out += [raw >> 8, raw & 0xFF]
    return out


def dictionary(words):
    """One synonym group per line, each word between commas."""
    return ['",%s,"' % w for w in words]


def database(title=TITLE):
    room_lines = []            # first: the descriptions are messages too
    for exits, text in rooms:
        description = messages.index(msg(text) and text) + 76 if text else 0
        room_lines.append(",".join(map(str, exits + [description, 0])))
    assert len(messages) <= 180 and len(nouns) < 128 and len(verbs) < 128
    header = [len(items) - 1, 0, len(nouns) - 1, len(verbs) - 1, len(rooms) - 1,
              5, LAB, len(messages) - 1, 0, 0, len(preps) - 1, len(adverbs) - 1,
              len(actions) - 1, 0, 0, 0, 0, 0]
    lines = ['"%s",' % title + ",".join(map(str, header))]
    for v, n, words, conds, commands in actions:
        numbers = [len(words) << 4 | len(commands) - 1, v, n] + words
        numbers += condition_numbers(conds) + commands
        assert all(0 <= x < 256 for x in numbers), numbers
        lines += map(str, numbers)
    lines += dictionary(verbs) + dictionary(nouns)
    lines += room_lines
    lines += ['"%s"' % m for m in messages]
    lines += ['"%s",%d,%d,%d' % item for item in items]
    lines += dictionary(adverbs) + dictionary(preps)
    lines += ['""',             # no substitutions
              "0,0,0",          # no object pictures
              "0",              # one unknown value
              '"Not a game: written by test/synthetic/mkdatabase.py"']
    return "\n".join(lines) + "\n"


def add_loader_edges():
    """Turn the database into EDGES.DAT: the same, with two things added
    at the end, so that what comes before keeps its numbers."""
    # A verb group without words: the groups after it keep their numbers
    verbs.append("")
    pack(verb("LATER"), 0, [msg("LATER expect this line got it")])
    # A message longer than the loader keeps: the rest is dropped, and the
    # message after it is still read from the right place
    long = "".join(str(n % 10) for n in range(MAX_STRING_LENGTH + 100))
    test("LONG", msg(long), msg(f"LONG expect {MAX_STRING_LENGTH} digits above got this"))


def manifest(rows):
    """The rows the script runner needs to find the databases."""
    lines = ["# The databases of the synthetic tests, written by mkdatabase.py.\n"
             "# Columns as in ../images/games.manifest.tsv.\n"
             "#\n"
             "# id\tdir\tfile\tsha256\tkeys\tnote\n"]
    for path, text, note in rows:
        digest = hashlib.sha256(text.encode("ascii")).hexdigest()
        folder = os.path.basename(os.path.dirname(path))
        ident = "synthetic" if path == OUT else folder
        lines.append(f"{ident}\t{folder}\t{os.path.basename(path)}\t{digest}\t\t{note}\n")
    return "".join(lines)


def main():
    text = database()
    prefix = database(title=TITLE.rstrip()[:-1])
    add_loader_edges()
    edges = database()
    rows = [(OUT, text, "made-up database, loads as X-Men"),
            (EDGES, edges, "the same with an empty verb slot and a long message"),
            (PREFIX, prefix, "the same titled with the start of X-Men's title: not loaded")]
    files = [(path, want) for path, want, _ in rows] + [(MANIFEST, manifest(rows))]
    if sys.argv[1:] == ["--check"]:
        for path, want in files:
            try:
                with open(path, encoding="ascii") as f:
                    have = f.read()
            except FileNotFoundError:
                have = None
            if have != want:
                sys.exit(f"{path} is out of date: run mkdatabase.py, "
                         "then synthcheck with --bless")
        return
    for path, want in files:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="ascii") as f:
            f.write(want)
    print(f"wrote {OUT}: {len(actions)} actions, {len(messages)} messages, "
          f"and its variants {EDGES} and {PREFIX}")


if __name__ == "__main__":
    main()
