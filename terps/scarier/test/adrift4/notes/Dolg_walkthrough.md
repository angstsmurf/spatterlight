# История одного похищения (Dolg.taf) — walkthrough (**WIN**, 10/35, 28%)

- **Game:** "История одного похищения" ("The Story of a Kidnapping"), author e-mail _svetlana@mtu-net.ru. ADRIFT 3.90, 1068 tasks, Russian text in cp1251, maximum score 35.
  - Start: the park (room 9), sitting on a bench, 25 May 1901, 6:00. You have 1000 coins and owe Barni 10000.
  - There are no intro waitkeys and no name prompt.
- **Result:** genuine win (ACT 6 of task 298, `спросить Барни про *`). Final text: «Ну вот, я и расплатился с Барни. ... Ваш счет 10 из максимум 35! Вы прошли 28% игры!» then the end-of-game key prompt.
- **Solution:** `goldens/dolg_solution.txt` (cp1251), 154 lines. Line 48 is blank for the `<waitkey>` after asking Vincent about the house.
- **Wired as** `dolg_solution.txt|Dolg.taf|я и расплатился с Барни|` (2026-09-25). No env beyond what the suite sets.
- **Runner check:** driven in Wine run390x from the same feed (`runner_transcripts/dolg.txt`, `Adrift_282_dolg_rt.txt`): **identical on every turn**, the hint line included.

## Why it was blocked, and what was ported (2026-09-25)

Nearly every door in the game is a task whose command is literally `войти в X` ("go into X"), with no wildcard, so the preposition is part of the command (126/127 `войти в дом`, 120 shop, 122 tavern, 123/124 office, 125 church, 121 garden, 128 pavilion, 225/226 room, 323/324 `войти в правую дверь` into Barni's study). The game also has `SYNONYM [в] -> [in]` (and `на -> on`, `из -> out`).

The old `pf_filter_input()` rewrote every whole word `в`, unconditionally, so `войти в дом` became `войти in дом` and matched nothing but the catch-all («Я не могу этого сделать !»). The Runner does something quite different, and it is now ported (`scprintf.cpp`, `pf_runner_word_gate()` / `pf_replace_binary()` / `pf_apply_synonym()`):

- run390 45F18C-45F206: `If c(orig, line) Then line = Replace(line, orig, LCase(repl), 1, -1, 0)`; run380 and run400 have the same loop. The Original is used raw from the record: no Trim, no LCase.
- The gate `c()` (4334B0, body 43330C-4334AA) LCase()s both sides and looks only at the FIRST InStr hit that starts the line or follows a space. TRUE iff that hit ends the line or the next character is a space, a comma or a full stop; otherwise FALSE with no further search. A hit inside a word (not after a space) is skipped and the search resumes one character on.
- Behind the gate the Replace is a BINARY substring replace of every occurrence, letters inside other words included.

Consequences, all measured live in Wine run390:

| line | Runner (and Scarier now) | why |
|---|---|---|
| `войти в дом` | enters the house | first hit is the `в` of `войти`, next char `о`: FALSE, nothing rewritten |
| `позвонить в звонок` | «Я не понимаю, что вы хотите!» | the standalone ` в ` passes; Replace gives `позinонить in зinонок` |
| `дернуть за шнурок` | rings the bell, the concierge opens | safe in both engines; this is the line the solution uses |
| `подсказка` | «На данном участке игры подсказок нет» | the game ALRs the Runner's own «No hints currently available.» (run380 42D2D4, run390 437A24, run400 45A0CC; run370 says «No hints available.»). `lib_cmd_hints()` now prints that wording by version. |
| CS2 `св` | «Да?» | an Original with a capital letter (`[СВ] -> [northeast]`) passes the LCase'd gate and then replaces nothing in the lower-cased line |
| shablon `выбросить пачку в урну` | the game's own task (wallet 178$, no fine) | `[в] -> [into]` never reaches the standalone ` в `: the first hit, inside `выбросить`, is followed by `ы` |
| Govard `осмотреть стол` | «Похоже, я делаю что-то не то...» (the game's catch-all; `смотреть стол` examines) | a trailing-space Original (`[осмотреть ] -> [examine ]`) is followed by the noun's first letter, never by a space, so it never passes |

`harness/syn.py <game.taf> <solution>` lists the solution lines where this rule and the old whole-word rule disagree.

## Route (line numbers in the solution)

The `осмотреться` ("look around") lines are not padding: objects and exits must be SEEN before their tasks match (the `%object%`/scope gate), and the square needs three looks before the gate task opens.

1. **Park, 1–13.** `встать`, look, `инв`, `осмотреть себя`, `подсказка` (the ALR'd no-hints line), E, S, S to the house; `дернуть за шнурок` (pull the cord, NOT `позвонить в звонок`), `войти в дом`.
2. **Home, 14–32.** Upstairs, W, S: `выдвинуть верхний ящик` and take the wallet (every money command needs it, obj 22), close the drawer, chips from under the bed; N: the bottle from under the sink, the lamp from the table, matches from the shelf.
3. **Shop, 33–44.** E, down, out, N, `войти в магазин`, `нажать на кнопку`, `купить керосин`, `заправить лампу`; out, S, `время`.
4. **Vincent's office, 45–62.** `войти в контору`, examine Vincent, `спросить Винсента про снять дом` then the blank line 48 for the waitkey; SE, S, S: the key from the pot, `открыть дверь конторы`, in, open the cupboard, take and open the folder, `снять ключ №4` (task 355).
5. **The square and the garden, 63–78.** N, N, three `осмотреться`, `открыть калитку` (task 11), `войти в сад`, up the steps, `войти в библиотеку`, **`осмотреть крест` here, before the hotel room** (task 338 later moves the key ring into the safe door), out.
6. **Hotel room via the roof, 79–106.** Back S, S, cord, house, upstairs, W, S: `открыть окно`, `выбраться в окно`, `лезть по лестнице`, `оторвать доску`, `перекинуть доску`, `перебраться`, `слушать`, `спуститься на балкон`, `войти в комнату`, examine the little door and the key ring, `взять брелок` (the fob); out, down, out.
7. **Night, 107–126.** `время`, two `wait`s: the clock runs 24 -> 25 -> 1 (never 0), and the library must be entered at hour 1–2. N, look, gate, garden, steps, library: `вставить брелок в крест`, `повернуть брелок` (task 342), `зажечь лампу`, `войти в потайную комнату`, look, `осмотреть коробки`, `взять предмет`, out, `осмотреть предмет` (task 347: it is the icon).
8. **Church, 127–146.** `время`, eight `wait`s, `время`, out through the garden, N, `войти в церковь`, `дать икону священнику` (task 348, +10000).
9. **Casino, 147–154.** S, S, `войти в трактир`, `спросить Сэма про казино`, upstairs (the casino), `войти в правую дверь` (task 323), `спросить Барни про долг`: WIN.

## Facts that shaped the route

- **Clock:** each turn is 10 minutes, so 144 turns make a day. Life (`жизнь`) is 720 and drops by 11 every 9 turns.
  - Event 14 fires at turn 420 (task 218) and starts Mark and Engel walking. From turn 422 they warn you (task 220) and then kill you (task 219) if they share room 2, 10, 12 or 13 with the player. The route finishes well before turn 420.
- **Money:**
  - Income: task 348, give the icon to the priest (room 25), +10000; task 1014, sell the spoon to the junk dealer, +500.
  - Costs: task 191 Lenny 500 (+1 point); task 369 Sam 10; task 1060 the priest 100; task 986 rent 550, needs month == 6, probably unreachable.
  - Task 324 (the study door) also checks that you hold the wallet.
- **Icon chain:** key №4 (task 355) -> the square gate (task 11) -> the library at night, turn the fob in the cross (task 342) -> the hidden room, examine the object (task 347) -> give it to the priest (task 348).
- **Joke "win" endings** (ACT 6 v1=0) to avoid: task 36 `прыгать` (jump) in room 0, and task 217 (knock) in room 2.
- **The 35 scoring tasks** (only 10 are taken on this route): 11, 20, 114, 178, 181, 185, 191, 199, 209, 254, 306, 309 (not wearing the chimney-sweep things), 310, 314 (hour < 6), 332, 336, 342, 347, 348, 355, 366/367/368 (the chimneys on three roofs), 369, 373, 942, 943, 970 (16:00-19:00), 986 (rent, June), 1014, 1016, 1046, 1051, 1060. A higher score is a separate project.
- **Engine fix found on the way:** task 661 has an empty command list; `run_task_command_dispatch()` (scrunner.cpp) now guards it instead of reading past the vector.
