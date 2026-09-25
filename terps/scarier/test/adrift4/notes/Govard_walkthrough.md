# Govard. Zabvenie (Part 1 — The Golden Dragon) — walkthrough (**WIN**, 300/310)

- **Engine:** ADRIFT 3.90, Russian (cp1251), by LARISALISA (updated 31 May
  2004). «Говард. Забвение.» (часть 1 - Золотой Дракон): the amnesiac King
  Govard escapes a prison, levels up in a small town, clears a list of
  monster contracts and recovers the Sword of Light Saratorr.
- **Result:** genuine win. Final text, from `готов` ("ready") in the dragon's
  cave:
  *«На этом первая часть приключений короля Говарда закончена. Ждите
  продолжения! Спасибо за игру! Ваш счёт 300 из возможных 310!»*
  ("This ends the first part of King Govard's adventures. Await the sequel!
  Thanks for playing! Your score is 300 out of a possible 310!")
- **Solution:** `goldens/govard_solution.txt` (cp1251), 542 lines. Line 1 is
  blank to answer the startup `<wait>` / press-a-key.
- **Env:** none beyond what row.sh already sets (`SCR_RNG=xoshiro`). The
  gambling phase depends on the xoshiro stream (see below).
- **Wired as** `govard_solution.txt|Govard.taf|На этом первая часть приключений|`
  (2026-09-25). The marker is the first half of the WINTEXT line and
  appears once, on the win only (cp1251 hex
  `cde020fdf2eeec20efe5f0e2e0ff20f7e0f1f2fc20eff0e8eaebfef7e5ede8e9`).
- **Synonym-gate revision (2026-09-25):** when the Runner's synonym gate
  was ported (see `Dolg_walkthrough.md`), `harness/syn.py` and a re-run
  showed seven solution lines that the old whole-word rewrite had let
  through and the Runner rule does not. They were re-spelled, so the route
  itself is unchanged:
  - 25 `осмотреть стол` -> `смотреть стол`. The Original `[осмотреть ]`
    carries a trailing space, so it is always followed by the noun's first
    letter and never passes the gate; unrewritten `осмотреть стол` gets the
    game's catch-all «Похоже, я делаю что-то не то...» (Wine run390 probe,
    `Adrift_282_govard_p.txt`). The same holds for `[бросить ]`,
    `[положить ]`, `[открыть ]` and `[взять ]`; `взять X` still works
    because a second synonym `[взять] -> [get ]` has no trailing space.
  - 350, 422, 423, 491 `бросить X` -> `оставить X` (the game's own drop
    verb; `бросить` is not rewritten, and the library only knows `drop`).
  - 369 `положить ножик в охотничий мешок` -> `put ножик в охотничий мешок`.
  - 380 `лезть в отверстие` -> `влезть`. The standalone ` в ` passes the
    gate for `[в] -> [in]`, and the Replace is a substring replace, so the
    line became `лезть in отinерстие`; `влезть` is task 263's ALTCMD.
  - `нажать на камень` (536) is now left alone (the first `на` hit is
    inside `нажать`), which task 259's wildcard matches either way.

## Score: 300 of 310

Only 10 points are missed: tasks 31 («вот ломоть хлеба», give the old woman
bread) and 32 («вот сыр», give her cheese) are both worth 10, and each one
blocks the other through a restriction ("Спасибо, странник, но с меня хватит и
хлеба!" — "Thanks, stranger, but the bread is enough for me!"). So 300 is the
real maximum. The final screen confirms it with «Вы недобрали 10 очк.»
("You fell 10 points short.").

Scoring (each task is scored only once, so repeat kills such as wolf, bear or
robber give nothing more):

| Points | Source |
|---|---|
| 10 | jailer beaten, escape (task 10) |
| 30 | three dogs (26–28) |
| 30 | old woman: cheese (32), water (37), dog carcass (38) |
| 20 | robber (76), goblin (77) |
| 70 | seven bottles of rum to the drunkard (114–120) |
| 10+10+10 | stone fish (252), wolf (249), bear (248) |
| 10+10 | harpy (255), `благодарить` ("thank") in the vineyard (305) |
| 20 | Grim (253) |
| 10 | Sirin (264) |
| 10 | rainbow snake (246) |
| 10 | snake skin to the smith (125) |
| 10 | scorpicore (247) |
| 10 | griffin (254) |
| 10 | vampire (256) |
| 10 | take Romin (297) |
| 10 | basilisk (258) |

## Route by phase (line numbers in the solution)

1. **Prison (1–30).** `встать` ("stand up"), `позвать тюремщика` ("call the
   jailer"), `схватить тюремщика` ("grab the jailer"), `брать ключ` ("take
   key"), `открыть дверь` ("open door"). Then a blank line for the
   `<waitkey>`, then fists on the jailer until he drops.
2. **Ruined city (30–80).** Three dogs, killed with `полоснуть собаку
   кинжалом` ("slash the dog with the dagger"). The old woman gets `вот сыр`
   ("here is the cheese"), water and the dog carcass
   (`свежевать собаку`, "skin the dog"). Then `заночевать` ("stay the night").
3. **Road (83–130).** Robber and goblin. The goblin fight needs three `пить
   бальзам` ("drink balm") swigs. Sell the goblin axe.
4. **Town (130–215).** Seven rums to the drunkard, then level 1 at the fight
   school (`учиться у учителя`, "study with the teacher"). Take the hunter's
   deal: `согласиться` ("agree"), `добываю рог` ("I'll get the horn"),
   `нет`, `нет денег` ("no", "no money"). Buy a roast chicken
   (`купить жареного цыпленка`, a full heal). At the lake: `раздеться`
   ("undress"; you freeze to death if you go in dressed), `войти в озеро`
   ("enter the lake"), then spear the stone fish. After that, `взять всё` /
   `надеть всё` ("take all" / "wear all"), `срезать рог у рыбины` ("cut the
   horn off the fish") and `дать рог охотнику` ("give the horn to the
   hunter").
5. **Hunting (215–260).** `охотиться` ("go hunting"), then the wolf and the
   bear. Skin both (`свежевать мёртвого волка/медведя`, "skin the dead
   wolf/bear") and sell the skins at the skinner.
6. **Harpy (261–296).** Accept the armourer's vampire job. At the tavern say
   `беру работу` ("I'll take the job"), eat a chicken, go west to the
   vineyard and kill the harpy (one balm swig midway). `благодарить`
   ("thank") must be typed there, right after the kill.
7. **Gambling (298–333).** NW of the tavern: `играть в кости` ("play dice"),
   then `ставлю N` ("I bet N"). Event 8 re-rolls var18 every turn, and a bet
   wins when var18 == 1 (task 151; 2 is task 152's loss). The bet ladder was
   found greedily under xoshiro (bet the largest stake you can afford; on a
   loss, spend the turn on `деньги`, "money"), by replaying the prefix once
   per turn with the real engine (35 turns, 15 wins). Gold goes from 56 to
   3006. The ladder was re-derived on 2026-09-25 after the two run390 ports
   below moved the RNG stream; the earlier 39-turn ladder then lost its
   fourth bet and never recovered.
8. **Gear up (339–368).** Buy the Storm sword, Storm shield and Storm helm,
   plate mail, cuirass and leather gloves, and wear them all. Result:
   attack 14, defence 19. Level 2 at the school. Accept the griffin quest
   (herbalist), the Grim quest (skinner) and the snake quest (smith). With
   defence 19, nothing except the basilisk can hurt Govard any more.
9. **Contracts (369–466).** The hunting knife goes into the hunting bag
   (`put ножик в охотничий мешок`, "put the knife in the hunting bag"; the
   Russian `положить ` never passes the synonym gate, see above).
   Otherwise every attack asks «Чем мне атаковать X with?» ("what shall I
   attack X with?").
   - Grim: N of the gate, 8 hits.
   - Sirin: NE, `влезть` ("climb in", task 263's ALTCMD; `лезть в
     отверстие` is mangled by the `в` synonym), 25 hits.
   - Rainbow snake: NE of the gate, 8 hits, then `резать кожу` ("cut the
     skin"). The knife is used from inside the bag.
   - `дать кожу кузнецу` ("give the skin to the smith"). He hands over a
     steel sword and a round shield, which are dropped with `оставить`
     ("leave"), the game's own drop verb.
   - Scorpicore: S, W, W, 8 hits.
   - Griffin: meadow E of the dense forest, `вверх` ("up"), 12 hits.
   - Vampire: W, W to the old cemetery, then `подождать темноты` ("wait for
     darkness"; see oddities). The vampire needs 20 hits of `проткнуть
     вампира` ("thrust at the vampire") with the silver rapier.
10. **Endgame (467–542).**
    - The vampire task drops all carried items in the armoury (room 31).
      Take back the Storm sword there and drop the rapier.
    - `поймать птицу` ("catch the bird") in the abandoned house E of the
      street. The bird turns into the Teacher, who gives the ring and brings
      the horse Romin. This needs 7 blank lines for 7 `<waitkey>`s.
    - `учиться у гейоса` ("study with Geyos"), then `забрать ромина` ("take
      Romin") in the stable, then a chicken to heal.
    - Snake lair, `вниз` ("down"), `север` ("north"): the basilisk, 11 hits
      with Romin kicking too.
    - E ×5 to the underground lake, `нажать на камень` ("press the stone"):
      the Sword of Light Saratorr.
    - `ждать` ("wait"): the goddess Ikata appears.
    - `войти` ("enter"), then `говорить` ("talk"), then a blank line for the
      waitkey, then `готов` ("ready").

## Engine oddities / footguns

- **`ждать темноты` ("wait for darkness") does not fire the vampire task.**
  The game has `SYNONYM ждать -> wait`. The line becomes
  `wait темноты`, so task 132's `*ждать* темнот*` no longer matches and the
  built-in wait answers «Время прошло...» ("Time passed..."). The armourer's
  own hint text says «дождаться темноты» ("wait until darkness"). The route
  uses `подождать темноты` ("wait a while for darkness"): `подождать` is not
  rewritten (the gate looks at the first hit, which is inside `подождать`),
  and `*ждать*` matches it. This is the Runner's synonym-before-match rule
  as ported on 2026-09-25 (`Dolg_walkthrough.md`), not a scare bug.
- **Trap synonyms** (all under the Runner's gate: an Original passes only
  when its first hit at a word start is followed by a space, comma, full
  stop or the end of the line; then EVERY occurrence is replaced as a
  substring):
  - `взять` ("take") is fine, but `брать всё` ("take all") in the armoury
    answers «Похоже, я делаю что-то не то...» ("Looks like I'm doing
    something wrong...").
  - `снять шкуру` ("take off the skin") becomes remove. Skin with
    `свежевать`.
  - `купить бальзам` ("buy balm") matches `*пить бальзам*` and DRINKS a
    swig (line 361). The route keeps that line because it is harmless.
- **Silver rapier cannot `полоснуть` ("slash")**: «You can't stab
  серебряной шпагой!» ("...with the silver rapier"). Use `проткнуть`
  ("thrust", cut) or `атаковать` ("attack").
- **Two weapons held** makes every attack ask «Чем мне атаковать X with?».
  A bare answer like `бури` ("Storm") works, but `меч` ("sword") and
  `мечом` ("with the sword") do not. The route avoids the prompt by holding
  only one weapon. In 3.9 the question is a full turn: run390's dobattle
  (44CE85-44CEDF) sets no not-a-turn byte, so the NPC strikes and the events
  tick on the same line (Runner turn 213: «Чем мне атаковать Волк with? Волк
  укусил меня...»). Only 4.0 makes it a free question; Scarier now gates
  `is_admin` on the version (`lib_battle_attack_bare`).
- **Room alts on a held object, pre-4.0, test the object's own position.**
  The Ruins' «...на юго-востоке - Хибара.» line (room 11, Obj 15 the knife)
  shows only while the knife is carried directly, not once it is in the
  worn belt, and the Road's alt on the licence never shows while the
  licence is inside the hunter's bag. run390 isdark (433920) reads the
  object's raw position field; 4.0 walks containers. Ported in
  `lib_use_room_alt` (case 2, conditions 0/1) with a `< 4.0` gate.
- **Status display** after Geyos shows «Атака: 14 (6)» and «Защита: 20 (3)».
  Geyos raises the *max* strength and defence, and the current values are
  not refreshed.
- Several leftover probe lines are harmless and fail quietly: a few
  `ударить тюремщика` ("hit the jailer") after he leaves, one surplus wolf
  and bear attack, and `снять шкуру`. They sit before the gambling phase,
  whose outcomes depend on the turn count, so they were kept.

## Determinism and the Runner

`row.sh Govard.taf goldens/govard_solution.txt` gave byte-identical output
3 times (3290 lines) before the synonym-gate revision; the `.expected.txt`
file was regenerated on 2026-09-25 with the suite pipeline and differs from
that run only in the seven echoed command lines.

**Wine run390 drives (2026-09-25).** The first drive of the re-spelled
route (`Adrift_282_govard_rt.txt`, run390x, seed 1234) ended with the Runner
dead in the Sirin fight at 210/310 while Scarier won. The compare
(`runner_transcripts/compare/govard.txt`) came down to two engine rules,
both confirmed in the run390 decompile and ported:

- **T213, the weapon question is a turn in 3.9** (see the oddities above).
  Scarier had answered «Чем мне атаковать Волк with?» without ticking, so
  from there on its RNG stream, event timing and every fight differed from
  the Runner's.
- **T74/T210/T248, room alts on a held object test the object's own
  position pre-4.0** (see above).

With both ported, Scarier reproduced the Runner's death turn for turn, so
the dice ladder was re-derived against the corrected engine (this file's
phase 7) and the route re-driven. Two Runner artefacts remain and are
deliberate deviations, not ported (`WINE-TRANSCRIPTS-TODO.md`):

- **T34-45** the Runner prints «чёрную собаку hits me.» where the game's
  own «Чёрная собака меня укусила.» belongs. Its cp1251 text goes through
  VB6 `UCase` under the English locale, which leaves ч and я alone, so the
  case-sensitive match on the NPC's line fails and the English library
  line is printed with the raw accusative name. Scarier keeps the game
  text.
- **T192/T193** the Runner's `взять всё` / `надеть всё` lists end with a
  phantom empty-name item («..., белый гриб и», «..., дорожный плащ, и
  кожаный пояс.»).

**Second drive (2026-09-25, `runner_transcripts/govard.txt`):** all 530 feed
commands echoed, the Runner wins 300/310 on the same turn as Scarier, and
the only differing turns are the eight artefact turns above (T34-45 dog
line, T192/T193 phantom list item). Manifest verdict: win yes, 8 differing
turn(s), both classified as deliberate deviations.

**Wine drive footgun (2026-09-25):** Govard's intro carries a chain of
real-time `<wait N>` tags (about 20 s in total) before its first keypress.
drive.exe decides the intro has "settled" after 3 s of quiet scrollback,
which lands inside one of those waits, and its pre-transcript Return then
hits the Runner's wait loop: the Runner dies with "Run-time error '9':
Subscript out of range" as soon as the entry box is re-enabled, and the
log says "the pre-transcript Backspaces did not complete". This is not a
game or engine defect (run390.exe played by hand in the same prefix loads,
takes the keypress and the commands). Drive it with
`INTRO_QUIET=25 PRE_SLEEP=30` in front of `xoshiro_par.sh`; `govard2`, by
the same author, will need the same.
