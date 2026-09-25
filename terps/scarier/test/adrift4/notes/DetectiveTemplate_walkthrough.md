# Шаблон детектива — walkthrough (**sandbox, 0/0, no win**)

- **Engine:** ADRIFT 3.90, cp1251 Russian. "Шаблон детектива" v1.2 by
  Larisalisa, 2003. File: `shablon.taf`.
  - This is a *template*: a detective's office, the street with an urn and a
    taxi, and a bar that sells coffee, beer and cigarettes.
  - There is no plot, no scoring (max 0), and no ending; WINTEXT is a
    placeholder.
- **Result:** a coverage walk of every working interaction, 57 prompts.
  - Wired as `shablon_solution.txt|shablon.taf||`, with no env and no
    marker.
- **Determinism:** 3 row.sh runs gave the same md5,
  `c67312da16f3fd53a6f3da84abf60d2d` (269 lines).

## Route by phase

1. **Office:**
   - Window (examine/open).
   - The desk drawer: take the ID, notebook and pen.
   - Phone, wallet, watch, `инв`.
   - Sit on the chair, then stand.
2. **Street:** examine the urn, get into the taxi, get out.
3. **Bar:**
   - Ask J.J. about the goods, read the menu.
   - Buy and drink the coffee and the beer.
   - Buy cigarettes, examine them.
   - `курить` 21 times. The first 20 give the scripted quips a1..a20; the
     21st gives "Закончились!".
4. **The urn:** pick up the empty pack, go N, and
   `выбросить пачку в урну`. Since the synonym-gate port (2026-09-25) this
   reaches the game's own task 60 («Я выбросил пустую пачку из-под сигарет
   в урну.») with no fine; before it, the line was rewritten to
   `выбросить пачку into урну` and the 2$ littering fine fired instead.
5. **State checks:**
   - The wallet goes 200→178$ over the whole session (drinks and
     cigarettes; no fine).
   - The clock advances +1h every 14 turns.

## Engine oddities seen (from the SYNONYM rewrite; run390-faithful)

The rewrite follows the Runner's gate since 2026-09-25 (`Dolg_walkthrough.md`):
an Original passes only when its first hit at a word start is followed by a
space, comma, full stop or the end of the line, and then every occurrence
is replaced as a substring. The whole solution is identical on every turn
in Wine run390 (`runner_transcripts/shablon.txt`, 2026-09-25).

- **`см`→exam.** Use `осмотреть`.
- **`взять`→get in this game.** Use `брать`.
- **`лечь на диван` can't be matched.** `на`→onto, and every ALTCMD also
  contains " на ".
- **`встать` prints "I am already stи ing!".** The ALR "and"→"и " rewrite
  substitutes inside the word "standing". This is a game-data quirk
  (ALR is substring-based).
- **`писать в книжку` gives "Не понимаю".** The task exists but has no
  text.
- **Task 60, the fine-free way to use the urn, IS reachable** with
  `выбросить пачку в урну`: the `[в] -> [into]` Original's first hit is the
  `в` inside `выбросить`, followed by `ы`, so the gate fails and the line
  is not rewritten. (The old whole-word rewrite turned it into `into` and
  only the fine task matched.)

## The walkthrough (UTF-8 rendering of the cp1251 golden)

```
осмотреть окно
открыть окно
осмотреть окно
осмотреть стол
открыть стол
брать удостоверение
брать книжку
брать ручку
осмотреть телефон
осмотреть бумажник
осмотреть часы
инв
сесть на стул
встать
юг
осмотреть урну
сесть в такси
выйти
юг
спросить джи джея про товары
осмотреть меню
купить кофе
осмотреть стойку
пить кофе
купить пиво
выпить пиво
купить сигареты
брать сигареты
осмотреть сигареты
курить ×21
брать пачку
осмотреть пустую пачку
север
выбросить пачку в урну
осмотреть бумажник
осмотреть часы
север
```
