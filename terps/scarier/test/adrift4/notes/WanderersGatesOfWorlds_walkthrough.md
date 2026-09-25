# Странники: врата миров 0.04 — walkthrough (**end of demo, 29/29, no win**)

- **Engine:** ADRIFT 3.90, cp1251 Russian. "Странники: врата миров" v0.04
  (WIP) by Сергей "Namata" Атаманов. File: `WanderersGoW 0.04.taf`.
  - 2nd person. The player is called Странник.
  - You come round on a hilltop, heal yourself with a magic drop, get down
    to the river, climb onto the raft, and lasso a tree branch to reach the
    house.
- **Result:** the full scored game, 29/29, reaching the author's
  end-of-demo room "Дом". Its room text reads "Пока все. Конец.
  Максимальный возможный счет на текущий момент: 29+20+5=54".
  - The game has no type-6 win, and WINTEXT is a placeholder, so the marker
    is empty. Wired as
    `wanderersgow_solution.txt|WanderersGoW 0.04.taf||`, with no env.
  - If a marker is wanted anyway, the room line `Пока все. Конец` is a
    single line, but it is not a real ending.
- **Determinism:** 3 row.sh runs gave the same md5,
  `fd6ba5cef83bab9591bd4313272c5698` (208 lines).
- **Turns:** 23, per `помощь`'s "Прошло ходов".

## Score

- **Main score, 29/29:**
  - капля: +6
  - встать: +2
  - raft: +6
  - loop: +5
  - tree: +10
- **Hidden scores, also collected:** these are variables that the author's
  own `помощь` screen reports.
  - Альт. счет, "скрытые приколы": 5, for `съесть траву`.
  - Экстра счет: 20, for `обернуть лист лохмотьями`.
- **`помощь` output:** "Общий счет: 54 из 999". 54 is the author's stated
  demo maximum.

## Route by phase

1. **Hilltop:**
   - A blank line absorbs the intro waitkey.
   - `x лист`, `x каплю`, `съесть траву`, `взять каплю`,
     `обернуть лист лохмотьями`.
   - `встать` is followed by a blank line for its "[дальше...]" waitkey.
2. **Descent:**
   - W, then `вниз`. From the summit, this hits task 13's `*вниз*` ALTCMD,
     a jump to room 5.
   - The jump has two waitkeys, so it needs two blank lines.
3. **River:** S, `прикоснуться к плоту`, `смотреть`, `взять веревку`,
   `сделать петлю на веревке`, E.
4. **Tree:**
   - `набросить петлю на сук` fails in daylight: "Ты промахиваешься в
     темноте.".
   - Wait ×4 for night. Night is event 0 → task 11 `#lighton`, 15 turns
     after `встать`.
   - Throw again, then S, S to Дом.
   - The recovery event turns the капля into a stone.
5. **Check:** `помощь`, `score`.

## Engine oddities seen

- **The synonym table shadows many Russian verbs:** взять, встать,
  помощь, есть→eat, …
  - Every scoring step survives because the author also supplied ALTCMDs,
    including English ones like `stand` and `take капл*`.
  - The rewrite applies to the typed line only, not to task
    commands/ALTCMDs. This is run390-faithful (45F20F). Under the Runner's
    synonym gate, ported on 2026-09-25 (`Dolg_walkthrough.md`), no line of
    this solution changes, and the Wine run390 drive
    (`runner_transcripts/wanderersgow.txt`) is identical on every turn.
- **The throw is gated on night even though its failure text talks about
  darkness.** The design sounds inverted, but it is how the tasks are
  written, not an engine bug.
- **Author's own listed bugs:** "можно выкинуть каплю, глюки с
  картинками". The route doesn't hit them.

## The walkthrough (UTF-8 rendering of the cp1251 golden)

```
(blank)
x лист
x каплю
съесть траву
взять каплю
обернуть лист лохмотьями
встать
(blank)
запад
вниз
(blank)
(blank)
юг
прикоснуться к плоту
смотреть
взять веревку
сделать петлю на веревке
восток
набросить петлю на сук
ждать
ждать
ждать
ждать
набросить петлю на сук
юг
юг
помощь
score
```
