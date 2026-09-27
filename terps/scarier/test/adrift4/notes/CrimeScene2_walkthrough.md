# Место преступления 2 — walkthrough (**best reachable, 23/70, no win**)

- **Engine:** ADRIFT 3.90, cp1251 Russian. "Место преступления 2" v1.0
  (updated 10.11.03), siriusif.narod.ru. File: `CS2.taf`.
  - A police procedural. Detective Коплан and partner Роже search a murder
    house (rooms 0–9) for five pieces of evidence: стакан, гильза, нож,
    тюбик, документы. Роже's "как успехи?" (T53) then ends the search. You
    hand the evidence to Марош in the lab (T70), and `написать отчет` in
    your office (T83, ACT type=6) wins.
- **Result:** every point Scarier can award, 23/70, in 63 prompts. There is
  no win.
  - Wired as `cs2_solution.txt|CS2.taf||`, with no env and no marker.
- **Determinism:** 3 row.sh runs gave the same md5,
  `ff734bb11555a8e7f3a7bac1d576effb` (290 lines).

## Why 47 points are out of reach: the SYNONYM table shadows the task verbs

- **The rewrite:** the game's SYNONYM table rewrites the typed line
  *before* task matching. It turns взять→get, открыть→open,
  разбить→smash, север/юг/запад/восток→north/… Since 2026-09-25 Scarier
  applies the Runner's own gate (an Original passes only when its first
  hit at a word start is followed by a space, comma, full stop or the end
  of the line; then every occurrence is replaced as a substring; see
  `Dolg_walkthrough.md`). None of these verbs has a trailing space in its
  Original, so the gate passes for every blocked line below and nothing
  changes for them.
- **No workaround:** CS2 has **no ALTCMDs at all**, unlike NAT_01 (169) or
  WanderersGoW. So every task whose command starts with one of those words
  can never match.
- **Measured in Wine run390 (2026-09-25):** the whole solution, driven as
  `runner_transcripts/cs2.txt`, is identical word for word. The compare
  tool reports "10+ differing turn(s)" only because the game's question
  menus print lines that begin with `> `, which it takes for prompts; a
  word-level diff shows no difference. Two synonym lines changed under the
  gate: `выглянуть в окно` is now left alone (the first `в` hit is inside
  `выглянуть`) and still scores its 5 points, and the compass `св` was
  changed to `ne` in the solution because the Original `[СВ]` has capitals:
  it passes the lower-cased gate and then replaces nothing in the
  lower-cased line, and the Runner answers a typed `св` with the game's
  own «Да?».
- **Library verbs don't help either.** All the evidence objects start
  hidden (pos −1), and only the tasks move them to the player, so the
  library verbs answer "Взять что?".
- **Capitals don't help either.** `Взять` is rewritten the same way; the
  Runner LCases the typed line first.

| Task | Pts | Command | Blocked by |
|---|---|---|---|
| T11 | 1 | открыть шкаф | открыть→open |
| T13 | 3 | взять трость | взять→get |
| T15 | 5 | разбить зеркало трость* | разбить→smash |
| T19 | 5 | взять стакан | взять→get |
| T28 | 3 | взять гильз* | взять→get |
| T29 | 3 | взять но* | взять→get |
| T58 | 3 | открыть сейф ключ* | открыть→open |
| T60 | 4 | взять документ* | взять→get (and T58) |
| T53 | 10 | как успехи? | needs стакан/гильза/нож/документы held ("Вы собрали не все доказательства.") |
| T70 | 5 | отдать улики | needs the same evidence |
| T83 | 5 | написать отчет (win) | room 25 is reachable only by T71 `юг` (юг→south) |

**Reachable awards (all taken, 23):**

- T5 посмотри наверху: 1
- T22 осмотреть пиво: 2
- T23 осмотреть тайник: 3
- T25 выглянуть в окно: 5
- T27 осмотреть книги: 2
- T38 разломать стул: 2
- T43 вызови паталогоанатома: 1
- T46 осмотреть шкафчик (key, room 6): 4
- T48 осмотреть тюбик: 3

**Is it Runner-faithful?** Yes, as far as run390 is decompiled. The
synonym loop at run390 45F18C–45F206 rewrites MemVar_468118 in place. The
post-synonym line is then snapshotted at 45F20F, and that line is what the
task matcher sees.

**But the author clearly expected the raw Russian line to reach the tasks:**

- T0 `север` in room 0 carries a protest from Роже ("А мне что прикажешь
  делать?") that never shows. The library simply walks you north.
- The whole second act runs on `юг`, which is shadowed the same way.

So either the author's Runner build or a Russified Runner matched tasks
before synonyms, or the game was shipped untested. Wine run390 (2026-09-25)
behaves exactly as Scarier does on every line of the solution, so it is
not a Scarier divergence; the 47 points stay out of reach.

## The patched row: 70/70, a win (2026-09-27)

`PATCH_CRIME_SCENE_2` in `sctafpar.cpp` is applied only under
`SCR_ASSUME_PATCHES=1`, which is on by default in the Glk build. It first
checks synonyms 5 юг, 6 север, 25 взять, 54 открыть and 67 разбить. It
then re-spells the 17 shadowed task commands (T0, T10, T11, T13, T15, T19,
T28, T29, T58, T60, T61, T66, T68, T71, T79, T80, T81) with the English
the synonyms leave behind, e.g. `get трость` and `open сейф ключ*`.

- Row: `cs2_patched_solution.txt|CS2.taf|You scored 70 out of the mximum 70!|SCR_ASSUME_PATCHES=1`.
  "mximum" is the ALR `a`→"" leak.
- The route is the faithful one plus `разбить зеркало тростью` (+5) and
  `взять документы` (+4). It is cut after `как успехи?` (+10, needs all
  five pieces of evidence; moves you to room 0).
- The second act:
  1. `юг` to the car (room 10), then `лаборатория` (13).
  2. `север` past the badge check (T68), then up and east to Марош.
  3. `отдать улики` (+5).
  4. Back to 13, then `юг` (T71) to the police station (17).
  5. N, E, up, W, W, N to the office (25).
  6. `открыть стол`, `взять бумагу`, `взять ручку`, `написать отчет`
     (+5, win).
- An author gap: the safe opens without the mirror being broken first.
- The faithful `cs2_solution.txt` row is unchanged at 23/70.

## Other observations

- **You can leave the crime scene at once.** `лаборатория` (T67, where=2,
  rooms 0/10) has no restriction and moves you to room 13. The badge gate
  T68 `север` is shadowed, but room 13's library exit N is ungated. Марош
  then refuses: "Вы ничего не хотите сказать Марошу." The route does not do
  this, because it scores nothing.
- **T72 `полицейский участок` has no move action** (author bug). Only T71
  could reach rooms 17–25.
- **Cosmetic ALR leaks:** "Вы пошли   up.", "Вы пошли   на северна восток.",
  and "Вы не можете   tht here!" (the ALR `a`→"" empties every "a").
- **Measured refusals are kept in the script.** `открыть шкаф`,
  `взять трость`, `разбить зеркало`, `взять стакан`, `взять гильзу`,
  `взять нож` and `открыть сейф ключом` each document a blocked award.

## Route by phase

1. **Entrance:** talk to Роже, ask all four questions. `посмотри наверху`
   (+1) sends him upstairs.
2. **Corridor:**
   - Talk to the patrolman.
   - The mirror, wardrobe and safe door (the blocked tries).
3. **Dining room (W):**
   - The table and glasses.
   - буфет, пиво (+2), тайник (+3).
   - `выглянуть в окно` (+5).
4. **Back door (NE):**
   - Talk to the patrol chief; `вызови паталогоанатома` (+1).
   - `осмотреть шкафчик` (+4, ключик).
5. **Guest bedroom (E):** `осмотреть стул`, `разломать стул` (+2,
   diamonds).
6. **Living room (S):** труп, книги (+2), then the blocked
   гильза/нож takes.
7. **Upstairs:**
   - The toilet.
   - The bathroom: шкафчик, then `осмотреть тюбик` (+3).
   - The bedroom: `как успехи?` → "Вы собрали не все доказательства.",
     then `score` = 23/70.

## The walkthrough (UTF-8 rendering of the cp1251 golden)

```
говорить Роже
кто убитый?
кто прибыл?
допросишь соседей?
посмотри наверху
север
говорить патрульный
когда приехали?
где напарник?
осмотреть зеркало
осмотреть шкаф
открыть шкаф
взять трость
разбить зеркало
осмотреть дверцу
запад
осмотреть стол
осмотреть стаканы
взять стакан
осмотреть стулья
осмотреть буфет
осмотреть пиво
осмотреть тайник
осмотреть окно
выглянуть в окно
ne
говорить патрульный
как зовут?
что трогал?
подозрения?
вызови паталогоанатома
осмотреть шкафчик
восток
осмотреть кровать
осмотреть тумбочку
осмотреть стол
осмотреть стулья
осмотреть стул
разломать стул
юг
осмотреть труп
осмотреть книги
взять гильзу
взять нож
осмотреть стекло
запад
открыть сейф ключом
вв
запад
осмотреть унитаз
восток
восток
осмотреть шкафчик
осмотреть тюбик
осмотреть зеркало
осмотреть ванну
запад
север
говорить Роже
как успехи?
осмотреть кровать
инв
score
```
