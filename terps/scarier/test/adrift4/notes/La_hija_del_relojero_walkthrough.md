# La hija del relojero — walkthrough

- **Engine:** ADRIFT 4.00 (`relojero.taf`, 21,775 bytes) by **"Nano"**, in
  **Spanish**. **One room, 8 tasks, 12 objects, no NPCs, 2 events.**
- **Result:** ★ **WON** — T5 `arreglar *fenix`, `ACT type=6 v1=0`, then
  WINTEXT. There is **no score**: not one `ACT type=4` in the file, so every
  win is equally maximal. There is no losing ending either — T5's is the
  file's only `ACT type=6`.
- **Solution:** `goldens/relojero_solution.txt` (golden blessed, row in
  `run_v4_walkthroughs.sh` with no env). Win marker:
  `Cierro los ojos y lloro.`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`,
  `SCR_TRACE_MATCH` and play.

The second Spanish game in the v4 corpus, after *Renuntio*, and the third
one-room game after *Salutations* and *I*. It is a vignette rather than a
puzzle box: a clockmaker sits at his daughter Maria's bedside while five roses
grow out of her back and feed on her blood. He cannot save her. What he can do
is wind up the brass Phoenix he built for her years ago and let it sing her to
sleep, which is what the win is.

## Route

```
x hija                   obj 3, via SYNONYM [Hija] -> [Maria]
x rosas                  obj 4 — the five roses, "rojas como la sangre"
hablar hija              T0 — "Shhh. No te esfuerces"
x mesita                 obj 5 — names the drawer
x cajon                  "The cajon is closed."
abrir cajon              library open of obj 7; reveals the Phoenix
coger fenix              obj 6
x fenix                  the game's own hint about the winding cord
tirar cuerda             T3 — the cord snaps off into your hand (obj 8)
x trozo                  obj 8
arreglar fenix           T5 — EndGame win
```

Eleven commands, no movement — the game never leaves *Dormitorio*.

## The chain

Only three of the eleven are load-bearing, and the game hands you all three.

**`abrir cajon`** is the plain library open of the bedside table's drawer
(`OPENABLE obj=7 [cajon] openable=6 key=-1`) and it reveals **obj 6, the
"Fenix de laton"** — the only object in the file that is not static, and the
only one not in scope at the start.

**`tirar cuerda`** is **T3** `Tirar *cuerda*`, gated on holding the Phoenix
(`RESTR type=0 v1=3 v2=1` — object 6, held). The Phoenix's own description is
the pointer: *"De su panza sobresale una pequeña cuerda, si tirase de ella lo
pondria en funcionamiento."* Pulling it does **not** wind the bird — the cord
is decades old and tears, and T3's `ACT type=0 v1=4 v2=4` puts the broken
piece (obj 8, "Trozo de cuerda") into your hand. That is the game's one
reversal, and it is only a beat long.

**`arreglar fenix`** is **T5**, gated on holding both the Phoenix and the
broken cord (two `RESTR type=0 … v2=1`); `arreglar *cuerda`, `unir *cuerda`
and `atar *cuerda` are its alternates. The clockmaker knots the two ends
together, the bird sings in a voice much like his dead wife's, and the roses
turn to ash. Win.

## Three of the eight tasks are dead

All three are dead in the file, not in Scarier: the real Runner reads the same
data and is equally stuck.

**T6 `*vaso*` and T7 `*Tamborilero*` are `Where` Type 0.** That is
`ROOMLIST_NO_ROOMS`, which means runnable in **no** room rather than in every
room — `sctasks.cpp task_can_run_task_directional` returns `FALSE` for it
outright, the rule settled against run400 for *The Hangover* (see
`notes/The_Hangover_walkthrough.md`). The room description offers *"un vaso y
una botella de agua"* on the bedside table and the tin drummer grinning in the
corner, and both objects were clearly meant to answer with these tasks.

The glass is lost twice over: `STATIC obj=10 [Vaso] rooms=` places it in no
room either, so `x vaso` is "You see no such thing" whichever way you come at
it — the only object in the game that is in the prose and nowhere else. The
drummer *is* placed, so `x tamborilero` still prints its object description
(it is a design he made for the king; wound up, it dances and drums a
different tune every time). What is lost there is only the task's text.

**T4 `Abrir ventana` is killed by the game's own synonym table.** The file
defines `SYNONYM [abrir] -> [Open]`, and synonym substitution runs **before**
task matching. So a typed `abrir ventana` reaches the matcher as
`Open ventana`, the pattern `Abrir ventana` cannot match it, "ventana" is not
an object, and the library answers "Open what?". The task's only restriction
(`type=3 v1=0 v2=0 v3=0`) passes; there is simply no form of the command that
can reach it. Opening the shutters onto the daylight the clockmaker closed out
in the first paragraph is dead content.

The evidence is `SCR_TRACE_MATCH=1`, which echoes the *post*-substitution
input. It prints `MATCH task=0 pattern=[hablar maria] input=[hablar Maria]`
for a typed `hablar hija` — the same substitution machinery working as the
author intended, through `SYNONYM [Hija] -> [Maria]`, one task away from the
one it breaks.

## The hablar trio, and the events

**T0, T1 and T2** are three separate tasks whose commands are `Hablar hija`,
`Hablar niña` and `hablar maria` — and all three carry the *same* two
alternates, so all three patterns sit on all three tasks and **T0 claims every
one of them**. T1 and T2 are unreachable as well, but harmlessly: they are
duplicates of the task that wins the match.

**EVENT 0** is the recurring moan of pain — *"Un gemido de dolor se escapa de
la boca de mi hija"* — at `start=4..4 time1=time2=4 restart=1`, so it lands
every fourth turn and paces the route above. **EVENT 1** (`start=6..6
time1=time2=1`) has no texts, no affected task and no object moves: it starts
on turn 6 and then finishes and restarts itself forever, printing nothing. A
no-op, confirmed with `SCR_TRACE_EVENTS=1`.

## Notes for the next reader

- **No score system.** `score` prints *"Your puntos is 0 fuera of a maximum of
  0. (0%)"* — the author translated the ADRIFT score nouns but left the
  sentence frame in English, which is why the marker is WINTEXT prose.
- **No `<waitkey>` anywhere** (checked with `SCR_MARK_WAITKEY=1`), so the row
  needs neither `SCR_SKIP_WAITKEY=1` nor a filler line.
- **Keep the solution ASCII.** `x munecas` does not match obj 9 `[Muñecas]`,
  whose names all carry the tilde, so the shelves of porcelain dolls are left
  out of the tour rather than put a high byte in a solution file. The golden
  itself is ISO-8859-1, like the game.
- The library's own replies stay in English (`You take the Fenix de laton de
  el cajon`, `The cajon is closed`), so the transcript is bilingual. That is
  faithful — the author localised the game's text and its synonym table, not
  the engine's library messages, and the real Runner behaves the same way.
