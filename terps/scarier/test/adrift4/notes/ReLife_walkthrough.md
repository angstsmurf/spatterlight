# ReLife — walkthrough (**WIN, 400/400**)

- **Engine:** ADRIFT 3.90, cp1251 Russian, "ReLife v1.14" by Row. It is a
  dystopia told in the 2nd person. You wake as an "over-ripe" newborn in the
  Согласие (Concord) tower. You register, get your medical check, work a
  shift, sleep, and feed the starving Administrator. Then you break into the
  computer centre, build a bomb on the generator and flee over the dump to
  meet a free child, Твит.
- **Result:** a full win at 400/400 in 79 prompts: the name line plus 78
  commands.
  - Wired as `relife_solution.txt|Relife.taf|Это естественный человек|`,
    with no env.
  - Marker in cp1251: `ddf2ee20e5f1f2e5f1f2e2e5ededfbe920f7e5ebeee2e5ea`.
    It is a line of the task-44 ending text ("Это естественный человек!").
- **Determinism:** 3 row.sh runs gave the same md5,
  `6ba57661356311e82a832bc5f68c9502` (591 lines).
- **Line 1 answers "Please enter your name:".** The name (`Тест`) is echoed
  back in the ending dialogue. There is no intro waitkey.

## Scoring (all 12 ACT type=4 awards, 400 total)

| Task | Pts | Command | Where / gate |
|---|---|---|---|
| T0 | 5 | включить терминал | room 0 |
| T1 | 15 | нажать любую клавишу | after T0; drops the карточка |
| T2 | 25 | набрать 0100111001 | opens room 0's east wall |
| T15 | 50 | дать карточку проконсулу | room 14; sets назначение |
| T16 | 50 | дать карточку врачу | room 19; sets профессия=2 |
| T25 | 30 | работать | room 20; needs T15+T16; sends you to room 21 |
| T23 | 20 | дать карточку раздатчику | room 18; needs T26 (sleep) |
| T24 | 30 | дать шарик админу | room 18; kills the Administrator |
| T10 | 25 | выбить дверь | room 7; needs T8 `открыть дверь`, which gives the ручка |
| T30 | 50 | набрать 11010001110 | room 9, the admin code |
| T42 | 75 | установить таймер на 4 минуты | room 24; needs T37–T41 in order |
| T44 | 25 | говорить | room 31; ACT type=6 win |

## Route by phase

1. **Birth (room 0):**
   - `включить терминал`, `нажать любую клавишу`
   - `взять карточка`, `взять часы` (the watch becomes the timer)
   - `набрать 0100111001`, then east.
2. **Registration:**
   - S, S, E to room 6, then S: `дать карточку проконсулу`.
   - N, N to room 19: `дать карточку врачу`.
   - `x регулятор`, `взять детонатор`, `взять проволока` (see oddities).
3. **Work shift:**
   - S, W, N×4 to room 5, then N to the lift.
   - `тянуть рычаг вниз`, then S, S to room 20.
   - `работать` puts you in room 21. `спать` wakes you back in room 0.
4. **Canteen:**
   - E, N, N, E to room 18.
   - `дать карточку раздатчику`. The protein ball sticks to the wall, so you
     have to `взять шарик` before `дать шарик админу`.
   - The Administrator is shot in the corridor, and you are moved to room 5.
5. **Computer centre:**
   - S×4, E, E to room 7.
   - `открыть дверь` (the handle comes off), then `выбить дверь` puts you in
     room 9.
   - `набрать 11010001110`
   - `нажать синюю кнопку` calls the repair robot. Then `x ремонтник`,
     `взять провод`.
   - `контроль системы` opens 9 S to the generator room.
   - `нажать зеленую кнопку` opens 13 N to room 25.
6. **Bomb (room 24):**
   - провод to пластид, ручка to провод, часы to переходник, проволока to
     таймер, детонатор to провод. These are tasks 37–41.
   - Then 6× `ждать`, then `установить таймер на 4 минуты`.
7. **Escape:**
   - N×14 goes 9, 7, 8, 10, 11, 12, 13, 25, 26, …, 31.
   - `говорить` wins.
   - A trailing blank line absorbs the ending "[Нажмите любую клавишу]".

## Timing (the two things that kill you on the way out)

- **Event 6, the explosion:** it fires 9–11 turns after T42 and kills you
  anywhere in rooms 0–25 (task 43). Room 24 to room 26 is exactly 9 moves,
  so there is no slack at all.
- **The patrolling Регулятор (NPC 0):** it walks a 20-stop loop through
  rooms 3–14. If it enters your room while you are in rooms 10–12, task 22
  kills you.
- **The waits:** a sweep of 0–40 `ждать`s before arming gave deaths only at
  k=0, 1, 2, 13 and 27 under `SCR_RNG=xoshiro`. The route uses k=6, in the
  middle of the safe 3–12 window.
- **Other deaths, all avoided:**
  - T11: the red button in room 9.
  - T29: swearing (`*fuck*`, `*бля*`, ...), which works anywhere.

## Engine oddities seen

- **Library nouns match only the raw nominative name.**
  - `взять карточку` and `взять проволоку` give "Что взять?". Use
    `взять карточка` / `взять проволока`; see the [Raw-name noun matching]
    note.
  - Task commands ("дать карточку …") are the author's own patterns, so
    they are unaffected.
- **Container contents can't be taken until the container is examined.**
  - Before `x регулятор` (room 19) or `x ремонтник` (room 9),
    `взять детонатор` / `взять провод` answer "Что взять?". Straight after
    the `x`, they work.
  - The room description never lists the contents, so the objects are not
    "seen" yet.
  - I assume the Runner behaves the same, but this is not verified under
    Wine: Cyrillic input to the Runner harness is untested.
- **A "Что взять?" question swallows the next line.** In one exploration
  run, `контроль системы` and `нажать зеленую кнопку` were each answered
  with "Что взять?". This is the question-prefix continuation. The golden
  never triggers it.
- **Cosmetic text from the game's own language data:**
  - "Регулятор приходит  из/с  на север."
  - "Регулятор уходит  to  на север." (an untranslated "to")
  - "Вы взяли … из/с разобранный Регулятор."
- **T31 is a decoy.** Typing the admin's other code, 1001110010, is
  refused. Nothing is lost by not trying it.
- **Optional lore in room 9, worth 0 points:** after T30, `palmfiles`,
  `документация`, `selfdestroy` and `история согласия`. The route skips
  them.

## The walkthrough (UTF-8 rendering of the cp1251 golden)

```
Тест
включить терминал
нажать любую клавишу
взять карточка
взять часы
набрать 0100111001
восток
юг
юг
восток
юг
дать карточку проконсулу
север
север
дать карточку врачу
x регулятор
взять детонатор
взять проволока
юг
запад
север
север
север
север
север
тянуть рычаг вниз
юг
юг
работать
спать
восток
север
север
восток
дать карточку раздатчику
взять шарик
дать шарик админу
юг
юг
юг
юг
восток
восток
открыть дверь
выбить дверь
набрать 11010001110
нажать синюю кнопку
x ремонтник
взять провод
контроль системы
нажать зеленую кнопку
юг
присоединить провод к пластиду
присоединить ручку к проводу
присоединить часы к переходнику
присоединить проволоку к таймеру
присоединить детонатор к проводу
ждать ×6
установить таймер на 4 минуты
север ×14
говорить
(blank)
```
