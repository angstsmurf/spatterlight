# Заноза Билл (zanoza.taf): walkthrough (**WIN, 28/29**)

- **Game:** "Заноза Билл. Золотая лихорадка. Билет в Ванкувер." ("Splinter
  Bill. Gold Rush. A Ticket to Vancouver.") by LARISALISA (Клуб Адвантюристов),
  version of 26 Apr 2007, ADRIFT 3.90, Russian text in cp1251. You play Bill, a
  small-time cowboy in Crouch City, 1 September 1897. He wants to reach the
  Canadian gold fields. The game ends when you board the Houston-Vancouver
  train with a first-class ticket and the treasure map.
- **Wired as:** `zanoza_solution.txt|zanoza.taf|<cp1251 "Поздравляю с победой">|SCR_SKIP_WAITKEY=1`.
  - The solution is 97 commands in cp1251.
  - The row uses `SCR_SKIP_WAITKEY=1`, like the other Russian rows (proba, demoshapka). The two intro `<waitkey>`s ("Нажмите любую клавишу") are skipped, so there are no leading blank lines.
  - Marker bytes: `cfeee7e4f0e0e2ebfffe20f120efeee1e5e4eee9` ("Поздравляю с победой", "Congratulations on your victory"). This text appears only in the task 91 win text.
  - The run is byte-identical across 3 runs of row.sh.
- **Clock:** the game has an in-game clock (`время` shows it). Start is 8:01 on 1 Sept. Most turns take about 10 minutes.
  - `дождаться ночи` ("wait for night") jumps to 0:00 on the next day.
  - At night Bob, Smith and the Mexican leave for the saloon, and the hotel owner leaves the lobby. You can then climb the hotel stairs, and the cemetery shows the entrance to the bunker.
  - The train (tasks 91/92) needs the day counter to be <= 20. This route boards on 2 September.

## Route by phase

1. **Morning, day 1 (6 points).**
   - East to the gun shop street. `придержать колокол` (hold the door bell, task 13) lets you steal the revolver unnoticed (`взять револьвер`, task 11).
   - In the saloon, `прислушаться` four times: overheard talk, three of which score (68-70) and which also point you to the card and the basement.
   - Buy rum. At the far field, the gypsies offer $700 for the revolver. Answering `нет` (task 44, "не согласен") makes them raise the offer, and you end up with $1202.
2. **Day 1 shopping (2 points).**
   - Take the path (`пойти по тропинке`) by day to the cemetery (task 14).
   - Buy whisky at the Smith & Wesson shop, and a rope and a shovel at Uncle Roger's store (room 5).
   - `дождаться ночи`.
3. **Hotel at night (11 points).**
   - With the owner away, take the four keys (tasks 21-24) and then `взять ключи` (task 25).
   - `подняться наверх` (task 20).
   - Enter rooms 1-4 (tasks 16-19). Room 3's text says the exit is west, but the real exit is **east**.
   - In room 1:
     - `open шкаф`, take the evening suit and the suitcase.
     - `обыскать костюм` finds the club card (task 26).
     - `взять чужой бумажник` robs the sleeping guest (task 12). The amount depends on the RNG stream.
4. **Fence and bunker (5 points).**
   - Sell the "expensive things", the suitcase and the suit to the gypsies for $800.
   - Take the path at night (task 15), then go `вниз` into the bunker, where Black Jack waits.
   - `напоить черного джека` twice. The first scores with the rum (task 83). The second scores again with the whisky (task 84), because task 84 has no check that Jack is present.
   - `связать черного джека` (task 85).
   - `взять связанного джека` carries you straight to the sheriff (room 18).
   - `отдать связанного джека шерифу` (task 87) pays the $10000 reward.
5. **Cell (2 points).**
   - Go north into the cell. The door locks behind you.
   - `stand on стол` (on the table), then `заглянуть в оконце` (look through the little window, task 46), then `пугнуть голубя` (scare the pigeon; you catch its twig, task 48).
   - `спрыгнуть со стола` (jump off the table), then `использовать прутик` picks the lock (task 94). Go south.
6. **Map, ticket and train (2 points).**
   - In the saloon, `показать карточку` to Spike takes you to the basement. This needs more than $500.
   - `ставлю 1000` wins the Mexican's map (task 67). The dice needed for this start inside your suede outfit.
   - Task 88 then fires on its own: you hold the map and have at least $10000. "Поздравляю! Вы сумели собрать нужную сумму денег!" ("You have raised the money you need!")
   - At the station, `купить билет за 150` (task 89, first class). South to the platform, then `сесть в поезд` (board the train, task 91). The win text: "Вы сели в поезд и по истечении 10 дней благополучно прибыли в канадский город Ванкувер! Поздравляю с победой! Вы помогли Занозе Биллу приблизиться к своей мечте!" ("You boarded the train and 10 days later arrived safely in Vancouver, Canada! Congratulations on your victory! You helped Splinter Bill get closer to his dream!")

## Score: 28/29, which is the real maximum

The game has 29 separate +1 tasks. Tasks 86 and 87 exclude each other, because both hand over the tied-up Jack:

- Task 86 is the variant for anywhere outside the sheriff's office, and needs Jack held.
- Task 87 is the variant for inside the office, and needs Jack lying there.

Each one removes Jack (obj45), so only one of them can ever fire. The end screen says "Очков пропущено: 1" (1 point missed).

The basement dice games (tasks 73-80, three bets, each paying 3x) are not needed: the reward already brings the total to about $12000.

## Engine oddities and suspected issues (none block the route)

- **`открыть шкаф` is broken in the game data, not in the engine.**
  - Task 97 has an object-state restriction with `v1=0`, which means "the referenced object". Its command has no `%object%`, so the restriction always fails.
  - In the same data, task 100 (`закрыть шкаф`) correctly uses `v1=1`, which is the wardrobe.
  - scare answers task 97 with the ALR'd "You can't do that here!" ("Нет возможности совершить это действие в подобных условиях!"), not with the task's fail text "Уже открыто!" ("Already open!"). What the Runner prints here has not been measured.
  - The route uses the library verb `open шкаф` instead. That is faithful, because the library open handler exists in the Runner.
- **SYNONYM `смотреть`→`look` and `см`→`l`.**
  - `смотреть через оконце` becomes "look через оконце" and never reaches tasks 46/47, whose first ALTCMD is exactly that phrase. The route uses `заглянуть в оконце`, which is ALTCMD 2.
  - `см X` becomes `l X`. scare's examine pattern has no `l`, so `см X` gets the "Попробуйте использовать глагол см" ("Try using the verb см") ALR.
  - The comments in sclibrar.cpp (run390 44B76C-44B833) say that 3.9 examines() *does* accept `l`, and the game's ALRs clearly expect `см X` to examine. This is a possible divergence, but it was not measured against the Runner. The route uses only bare `см`.
- **There is no Russian verb for standing on the table.** `встать/стать/залезть на стол` ("stand/get/climb on the table") all get the "other verb" ALR. The library `stand on стол` works, and prints "You stи  on тюремный стол.", because the game's ALR `and`→`и` also rewrites the "and" inside "stand".
- **Untranslated library text, all left as is:**
  - `наверх` in the basement prints "Вы пошли up." ("You went up.").
  - Walking south in the locked cell prints "You can't go in any direction!".
  - The daytime saloon description prints a raw ` a1`. The game defines ALRs only for `a0` (night) and `a2`.
- **Walking into the cell yourself is allowed.** Room 18 N has no gate. The intended way in is probably just this: the door locks behind you, and the pigeon/twig puzzle is the way out.
- **`взять вещи` needs the things to have been seen.** It fails with "Не понимаю" ("I don't understand") unless you examine the suitcase first. Selling them straight from the suitcase works, so the route never takes them.
