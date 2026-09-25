# Govard. Zabvenie (Part 2 — The Book of Genesis) — walkthrough (**WIN**, 22/22)

- **Engine:** ADRIFT 3.90, Russian (cp1251), by LARISALISA, v1.4 beta
  (updated 14 Feb 2005). «Говард. Забвение.» (часть 2 - Книга Бытия): the
  Golden Dragon sends King Govard and his horse Romin to the port city of
  Trion. There he earns money and skills, recovers the Book of Genesis
  (Книга Бытия) from the Island of Despair, and reads it in the Trion
  library to recover his memory.
- **Result:** a genuine win at the maximum score. The final text comes from
  `раскрыть книгу` ("open the book") in the library (task 264, ACT
  end-game). It ends with:
  *«Ворота Горра почти открыты, но теперь я знаю, что надо делать! Теперь я
  обрёл все свои знания и умения и готов к решающей битве! Лерилей
  проиграет, это сказал я, король Говард, и будет так!»*
  ("The Gates of Gorr are almost open, but now I know what must be done! I
  have regained all my knowledge and skills and am ready for the decisive
  battle! Leriley will lose. I, King Govard, have said it, and so it shall
  be!")
  After that: «Вы набрали 22 из максимума в 22! Это 100% от всей игры!
  Отличная игра - Вы набрали максимальное количество очков!!» ("You scored
  22 out of a maximum of 22! That is 100% of the game! Excellent game — you
  scored the maximum number of points!").
- **Solution:** `goldens/govard2_solution.txt` (cp1251), 339 lines. There
  is no startup press-a-key. Blank lines are at:
  - 151: Krag's `<waitkey>`
  - 248–249: the pirate's two `<waitkey>`s
  - 334–339: the six ending `<waitkey>`s. Headless scare does not consume
    these (the output is identical with 0–7 blanks). They are kept so a
    Runner feed does not cut the ending.
- **Env:** none beyond what row.sh already sets (`SCR_RNG=xoshiro`). Three
  fights depend on the xoshiro stream: the lion, the gargoyle and the bear.
  See below.
- **Proposed manifest row:**
  `govard2_solution.txt|Govard2.taf|готов к решающей битве|`.
  - The marker comes from the final line of the ending text (task 264). It
    appears exactly once, on the win only.
  - cp1251 hex: `e3eef2eee220ea20f0e5f8e0fef9e5e920e1e8f2e2e5`.

## Score: 22 of 22

The score is 22 one-point tasks (ACT type 4), and all of them are reached:

| Task | Command (line) | What |
|---|---|---|
| 367/368/370 | `обыскать песок` ("search the sand"; lines 1, 128, 245) | ruby in the dragon's cave, then two later sand searches |
| 239 | `говорить со стражем` ("talk to the guard"; 17) | enter Trion |
| 366 | `обыскать фонтан` ("search the fountain"; 26) | |
| 129 | `продать рубин` ("sell the ruby"; 29) | |
| 185 | `молиться` ("pray"; 32) | temple |
| 96 | `купить отшлифованный щит` ("buy the polished shield"; 53) | |
| 117 | `продать волчью шкуру` ("sell the wolf skin"; 82) | |
| 114 | `продать львиную шкуру` ("sell the lion skin"; 141) | |
| 247/249 | `поискать скрытое` ("look for hidden things"; two of lines 126, 131, 134) | needs the arcane-sight course |
| 157/158/159 | basilisk: shield, `вырезать глаза` ("cut out the eyes"), `наполнить склянку слюной` ("fill the vial with saliva") (156–158) | |
| 124 | `продать склянку со слюной` ("sell the vial of saliva"; 168) | |
| 248 | `поискать скрытое` (212) | also gives the second teleport scroll half (var24) |
| 179 | `идти на болото` ("go to the swamp"; 220) | ghoul |
| 122 | `дать жемчужное ожерелье Патриции Линн` ("give the pearl necklace to Patricia Linn"; 236) | |
| 315 | `использовать порошок окаменения` ("use the petrifying powder"; 254) | kraken |
| 251 | `использовать золотую статуэтку` ("use the golden statuette"; 273) | Island of Despair |
| 121 | `отдать шкуру Оливеру Вонгу` ("hand the skin to Oliver Wong"; 324) | bear skin |

## Route by phase (solution line numbers)

1. **Dragon's cave (1–2).** `обыскать песок` finds a ruby. `готов`
   ("ready") takes you to the shore near Trion.
2. **Sirin (3–11).** `лезть в отверстие` ("climb into the hole"), six
   `рубануть Сирин` ("chop Sirin").
3. **Trion (12–60).** Show yourself to the guard. Swap the old shirt for a
   silk one, search the fountain, sell the ruby and pray. Swap the cuirass
   for a satin doublet and buy:
   - vials and an antidote
   - a big torch and an iron spade
   - the polished shield

   Then take the full arcane-sight course (`полный курс тайновидения`).
4. **Wolf and lion (61–148).**
   - Fill a vial at the pit, then the night wolf: `дождаться ночи` ("wait
     for night"), 5 hits, `срезать шкуру` ("cut off the skin").
   - Put the cuirass back on. `скакать в Трион` ("ride to Trion") and sell
     the skin and the blood vial.
   - Stable Romin, then the lion: 17 hits, with a balm and 7 feeds for
     Romin.
   - Hidden-thing searches and `вырыть клад` ("dig up the treasure").
   - Back in Trion: sell the lion skin, buy another vial.
5. **Krag / basilisk (149–169).** `говорить с Крагом` ("talk to Krag"),
   then `согласен` ("agreed") and a blank line for the waitkey.
   - Swap in the polished shield, go down, `лезть в дыру` ("climb into the
     hole"), `использовать отшлифованный щит` (the basilisk is petrified by
     its own reflection), cut out the eyes, fill the vial with saliva.
   - Put the Storm shield back on and report to Krag.
   - Take the full lock-picking course (`полный курс взлома`), sell the
     saliva vial, and get the petrifying powder from the enchantress.
6. **Gargoyle (170–213).** `взломать замок` ("pick the lock"), light the
   torch, `ковырять оникс` ("pry at the onyx"). Gargoyle: 17 hits, 3 balms,
   4 Romin feeds. Take the golden statuette, go down, search, and
   `говорить с мальчиком` ("talk to the boy").
7. **Ghoul (214–236).** `идти на болото`. One `рубануть упыря` ("chop the
   ghoul") kills it.
   - `бросить факел` ("drop the torch"): hands are full.
   - `look`, then `взять ожерелье` ("take the necklace"), then
     `выпить противоядие` ("drink the antidote").
   - Back to Trion, give the necklace to Patricia Linn.
8. **Pirates and kraken (237–259).** Buy the teleport and revival scrolls.
   - `готов` shipwrecks you on the beach. Search the sand.
   - `говорить с пиратом` ("talk to the pirate"): two blank lines, then
     `согласен`.
   - `использовать порошок окаменения`, `отрезать зелёное щупальце` ("cut
     off the green tentacle"). Back to the pirate: he takes you to the
     schooner.
9. **Island of Despair (260–282).**
   - Pick the chest, `look`, take the wind rose. Feed Romin ×5.
   - Search, `вставить розу ветров в углубление` ("put the wind rose in the
     recess"), `пройти в проход` ("go through the passage"), use the
     statuette.
   - Feed Romin ×3 and take a balm swig before the golems.
   - `дотронуться до помоста` ("touch the dais"), `стереть слово` ("erase
     the word") ×2, `провести кольцом над помостом` ("pass the ring over the
     dais"), `забрать книгу` ("take the book").
10. **Trion again (283–293).** `телепортироваться` ("teleport"). Buy oats.
    `поставить`/`забрать Ромина` ("stable" / "take Romin") refills him to
    full. Talk to the captain.
11. **Bear (294–314).** Go to the Kronda mountain path (room 23). Two
    `ждать` ("wait"), then `северо-запад` ("northwest") to the Маралисовая
    Пустошь (Maralis wasteland, NPC 37: stamina 145).
    - Fight: 9 hits, 1 balm (`рубануть медведя` = "chop the bear").
    - `срезать шкуру`: the skin goes into the hunting bag.
12. **Oliver (315–324).** To room 38, `отдать шкуру Оливеру Вонгу`: the last
    point.
13. **Finale (325–339).** To the library.
    - `пройти в зеркало` ("go into the mirror"; task 25).
    - `оживить изваяние` ("revive the statue"; 27, uses the revival scroll).
    - `look`: event 4 fires task 78 (#старец). Archius thanks you and moves
      you both back to the library.
    - `говорить со старцем` ("talk to the old man"; 233), then `вставить
      изумрудное кольцо в углубление` ("put the emerald ring in the recess";
      259), then `раскрыть книгу` (264: the ending).

## RNG-dependent fights

`battle_select_target` (scbattle.cpp) picks uniformly at random between the
player and Romin for each enemy blow. Damage is fixed:

| Blow | Damage |
|---|---|
| Bear on me (strength 50, defence 20) | 30 |
| Bear on Romin (defence 13) | 37 |
| My sword | 11 |
| Romin's kick | 5 |

`накормить Ромина` ("feed Romin") gives him only +30, so a run of bites on
Romin kills him, and **Romin's death ends the game** («Нельзя было допускать
этой смерти!», "This death should not have been allowed!").

- With no `ждать` before entering the bear's room, the bear bit Romin four
  turns running and the fight could not be won.
- The two `ждать` on lines 301–302 shift the xoshiro stream so the bites
  split. The sequence was found by stepping one turn at a time: at each
  turn the next target was read from `SCR_TRACE_BATTLE`, since the target
  roll does not depend on my action. Then I healed only when needed.
- The gargoyle and lion sequences were found the same way (adaptive
  balm/feed).
- Any change before these fights needs them re-derived.

Romin keeps following you (walk 0, stopTask 26) even after he is stabled:
`поставить Ромина в стойло` moves him to the stable, but the walk brings him
back. So the bear cannot be fought solo, since task 26 (leaving the mirror
room without reviving the statue) is the only thing that stops the walk. The
stable task pair is used only to top him up. This looks authored, not an
engine issue.

## Engine oddities / footguns

- **Verb synonyms hide task commands.** The game's SYNONYM table rewrites
  the verb before task matching:
  - `войти` → enter
  - `открыть` → open
  - `взять` → get
  - `поднять` → pick
  - `идти` → go
  - `искать` → find
  - `ждать` → wait

  Some effects:
  - `войти в зеркало` gives «Попытайтесь по-другому» (the ALR text for
    "Just a direction will do", i.e. "Try another way"). Use `пройти в
    зеркало`.
  - `открыть книгу` gives the library refusal «Я не могу открыть это .»
    ("I can't open this ."). Use `раскрыть книгу`; it still matches task
    264's `*крыть книг*`.
  - The Book of Genesis is taken with `забрать книгу`: `взять книгу` and
    `поднять книгу` answer «Взять что?» ("Take what?").
  - `наверх` ("upwards") works for up; `вверх` ("up") does not.

  This is the Runner's synonym-before-match order, as in Part 1. It was not
  checked against the Wine Runner.
- **Take needs the object seen.** After the ghoul dies, `взять ожерелье`
  answers «Взять что?» until a `look` shows the necklace (line 223).
- **Hands full**: the torch is dropped after the gargoyle caves (line 222).
- `положить X в мешок` ("put X in the bag") answered «Я не пойму, что делать
  с объектом» ("I don't understand what to do with the object") during
  derivation. The route does not need it, since the skins go into the
  hunting bag by themselves.
- Level-up (task 8, experience == 100) adds to *max* strength only. The
  trace keeps `strength 21` for the player's blows afterwards. Levels 2+
  need experience to equal exactly 200/300/…, and this route never lands on
  those values (it ends at 160). That doesn't matter for the score.
- Battle text is from the ALR and is slightly ungrammatical: «Медведя укусил
  Ромина» ("The bear's bit Romin", wrong case). This is cosmetic.

## 3-run determinism

`row.sh Govard2.taf goldens/govard2_solution.txt` gave byte-identical output
three times (md5 51f0383db8d12f280ba6a553eeecc58a, 2624 lines). That output
is `goldens/govard2_solution.expected.txt`. Every command in the transcript
is accepted: none of them gets «Похоже, я делаю что-то не то», «Я не пойму»
or «Взять что?».
