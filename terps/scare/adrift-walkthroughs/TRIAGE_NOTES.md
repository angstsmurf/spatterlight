# Triage of the 32 previously-untouched games

Structural dump (SC_DUMP_TASKS) classification. "win" = a type-6 EndGame with
`var1=0` (a real victory) exists; "score" = at least one type-4 ChangeScore.

## A. WINNABLE — score + a real win ending (derive full walkthroughs)
| game | KB | tasks | score-actions | win-endings | status |
|---|---|---|---|---|---|
| cyber | 1.8 | 5 | 3 | 0* | **DONE 150/150** (*only a lose-stop ending; max score reached) |
| cyber2 | 3.4 | 14 | 10 | 2 | **DONE WIN 355/355** |
| Jason Vs. Salm | 2.9 | 19 | 3 | 4 | **DONE WIN, honest max 0/1000** |
| FunHouse | 3.6 | 31 | 4 | 2 | **DONE WIN, max 310/410** (cassette→ticket man; maze BFS-mapped; +100 locked behind 2 orphaned NO_ROOMS tasks) |
| TheCatintheTree | 6.2 | 16 | 1 | 1 | **DONE WIN 50/50** |
| Del Sol | 6.5 | 28 | 16 | 1 | **DONE unwinnable, 24/46** |
| Colony | 14.8 | 10 | 4 | 1 | **DONE WIN 200/200** |
| thetest | 22 | 29 | 6 | 1 | **DONE — UNWINNABLE, max 5/25** (the win exists but is sealed by a circular lock on the first door: #unlockdoor needs robot2==3, set only by #shoutrobots which needs Room 1, which the door gates) |
| light_up_4summer_comp | 28 | 71 | 19 | 1 | **DONE WIN 73/75** (5-chapter horror; Battle System works; `hard`+15; laptop +2 RNG-conflicts) |
| Melbourne Beach | 70 | 107 | 19 | 1 | **DONE WIN, max 38/41** (beach-house morning chores → drive → rinse sandy feet at the beach shower = win; 3 lost pts are faithful data: non-repeatable wash makes turn-dryer+1 XOR fold+5, and 2 red-cup coffees are oil-contradiction / oil-penalty net-0) |
| The Screen Savers On Planet X | 80 | 133 | 15 | 1 | todo |
| ALEXIS | 87 | 78 | 25 | 1 | **DONE — WON 23/65** (native 3.9; collect 7 stones→give Larnt→password into Uron→keys→Mirror-room `north`-only trap→kill Urgorn; Serond NPC assists, only mandatory fight; light lantern AT HOME; banked min-combat win, ~42 optional pts) |
| Shadowpeak | 96 | 574 | 69 | 1 | todo (large) |
| circus | 117 | 158 | 58 | 3 | todo (large) |
| WesGHN | 3500 | 25 | 12 | 1 | **DONE — UNWINNABLE, max 30/100** (orphaned gold ring on a never-revealed severed hand seals the Lovers'-Fountain gate → whole back half unreachable; combat is correctly configured, scythe kills Hope) |
| Space Boy's First Adventure | 3865 | 78 | 37 | 1 | **DONE — WON 1184/1374** (native 4.0; 4 power items mimic the lost cape: Flight Boots/Ice Gloves/Heat Goggles/Strength Belt → fly N/S/E from the hub. East = "TOTHEGARAGE" letter maze w/ melt-ice + freeze-fire elemental gates → Phased-Ion-Bridge-in-Power-Plant transporter → fork → belt; endgame move-rock→Room-Key→unlock door→take+drop cape→read note. Evil Man harmless. Dead Ice-gloves +30 typo caps max ≤1344) |

## B. Win ending but NO score (pure 0/0 win)
| game | KB | tasks | win-endings |
|---|---|---|---|
| donuts_intro | 5.2 | 6 | 1 | **DONE WIN (0/0 intro)** |
| Main Course | 7.4 | 11 | 1 | **DONE WIN (0/0)** — eat cat→use toilet→button wakes pilot→wear cat-fur disguise→kill+eat pilot→`main course`. Deterministic |
| Bomb Threat | 18 | 16 | 2 | **DONE WIN (0/0)** — defuse the skyscraper bomb (cut RED wire, blue=death) then win the Edgar shoot-out; `shoot edgar` is `hit+=random(1,3)` (1/2 win, 3 death), pad 1 `z` for the deterministic headshot |
| tcom | 37 | 29 | 1 | **DONE — WON, 0/0** (*The Cave of Morpheus* Pt 1: dash across campus to the exam-hall door while Death chases; win = `open wooden door`, no restriction; Death harmless) |

## C. Score but NO win ending (max-score only; document like Mr_Smith/V&K)
| game | KB | tasks | score | endings |
|---|---|---|---|---|
| Trabula | 2.2 | 4 | 2 | none | **DONE — no ending, max 125** |
| QuestI | 2.5 | 13 | 2 | deaths only | **DONE — unwinnable, max 10** |
| Matt's House | 33 | 105 | 1 | none | todo |
| Les Feux de l'enfer | 54 | 289 | 18 | 5 (lose/death, no win) |

## D. No score, lose/stop ending only (unwinnable; document)
| game | KB | tasks | endings |
|---|---|---|---|
| Through time | 42 | 164 | 1 lose / 1 death / 8 stop (no win) |
| adriftorama | 46 | 51 | 1 lose | **DONE — no victory, mini-golf** |

## E. No score, no ending (sandbox / not-a-game / intro)
| game | KB | tasks | rooms |
|---|---|---|---|
| IceCream | 5.0 | 15 | 1 | **DONE 0/0 sandbox** |
| Invasion of the Second-Hand Shirts | 5.3 | 19 | 7 | **DONE 0/0 no ending** |
| SRSintro | 158 | 37 | 3 | **DONE — INTRO DEMO, 0/0 no ending** (*Silk Road Secrets*; 0 type-6 + 0 type-4; Marketplace→Citadel(Khan's Jan-wa sword)→Shrine(priest lore); NE-to-shrine gated on taking the sword) |

## F. "Hangs after Loading game…" — MISDIAGNOSED: they just use the name+gender prompts
These three are **not** hangs. With `</dev/null` they print nothing after the
banner because they block on the **"Please enter your name"** + **"choose
gender"** start-up prompts (the committed prompt support). Feed `name`,`male` and
they boot and play normally. All three have a real win ending (type-6 var1=0):
- **Theannihilationofthink2** (10 tasks) — **DONE, WON 35/35** (see
  `Theannihilationofthink2_walkthrough.md`). Surfaced + fixed a real SCARE
  command-separator bug (split on bare `.` vs the Runner's `. `/`,`/`then`),
  which had made it unwinnable.
- **deaths** (62 tasks, 9 score, 1 win) — playable; "House of Death" RPG, not
  yet banked.
- **lair-of-the-cybercow** (226 tasks, 17 score, 1 win) — **DONE: max 10/10,
  deterministic; the lone win-ending appears unreachable.** Conrad Cook, ADRIFT
  3.9, IF Comp 2008. See `Lair_of_the_CyberCow_walkthrough.md` /
  `harness/cybercow_solution.txt`. Surfaced + fixed a real SCARE bug (walk
  `MeetObject` dynamic→global conversion missing for 3.9/4.0 — the fairy
  *Vluurinik* was uncatchable, capping the game at 6/10). 10/10 route ends at the
  Robot-vs-CyberCow confrontation; the "fairy kiss" `#end game` (task 175) needs
  the lair-flood, whose only entrances (tasks 98/112/117) are all deadlocked.
