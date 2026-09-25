# Натаниель Пек. Дело 1. Пропавшее ожерелье — walkthrough (**WIN, 18/18**)

- **Engine:** ADRIFT 3.90, cp1251 Russian, by LARISALISA
  (advantureclub.narod.ru; the intro is dated 26 апр 2007). File:
  `NAT_01.taf`.
  - A detective case: a necklace has been stolen from Mr Twist's house.
  - You collect clues, write each one in your notebook (`записать`), accuse
    the gardener and shake hands with Twist. Then, before time runs out, you
    feel the ring (`пощупать перстень`).
- **Result:** a full win at 18/18 in 46 prompts.
  - Wired as `nat01_solution.txt|NAT_01.taf|Вы прирожденный детектив|`,
    with no env.
  - Marker in cp1251: `c2fb20eff0e8f0eee6e4e5ededfbe920e4e5f2e5eaf2e8e2`.
    No usable ASCII marker exists; the only ASCII candidate is "100%".
  - The ending also prints "Вы набрали 18 из максимального количества в
    18!".
- **Determinism:** 3 row.sh runs gave the same md5,
  `d989591c4edc13a127cd3ee101f881c4` (320 lines).

## Mechanics

- **Scoring:** 18 separate +1 awards (ACT type=4): the clues, the notes and
  the resolution steps. The route takes all of them.
- **The clock** starts at 8 o'clock and advances 1 hour every 14 turns. At
  21 o'clock the game is lost, so there is ample room for a 46-prompt route.
- **Event 1:** coffee appears in the kitchen 2 turns after the summary note.
- **Event 3:** after the handshake you have 2 turns to `пощупать перстень`.
- **The win:** task 57 fires 1 turn after task 56 (type=6). That is why the
  route ends with `ждать` and then a blank line for the ending key prompt.

## Route by phase

1. **Office:**
   - A blank line absorbs the intro waitkey.
   - `об игре`
   - Take the notebook, pen and paper, and read the paper.
2. **Taxi to Лейч-стрит:** S, `сесть в такси`, `ехать на лейч-стрит`, N.
3. **The house:**
   - Ring the bell and show your ID.
   - Examine the blue slip and the ring, then `записать`.
   - East: the picture. Up: the clay lump, then `записать`.
   - `x столик`, `open шкатулку`, examine and take the diamond shard, then
     `записать`.
4. **Garden:** the garden and the gardener's boots, `записать` at each. One
   more `записать` north.
5. **Hall summary note:** S, S, `записать`. This is task 51, and it must be
   done in a room with no room-specific note task.
6. **Resolution:**
   - Wait twice for the coffee, then E and `выпить чашечку кофе`.
   - W, W.
   - `сказать твисту что вор это ваш садовник`
   - `пожать руку твисту`
   - `пощупать перстень`
   - `ждать`, then a blank line.

## Engine oddities seen

- **The intro waitkey eats the first line,** so line 1 is blank.
- **`открыть шкатулку` gives "Не понимаю, что Вы хотите!".**
  - Tasks 123–128 (open/close window/bar/box) have an object-state
    restriction with Var1=0, meaning the "referenced object".
  - With no `%object%` bound, the restriction fails silently and control
    falls to the library.
  - This matches the Runner. It is ported in `restr_get_fail_message`; the
    run400 480F9E annotation covers it, and run390 behaves the same by
    analogy.
  - Workaround: the English library verb, `open шкатулку`.
- **The synonym table rewrites whole words before task matching** (`см`→l,
  `взять`→get, …). The 169 ALTCMDs survive that rewrite, which is why most
  Russian verbs still work.
- **`осмотреть стол` gives "Я не могу сделать это!".** Use `x столик`,
  since the object's raw name is столик.
- **`вверх` echoes as "Я пошел up."** This is a library-string leak and
  cosmetic only.
- **Not verified:** the Runner's behaviour with Cyrillic input under Wine
  (untested).

## The walkthrough (UTF-8 rendering of the cp1251 golden)

```
(blank)
об игре
взять записную книжку
взять шариковую ручку
взять листок бумаги
читать листок бумаги
инв
юг
сесть в такси
ехать на лейч-стрит
север
звонить в дверь
показать мое удостоверение
осмотреть синий листок
читать листок
осмотреть перстень
записать
восток
осмотреть картину
вверх
осмотреть комочек глины
записать
x столик
open шкатулку
осмотреть осколок бриллианта
взять осколок
записать
вниз
север
осмотреть сад
записать
осмотреть башмаки садовника
записать
север
записать
юг
юг
записать
ждать
ждать
восток
выпить чашечку кофе
запад
запад
сказать твисту что вор это ваш садовник
пожать руку твисту
пощупать перстень
ждать
(blank)
```
