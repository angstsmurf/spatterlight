# Vardock Bates — walkthrough

**File** `Vardock Bates.taf`, 2,928,980 bytes, ADRIFT **4.00** (Release 51).
**Author** "Pipo98", v1.0.2. **Language: Spanish.**

**Result: WON in 103 commands.** The ending prints

```
Ha nacido el nuevo Dios y Señor de las bestias...Vardock Bates...

El Dios de los muertos...

HAS ELEGIDO LA INMORTALIDAD PARA SIEMPRE...

Has ganado, te espero en la próxima aventura de Vardock Bates.
```

**Artefacts**

| What | Where |
|---|---|
| Route | `goldens/vardock_bates_solution.txt` |
| Transcript | `goldens/vardock_bates_solution.expected.txt` (891 lines) |
| Suite row | `harness/run_v4_walkthroughs.sh`, `vardock_bates_solution.txt\|Vardock Bates.taf\|HAS ELEGIDO LA INMORTALIDAD PARA SIEMPRE\|SCR_SKIP_WAITKEY=1` |

No published walkthrough exists (checked IFDB, CASA, the Spanish CAAD archive),
the file ships no hint menu, and the game is not in the IF Archive's solution
tree. The route was derived from `SCR_DUMP_TASKS` and played through live.

## The game has no score

There is **not a single `ACT type=4` in the file** and `WINTEXT` is empty, so
there is nothing to maximise and no engine-generated win banner to key on — the
suite row matches a line of the winning cutscene instead. As with `relojero`
(the other Spanish 4.00 game in the corpus) no end-of-game score summary is
printed at all.

## Structure

39 rooms, 68 tasks, 77 objects, 4 NPCs (Vagabundo, Taxista, Jason Dhirco,
Lobo), 4 events, 1 variable (`encender-mechero`), five chapter banners each
behind a `<waitkey>` — hence `SCR_SKIP_WAITKEY=1`.

| Chapter | Title | Rooms |
|---|---|---|
| 1 | *El Despertar* | 0 (Oscuridad — buried alive) |
| 2 | *La Búsqueda* | 1–23 (cemetery, street, flat, park, lab) |
| 3 | *El Brazalete Salvador* | 24–28 (Egypt, 1279 BC) |
| 4 | *El Robo* | 29–37 (Barcelona, the museum heist) |
| 5 | *Resurrección* | 38 (Brasil, the choice) |

Vardock Bates was a vampire hunter; Jason Dhirco's coven turned him and buried
him. The bracelet of Ramsés II is the only cure for a vampire bite, which is
why the second half is a museum robbery — and the ending is whether he uses it.

## The Spanish/English parser split

The author writes his task patterns with **English verbs and Spanish nouns** —
`[take]{el}[mechero/encendedor]{de/del}{bolsillo}`, `[light]{el}[mechero]`,
`[read]{la}[carta]` — and ships a `SYNONYM` table that rewrites the player's
Spanish input before matching (`gritar` → `scream`, `coger` → `take`, …). Both
languages therefore parse, and the library replies are Spanish.

One consequence is route-relevant: **the built-in take handler does not accept
the Spanish article.** `coger el revolver` gets *"¿Qué quieres coger?"* while
`coger revolver` works, because the library's noun parser only knows the
object's own name and prefix words. Author-written tasks accept the article
because they spell it out in a `{el}` optional group. Hence the route mixes
`coger el mechero del bolsillo` (task 0) with bare `coger revolver`,
`coger adoquin`, `coger baston`, `coger documento` (all library takes).

## Chapter 1: three flicks of the lighter

The one variable in the game gates the opening. `TASK 1 [light]{el}[mechero]`
is repeatable while `encender-mechero < 3` and adds 1 each time; `TASK 54
escuchar` and `TASK 2 gritar` both require it to be **exactly 3**. So the
coffin is: take the lighter out of the jeans, light it three times (each flick
shows a little more of the coffin lid), listen, then scream — `TASK 2` moves
the player to the cemetery.

## The two hard timers

**`EVENT 0 [Jinetes]`** — `startTask=21` (TASK 20, mounting the horse in
Egypt), `time1=time2=10`, `affTask=24` → **TASK 23 `--Fin--`, `ACT type=6
v1=1` = EndGame LOSE**. `pauseTask=23` → TASK 22 `say * museo *`. Two hundred
dark riders chase you from the moment you mount, and the only thing that stops
them is being in the taxi. The shortest path is eight turns —

```
montar en el caballo / este / este / sur / lanzar el brazalete a la charca
    / sur / hablar con el taxista / decir museo
```

— so there are exactly **two turns of slack**. The route spends one of them on
`examinar el taxi` and keeps the other.

**`EVENT 3 [Lanzamiento de bastón]`** — `startTask=29` (TASK 28,
`hablar con jason` on the Terraza Mirador), `time1=time2=1`, same `affTask` =
death. Dhirco throws his cane at your heart and it lands on the **next** turn,
so `esquivar el baston` (TASK 29) must be the very next command. It hides
Dhirco — he turns into a bat — and drops the cane in room 34.

The other two events are generous: `EVENT 2` is the three-turn taxi ride
(`decir museo`, then two `esperar`), and `EVENT 1 [Mordedura Taxista]` is a
ten-turn hunger clock that starts when the taxi arrives. `morder al taxista en
el cuello` (TASK 26) sates it; the route bites on the turn the craving text
appears.

## The revolver is a trap

Throwing the cobblestone at the bathroom mirror (`TASK 48`) drops **both** a
revolver and the cobblestone into the Aseo. The revolver has exactly one use in
the file:

| Task | Command | Action |
|---|---|---|
| 56 | touch / take / hit / kick / break the wolf | `exec task 23` — death |
| 57 | `disparar al lobo con el revólver` | `exec task 23` — death |
| 31 | `matar a jason` | `exec task 23` — death |

Kork, the wolf guarding room 16, is beaten by **`lanzar el adoquin al lobo`**
(`TASK 58`, requires the cobblestone held), which hides NPC 3. That is also why
the adoquín has to be picked back up after it shatters the mirror — it is the
same stone doing both jobs. The revolver is carried the whole game and never
fired.

## The rest of chapter 2

Nothing is timed, but `TASK 17 coger el maletin` — the chapter's exit — carries
four restrictions and they are the real puzzle:

```
TASK 17 room=21  [take]{el/la}[maleta/maletín/maletin]
    carta held  ·  billete held  ·  task 18 done  ·  task 19 done
```

so all four of the letter, the plane ticket, *reading* the letter and *reading*
the newspaper clippings are required before the briefcase can be lifted. The
letter comes out of mailbox six in the building entrance; the plane ticket is
what the tramp gives you for the motorbike keys (`TASK 16`); the clippings are
in the flat and are what tell you about the Ramsés II exhibition in Barcelona.
Taking the briefcase destroys everything held and worn, grants the brazalete
and teleports to Egypt.

The passage under the back alley opens with `decir el nombre de jhave`
(`TASK 8`) — the name is on the inscription of the park statue, room 23.
`tirar de la palanca` in room 22 (`TASK 9`) opens a shortcut back to the statue
and is optional; the route pulls it anyway because it is one turn and the
transcript then covers the exit-gating code path.

## The endgame: two wins, and the fuller one

Room 38 (Brasil, on top of Christ the Redeemer) holds **both** endings, and
both are `ACT type=6 v1=0` — the engine scores them identically:

| Task | Command | Restrictions |
|---|---|---|
| 36 | `poner el brazalete` (wear it — go back to being human) | none |
| 35 | `lanzar * brazalete *` (throw it away — lead the Committee) | task 36 **UNdone**, tasks 37/38/39 done, documento held |

Because TASK 35 requires 36 undone, wearing the bracelet locks the other ending
out; the reverse is not true. TASK 35 is therefore the fuller ending — it also
requires opening the briefcase (`TASK 37`, which puts the Committee's document
in the room), taking the document (`TASK 38`) and reading it (`TASK 39`), which
is the letter that explains the choice. That is the one wired.

`TASK 32 --Fin--2` and `TASK 23 --Fin--` are the losses.

## Route summary

| Chapter | Commands |
|---|---|
| 1, coffin | 0–6 |
| 2, cemetery → motorbike | 7–11 |
| 2, street / mailbox / flat / mirror | 12–31 |
| 2, park / wolf / statue | 32–46 |
| 2, alley → passage → laboratory → briefcase | 47–65 |
| 3, Egypt | 66–72 |
| 4, Barcelona and the museum | 73–98 |
| 5, Brasil | 99–102 |
