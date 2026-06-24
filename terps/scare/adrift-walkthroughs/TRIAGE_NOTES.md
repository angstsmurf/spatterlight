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
| thetest | 22 | 29 | 6 | 1 | DEFERRED — winnable puzzle-box (color key + phone number + teleporter); route not yet banked |
| light_up_4summer_comp | 28 | 71 | 19 | 1 | **DONE WIN 73/75** (5-chapter horror; Battle System works; `hard`+15; laptop +2 RNG-conflicts) |
| Melbourne Beach | 70 | 107 | 19 | 1 | todo |
| The Screen Savers On Planet X | 80 | 133 | 15 | 1 | todo |
| ALEXIS | 87 | 78 | 25 | 1 | todo |
| Shadowpeak | 96 | 574 | 69 | 1 | todo (large) |
| circus | 117 | 158 | 58 | 3 | todo (large) |
| WesGHN | 3500 | 25 | 12 | 1 | todo (graphics-heavy, tiny game) |
| Space Boy's First Adventure | 3865 | 78 | 37 | 1 | todo (graphics-heavy) |

## B. Win ending but NO score (pure 0/0 win)
| game | KB | tasks | win-endings |
|---|---|---|---|
| donuts_intro | 5.2 | 6 | 1 | **DONE WIN (0/0 intro)** |
| Main Course | 7.4 | 11 | 1 | DEFERRED — winnable (eat cat+pilot→"main course"); catnip/disguise puzzle not yet banked |
| Bomb Threat | 18 | 16 | 2 |
| tcom | 37 | 29 | 1 |

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
| SRSintro | 158 | 37 | 3 |

## F. Hangs after "Loading game…" (need investigation — intro loop / prompt)
- Theannihilationofthink2
- deaths
- lair-of-the-cybercow
