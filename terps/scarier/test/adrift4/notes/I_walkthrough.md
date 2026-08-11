# I... — walkthrough

- **Engine:** ADRIFT 3.9 (`i.taf`, 5,164 bytes — the smallest game in the
  corpus), Christopher Cole. Billed on its own title screen as "a one-room
  interactive fiction game": one room, one (barrier) object, no NPCs,
  17 tasks, 4 events.
- **Result:** ★ **ending reached.** There is no score — `score` answers
  *"There is no scoring system in this game."* — so the win condition is the
  EndGame action on TASK 9.
- **Solution:** `goldens/i_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `I am dead.`
- **Provenance:** no published walkthrough (Key & Compass, IF Archive, CASA
  all have nothing). Derived from `SCR_DUMP_TASKS`.

## Route

```
x dark / x cold / smell / listen / cry / sit / say hello / i remember
feel pulse
z ×21
```

## How the game works

You are buried, and cannot move. Every task except one is a canned reply to
poking at the dark (`x dark`, `x cold`, `smell`, `sit`/`stand`, `* cry *`,
`* say/ask/talk *`, `* time/date *`, `* remember *`, `how *`, `take *`,
`break *` and its 40-odd verb aliases, `* yes *`). None of them advance
anything, and the eight in the route above are there only so the transcript
shows what the game has.

The whole game is one event chain, and `feel pulse` is its ignition:

| Event | Started by | Timer | Fires |
|---|---|---|---|
| 0 `Memory 1` | TASK 3 `examine/feel/touch * heart* *` \| `* pulse` | 3–5 | TASK 6 `memorynumberone` |
| 1 `Memory 2` | TASK 6 | 5–7 | TASK 7 `memorynumbertwo` |
| 2 `Memory 3` | TASK 7 | 5–7 | TASK 8 `memorynumberthree` |
| 3 `Memory 4` | TASK 8 | 5–7 | TASK 9 `memorynumberfour` |

TASK 9 is the only task in the file with an action: `ACT type=6` (EndGame).
It prints the rose, "I remember now... a road... a crash...", "I am dead."
and the three closing lines, and the session is over.

TASK 3's four `memorynumber*` tasks are unreachable by typing — they are
plain word patterns with no spaces, dispatched only by their events — so
nothing you can enter shortens the wait. The route is therefore *one*
command and then 21 turns of `z`.

## Notes

- **21 is measured, not padded.** The four timers are `Time1..Time2` ranges
  (3–5, 5–7, 5–7, 5–7); under the harness's fixed seed they come out
  4 + 5 + 7 + 5 = 21. Adding the eight flavour commands ahead of `feel pulse`
  does not shift them — the same 4/5/7/5 falls out either way — but a
  different seed would, so the golden is seed-pinned like the rest of the
  suite.
- `feel pulse` is the phrasing the route uses; TASK 3 accepts any of
  `examine`/`x`/`look`/`feel`/`touch` on `heart…` or `pulse`.
- The one object in the game is the static *barrier* that answers `sit` with
  "A barrier prevents me from getting up" — it is the coffin lid, and it is
  never interactive.
