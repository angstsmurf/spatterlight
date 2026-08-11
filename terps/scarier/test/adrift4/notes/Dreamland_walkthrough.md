# Dreamland — walkthrough

- **Engine:** ADRIFT 3.9 (`Dreams.taf`, 6,508 bytes). One room
  (*Forest Clearing*), five objects, no NPCs, four tasks, one event.
  The Sphere holders send you to return the stolen water of dreams to the
  basin of life.
- **Result:** ★ **WON, 50/50** — the game's only `ACT type=4` is the +50 on
  TASK 1, which is also the task carrying the winning `ACT type=6`.
- **Solution:** `goldens/dreamland_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `You have saved the Dreamworld`.
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`.

## Route

```
<blank>            (answers the intro's "Click any button")
i / x canopy / x forest / x pool / x basin
wipe away dust
x basin
fill waterskin with water
pour water from waterskin into basin
```

## How the game works

You start holding the waterskin, in the only room, with no exits. Two tasks
matter:

| Task | Command | Effect |
|---|---|---|
| 0 | `fill waterskin with water` (7 phrasings) | sets the waterskin full |
| 1 | `pour water from waterskin into basin` (5 phrasings) | **+50, EndGame win** |
| 3 | `wipe away dust` | reveals the basin's unreadable third line |

TASK 3 is pure lore: it uncovers a potted history of the Dreamworld's three
planes, cut off mid-sentence ("A small piece of the basin has been broken off
here"). It scores nothing, but the route takes it because it is the only
other thing in the game to do, and re-examining the basin afterwards shows
the revealed text.

## The 35-turn clock

EVENT 0 starts immediately at load, runs `Time1 = Time2 = 35`, and fires
TASK 2 `#end game fail` — `ACT type=6 v1=2`, a losing end. Its three
`StartText`/`LookText` beats are the shadow closing in (birds scattering,
"the shadow is almost upon the forest clearing", then the shadow forms eating
you alive, complete with the author's stray `>br>` tag). Confirmed by
waiting 40 turns; the route uses 10, so the clock never matters in practice.
