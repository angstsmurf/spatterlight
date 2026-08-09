# Cut the Red Wire! No, the Blue Wire! — walkthrough (**WIN, 1/1**, one command)

- **Author:** David Whyld, released in *InsideADRIFT* #41 (2012). You are a
  bomb disposal expert standing in front of a bomb with a red wire and a blue
  wire, and a crowd who would rather like to see you explode.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 games/Cut_the_Red_Wire.taf | cut -c17-22` → `93453e`).
- **Result:** **WON, 1/1** — the game's stated maximum ("a score of 1 out of a
  maximum possible of 1. Well done.").
- **Source:** `downloaded/CutTheRedWire_clubfloyd.html`, the 3 March 2013
  ClubFloyd session.
- Row: `redwire_solution.txt|Cut_the_Red_Wire.taf|a maximum possible of 1. Well done.|SCR_SKIP_WAITKEY=1`.

## The whole walkthrough

```
undo
```

That is the entire solution file. Cutting the red wire kills you; cutting the
blue wire kills you; waiting kills you; anything else kills you, on a one-turn
fuse. The winning move is to undo the move that brought you here — you back
out of the warehouse, evacuate it, and let the bomb go off with nobody inside.
Your career is over, but you are alive, which is what the point is for.

## Why `undo` reaches the game at all

`undo` is in SCARE's standard-command table (`scrunner.cpp`, `{"undo",
lib_cmd_undo}`), so the natural expectation is that the library swallows it
before any authored task can see it. It does not, because of the dispatch
order in `run_game_turn()` (`scrunner.cpp` ~1616):

```
run_game_commands_in_parser_context(..., FALSE)   <- authored tasks, first pass
run_priority_commands
run_game_commands_in_parser_context(..., TRUE)
run_standard_commands                             <- the library, last
```

Game tasks beat the standard library, so the author's `[undo]` task fires and
`lib_cmd_undo` never runs. Confirmed directly with `SCR_TRACE_MATCH`:

```
MATCH task=0 pattern=[undo] input=[undo]
```

## Why the golden keeps going after the win

The game has **no game-over action at all**. It prints the ending and the
score line, offers "Press RETURN if you feel like giving it another go", and
then puts you back in the warehouse. So the harness's appended `quit`/`y` are
answered by the *game* (`Whatever that was, it won't be done.` followed by the
one-turn fuse expiring, twice) rather than by the library, and the transcript
runs on for another two death-and-resurrection cycles before stdin hits EOF.
That tail is deterministic and is in the golden on purpose; the win marker is
the score line, which appears before it.
