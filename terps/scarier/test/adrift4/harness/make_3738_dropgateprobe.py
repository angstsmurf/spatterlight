"""What a pre-3.9 `drop` line has to be carrying before a task may claim it.

WINE-TRANSCRIPTS-TODO's "bare `Drop what?` arm at run380 @438FE6" row: an
`Adrift_pretry37c`/`38c` feed showed a literal, unrestricted, all-rooms task
`drop a coin` running only while the coin was HELD, and `drop a pebble`
answering "You don't have a pebble!" for a pebble lying in the same room.
run380 `drops()` 438659..438FEA (run370 430475..430DD3 is the same shape)
says the gate is not the task matcher:

    438659  the line is claimed whenever it says drop/put down/leave, or
            put AND down with 44F0E7 clear -- run390 445575 still pushes
            "leave", and so does run400's drops() at 46F15E-46F197, but at
            4.0 nothing routes a `leave` line to drops() and every one of
            them falls to the catch-all instead (lg40.txt).
    4386F5  checktask(line) sets var_A6 = 1 only when NO task matched, and
            438AD5 `If (var_A6 > 0)` gates the whole library print loop --
            a task-matched line therefore prints neither "You drop the X."
            nor "You don't have X!".
    438E68  the task-dispatch loop walks the objects; 438E8E lets only byte
            22 = 0 (held) or &H9C (worn) through, 438EF4 co(idx) asks
            whether the LINE NAMES that object, and only then 438F4F calls
            tasks(1) on the raw line.
    438FE0  buffer still empty -> 438FE6 writes "Drop what?".

The trap the first reading fell into: generaltasks does NOT branch on the
message buffer, it branches on drops()' handled flag, and on the one-object
arm that flag is only ever set at 438E94 (run370 430C55), inside the
held-or-worn test, and in the print loop's act-on-an-object arm.  So an
EMPTY-HANDED player is handed straight on to the ordinary tasks(0) pass and
the "Drop what?" the handler just wrote is simply overwritten by whatever
answers next.  Two worlds are needed:

  DROPGATE -- a loose coin and pebble, a held bean and a wearable cloak.
    drop a coin     task, coin LOOSE, bean+cloak held  -> "Drop what?"
    drop coin bean  the HELD bean is named, the task's coin is not -> runs
    drop zzz        task, nothing it names exists      -> "Drop what?"
    drop a cloak    task, the cloak WORN (the &H9C arm) after `wear cloak`
    drop a coin     task, the coin held again          -> runs
    leave zzz / leave bean / leave a coin -- `leave` claims the same way

  EMPTYHAND -- the same loose coin and pebble, nothing held at all.
    drop a coin     task, nothing held                 -> the task RUNS
    drop a pebble   no task; the print loop speaks     -> "don't have"
    drop zzz        task, nothing held                 -> the task RUNS
    drop qqq        no task, names nothing             -> "Drop what?"
    leave qqq / put down qqq                           -> "Drop what?"
    take a coin then drop a coin                       -> the task runs

Alice's Restaurant (arlo.taf, 3.70) is the corpus case the EMPTYHAND world
was built for: `leave station` at the police station, carrying nothing, runs
its task on run370.

3.90 is built too, as the contrast: its drops() 445F20 carries the same
held/worn walk byte-for-byte (P32Dasm 00045D79) and its own "Drop what?"
(445F0B) still answers `drop qqq`, but 44562A runs a matching task at the
checktask gate, above the walk, so a task fires with nothing in hand.

4.00 is built as the other contrast, for the `leave` feeds: it answers every
`leave` line "I don't understand." / "I don't understand what you want me to
do with the bean.", which is the catch-all and not drops().  The DROPGATE
world doubles as the `leave`-is-`drop` probe -- cmdfile_leavegate.txt
(leave / leave qqq / leave coin / leave bean / leave north) and
cmdfile_leaveall.txt (leave all / leave X and Y / leave all except X /
leave everything empty-handed), transcripts lg37.rtf lg38.rtf lg39.txt
lg40.txt la37.rtf, 2026-09-20.  3.7/3.8/3.9 answer them exactly as `drop`
does, bar the 3.9 article in "You don't have the coin!", and `leave north`
is "Drop what?" with no movement, because generaltasks calls drops() well
above moves() (run370 43B958 vs 43BAEB).

Usage:
    python3 make_3738_dropgateprobe.py [370|380|390|400|all]

Session (from ~/adrift-battle/runner/wine):
    ./fast.sh p37DROPGATE.taf  cmdfile_dropgate.txt  run370.exe
    ./fast.sh p38DROPGATE.taf  cmdfile_dropgate.txt  run380.exe
    ./fast.sh p37EMPTYHAND.taf cmdfile_emptyhand.txt run370.exe
    ./fast.sh p38EMPTYHAND.taf cmdfile_emptyhand.txt run380.exe
run390 needs the transcript name to carry its .txt extension
(TRANSCRIPT=eh39d.txt), or the Select Transcript File dialog writes nothing.
"""
import sys

import make_surfprobe as surf

LIT = surf.LIT

DROPGATE_OBJECTS = [
    ("coin",   "a", "A gold coin.",   ("room", LIT), "", 0, 0, 0),
    ("pebble", "a", "A small pebble.", ("room", LIT), "", 0, 0, 0),
    ("bean",   "a", "A dry bean.",    ("held",), "", 0, 0, 0),
    ("cloak",  "a", "A grey cloak.",  ("held",), "", 0, 0, 0),
]
DROPGATE_TASKS = [
    ("probe",          "PROBE OK."),
    ("drop a coin",    "DROPCOIN."),
    ("drop coin bean", "DROPCB."),
    ("drop zzz",       "DROPZZZ."),
    ("drop a cloak",   "DROPCLOAK."),
]

# The same room with the player's hands empty, so drops() never claims.
EMPTYHAND_OBJECTS = DROPGATE_OBJECTS[:2]
EMPTYHAND_TASKS = [
    ("probe",       "PROBE OK."),
    ("drop a coin", "DROPCOIN."),
    ("drop zzz",    "DROPZZZ."),
]

WORLDS = [
    ("DROPGATE",  DROPGATE_OBJECTS,  DROPGATE_TASKS,  {"cloak"}),
    ("EMPTYHAND", EMPTYHAND_OBJECTS, EMPTYHAND_TASKS, set()),
]

if __name__ == "__main__":
    arg = sys.argv[1] if len(sys.argv) > 1 else "all"
    for v in ([370, 380, 390, 400] if arg == "all" else [int(arg)]):
        for tag, objects, tasks, wearable in WORLDS:
            surf.OBJECTS = objects
            surf.NAMES = [o[0] for o in objects]
            surf.WEARABLE = wearable
            surf.TASKS = tasks
            surf.OUT = {370: "p37%s.taf" % tag, 380: "p38%s.taf" % tag,
                        390: "p39%s.taf" % tag, 400: "p4%s.taf" % tag}
            surf.emit(v)
