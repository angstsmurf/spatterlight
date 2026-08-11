# Impulso — walkthrough

- **Engine:** ADRIFT 3.9 (`impulso.taf`, 22,214 bytes), in **Spanish**. Seven
  "rooms", 12 tasks, and nothing else — no objects, no NPCs, no events, no
  variables, no exits.
- **Result:** ★ **WON.** There is no score at all (zero `ACT type=4`; `score`
  answers *"Your score is 0 out of a maximum of 0"*), so the finish line is
  TASK 11's `ACT type=6 v1=0`.
- **Solution:** `goldens/impulso_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `Solo una cosa. Me di cuenta hace un cuarto de hora`.
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS`.

## Route

```
score
atacar al anciano                   (room 0 → 1)   * PRIMERA SANGRE
examinar la casa                    (room 1 → 2)
esconder el cuerpo                  (room 2 → 3)   * DOS CUERPOS
atacar al chico                     (room 3 → 4)
atacar a la chica                   (room 4 → 5)   * CADAVER A LA TAZA
meter el cuerpo en el servicio      (room 5 → 6)
bajar los pantalones                (EndGame win)
```

Seven commands. That is the whole game.

## The game is a conversation, not a map

A journalist named Damián has waited four hours outside the barracks to walk
Inspector **Juan Antonio Cortés** home, asking about the serial killer the
papers have christened *el "Carnicero"*. Cortés will not tell him anything he
does not already know — so instead he invites him to reconstruct the three
murders, and **the player types what the killer did next**, in the third
person.

Each "room" is one beat of the interview. Every one of the twelve tasks has
`restr=0`, and the only mechanism in the file is `ACT type=1 v1=0` (move the
player):

| Room | Beat | Advancing task | → |
|---|---|---|---|
| 0 | Don Rodrigo at his gate | T0 `atacar al anciano` | 1 |
| 1 | *"¿Así que qué hizo?"* | T4 `examinar la casa` | 2 |
| 2 | the body is visible from the street | T5 `esconder el cuerpo` | 3 |
| 3 | the couple at the duck pond | T8 `atacar al chico` | 4 |
| 4 | *"¿Y qué pasó con la chica?"* | T7 `atacar a la chica` | 5 |
| 5 | the public toilets | T10 `meter el cuerpo en el servicio` | 6 |
| 6 | *"Faltaba un detalle esencial."* | T11 `bajar los pantalones` | **win** |

There is no losing ending and nothing to carry, so the only way to fail is to
never find the phrasing. Each task is generous about it — TASK 0 alone
carries sixteen alternatives (`atacó` / `ataco` / `atacar` / `asesinó` /
`asesino`, with and without *al anciano*).

## Wrong answers

Four tasks exist purely to reject a plausible-but-early theory, and they have
**no actions at all**:

| Task | Room | Says |
|---|---|---|
| 1 `* ocultar * *` | 1 | *"No podía limitarse a ocultar el cuerpo en plena calle sin más. Tenía un sitio mejor delante de sus narices."* |
| 2 `entró * * *` | 1 | *"No. Primero necesitaba tener las llaves de la verja."* |
| 3 `escapó` | 1 | *"Como ya he dicho el asesino quería ocultar su obra antes de huir."* |
| 6 `atacó * * chica` | 3 | *"No podía ir contra la chica primero ya que ésta era más débil. Debía de acabar con el más fuerte."* |

Anything else falls through to the engine's fallback — and **the fallbacks are
never translated**. Typing the *right* verb one beat too early (`esconder el
cuerpo` while still in room 1, where TASK 5's `where=1 room=2` fails) answers

> You can't do that here!

in English, in the middle of a Spanish transcript. The author localised the
prose and left the runner's messages alone; it is the game's one rough edge
and it reads as a bug to a Spanish-speaking player.

## Notes

- **The stray digits are room names.** `SCR_DUMP_TASKS` prints `ROOM 0 [1]`
  through `ROOM 6 [7]` — the author "named" each beat with its number, so the
  transcript is punctuated by bare `1`, `2`, `4`, `5`, `6`, `7` where ADRIFT
  prints the short room description on arrival. Room 2 has neither a name nor
  a description that prints; its prompt (*"necesitaba esconder el cadáver ya
  que se podía ver desde fuera"*) is the tail of TASK 4's own completion text.
- **`water` and `servicio` are twins.** Room 5 has two advancing tasks, T9
  (`… water`, i.e. the WC) and T10 (`… servicio`), with identical
  `ACT type=1 v3=6`. Either wording works; T10 comes second in list order but
  the patterns do not overlap, so neither shadows the other.
- **All-ASCII commands exist for every step**, which is why the solution file
  is 7-bit even though the `.taf` is CP1252: T5 `ALTCMD[4]=[* esconder * * * *
  *]`, T8 `ALTCMD[2]=[atacar * chico]`, T10 `ALTCMD[4]=[meter * * * *
  servicio]`, and so on. The accented base patterns (`atacó`, `ocultó`,
  `escondió`) all have an unaccented or infinitive twin.
- **No `<waitkey>` anywhere** (`SCR_MARK_WAITKEY=1`), despite the long
  right-justified dialogue blocks — the author formatted the interview by
  hand with trailing spaces rather than using the runner's pauses.
- **The ending is the reveal.** After the third reconstruction the journalist
  says he worked it out fifteen minutes ago but wanted to hear the inspector
  out, and asks *"¿Por qué lo hizo?"*. Cortés: *"todo se reduce a un simple
  impulso… solo existe el ansia, la necesidad del momento."* Then: *"Ahora
  márchese y no se detenga a mirar atrás… la muerte siempre es más dulce
  cuando no se la ve venir."* You have spent the whole game narrating your own
  murders in the third person.
