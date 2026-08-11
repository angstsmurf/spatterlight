# Asylum — walkthrough

- **Engine:** ADRIFT 3.90 (`as.taf`, 44,503 bytes), *Morningwood Mental
  Institution*. **14 rooms, 1 NPC, 92 tasks, no events, 4 variables** — and
  none of the variables is a score.
- **Result:** ★ **WON** — T59 `asylum`, `ACT type=6 v1=0`. There is **no
  score** (not one `ACT type=4` in the file), so the win is the whole goal.
- **Solution:** `goldens/asylum_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh` with `SCR_SKIP_WAITKEY=1`). Win marker:
  `A large plaque sat on the wall`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`
  plus play.

## Route

```
Room 4 (yours)        open pillow                 (screwdriver)
East Hallway          s
Room 3                s / take cross              (Jack hands it over)
East Hallway          n
Games Room            w / use screwdriver on plug (Leroy leaves his chair)
Dr. Walsh's Office    n / push chair              (Walsh storms out)
                      push button / 2             (medication ordered)
Games Room            s
South Hallway         s
Medication Room       s / talk to nurse / 4       (sleeping pills)
Games Room            n / n
West Hallway          w / pick lock               (the cross is the key)
Stairway              w
Front Entrance        d / put pills in coffee     (guard falls asleep)
Black Room            d / yes / jacob
Games Room (the real  asylum                      (EndGame win)
one)
```

Twenty-seven commands. Four `<waitkey>` pauses, all skipped by
`SCR_SKIP_WAITKEY=1`.

## One strictly linear chain

There are no events at all, so nothing is timed and nothing can be missed;
the whole game is one sequence of `RESTR type=2` gates, each opened by the
previous step (Var2 = 0 throughout, i.e. *the named task must already be
done*).

| Step | Task | Opens |
|---|---|---|
| `open pillow` | T0 | the screwdriver, the only hidden object in the game |
| `use screwdriver on plug` | T12 (screwdriver held) | Leroy leaves his chair to hammer the dead TV, which unblocks **T22 `n`** — the only way into the office |
| `push chair` | T31 | Dr. Walsh falls, complains, leaves; **T29** cabinets, **T30** intercom and **T46** desk all require it |
| `push button` → `2` | T30, T37 | tells Nurse Lois to have medication waiting; **T44** requires T37 |
| `talk to nurse` → `4` | T40, T44 | the bottle of sleeping pills |
| `take cross` → `pick lock` | T6, T50 (cross held) | the padlocked West Hallway door |
| `put pills in coffee` | T55 | the guard sleeps, which is what **T52 `d`** requires |

`take cross` needs no conversation — T6's own ALTCMD is `ask jack about
cross`, and Jack hands it over the first time you ask.

## The two things worth remembering

**Conversation menus are room-scoped bare numbers whose meaning flips on task
state.** Every `1`/`2`/`3`/`4` is its own task, room-locked, and some of them
are paired on opposite polarities of the same gate: in Dr. Walsh's office `1`
is T25 (Walsh's own answer, `RESTR` on `push button` **undone**) and becomes
T36 (Nurse Lois over the intercom, the same gate **done**) the moment you use
it; likewise `3` is T27 then T38. `2` is T37 either way. Nothing ever
re-prompts, so a number typed in the wrong room or the wrong state is simply a
turn thrown away.

**WINTEXT is `<br><br>`** — non-empty, and that is all it takes to suppress
`Congratulations!`, because `task_run_end_game_action()` only falls back to the
default when `scr_strempty(wintext)`. So the whole of the ending is the task's
completion text and the harness marker has to be prose. This is the mirror
image of the *Wheels Must Turn* case in the same batch, where the default
*was* printed and the game's ALR table erased it; between them they cover both
ways a v4 game can end without ever showing an engine message.

## Footgun: the answer is `jacob`, not `mr. tanakian`

At *"Now, answer me this. Who are you?"* the game wants T58, whose printed
command is the author's typo **`mr. tanakiam`**, with ALTCMDs `mr. tanakian`,
`mr tanakian`, `tanakian`, `jacob`, `jacob tanakian`. The two forms containing
a period never match — `mr. tanakian` answers *"I don't understand what you
mean!"* — so the working answers are **`jacob`**, `jacob tanakian`, `tanakian`
or `mr tanakian`. Every card in the asylum reads *"Mr. Tanakian"*, so the
obvious thing to type is the one that fails.

## Two endings, both wins

The voice ends by asking whether you would *"like to stay in horrible reality,
or … go back to the asylum"*:

- **`asylum`** (T59) — the game reprints its own opening paragraph, very
  slightly reworded. You are back in Room 4 with no memory, and the loop the
  title is about closes. This is the route's ending.
- **`reality`** (T60) — *"You spend the rest of your life sitting in the Games
  Room, barely alive and only able to eat, drink and sleep."*

Both are `ACT type=6 v1=0`, and with no score in the file neither is
mechanically better than the other. Saying `no` at the Black Room's first
question instead of `yes` is T56, the only `v1=1` in the game.

## Notes

- **The twist** is that you are not Jacob Tanakian: you are Taylor "Jesus"
  James, the catatonic man in the black suit, and the whole asylum is a world
  you assembled inside your head out of things in the real one. The optional
  material is the evidence for it — `x picture` (T47) in the office unlocks a
  fifth question about the photo, whose two subjects are *Jack Trall* and
  *Leroy Hogan*; `open cabinets` plus `read 1`…`read 4` gives the four patient
  files, including the one on Taylor James. None of it is required to finish,
  which is a pity, because without it the ending has nothing to land on.
- `x dr. walsh` and `talk to dr. walsh` both fail — Walsh is not an NPC, only
  room text and a set of numbered tasks. The single real NPC in the file is
  **Jesus James**, who never moves or speaks.
- `wear cross` (T9) exists and refuses: *"It doesn't fit around your head."*
- The game has **64 identical container entries** for one object (`door`),
  which is a Generator artefact, not content.
