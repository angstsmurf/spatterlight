# Renuntio — walkthrough

- **Engine:** ADRIFT 3.90 (`Renuntio.taf`, 48,764 bytes), **in Spanish**, from
  the IF Archive's `adrift/spanish/` shelf (`renuntio.zip`). **27 rooms, no
  NPCs, 53 tasks, 3 events, no variables.** The first Spanish game in the v4
  corpus.
- **Result:** ★ **WON** — T46 `arrancarse * ojos * *` in room 26, the single
  `ACT type=6` in the file. **No score** (not one `ACT type=4`) and only that
  one ending, so the route is maximal by construction.
- **Solution:** `goldens/renuntio_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh` with `SCR_SKIP_WAITKEY=1`). Win marker:
  `Yo-nos me alzo y estiendo mis-nos brazos`
- **Provenance:** no published walkthrough. Derived from `SCR_DUMP_TASKS` and
  play.

## What it is

A novice's initiation into the order of the *Renuntio*, told in the first
person by a woman strapped to a metal bar in the dark. In the prologue the
Renuntio who comes for her recites four renunciations — *to what chains you,
to life taking root in this body, to having your road dictated, and to time
as an anchor and patience as a rare treasure* — and the four doors that
follow are those four surrenders, one apiece. The game is nearly all prose;
of its 53 tasks the large majority are `ex` responses, and only one sequence
is a puzzle at all.

## Route

```
Comienzo         desatarse / cortar tiras / reza / esperar
                 c                                        (T3  → Puerta uno)
Puerta uno       ex puerta / abrir puerta                  (T6  → Puerta dos)
Puerta dos       entrar                                    (T50 → Cubos)
Cubos            ex vigas / cierra ojos                    (T10 → Ojos cerrados)
Ojos cerrados    n / abrir ojos                            (T11 → Pasillo)
Pasillo          ex puerta / abrir puerta                  (T18 → Sala)
Sala             the water loop, three times, below        (     → Pasillo)
Pasillo          ex puerta / abrir puerta                  (T51 → Pozo)
Pozo             soltar a la chica                         (T42 → Pasillo final)
Pasillo final    ex pozo / saltar al pozo                  (T47 → room 26)
A aquello que    arrancarse los ojos con las manos          WIN
te encadena.
```

Thirty-nine commands.

## The one thing you cannot guess: `c`

T3 lives in room 1 and moves you to room 2, and its command is the bare
letter **`c`**. The prompt for it arrives inside **event 0 [Luz]**, which is
timed (`starter=2 start=5..5`) and fires four commands in: the Renuntio
appears, speaks the four renunciations, and the passage ends
`PULSA C PARA CONTINUAR`.

T3 has no restriction, so `c` works from turn one and the route could skip
straight past — it deliberately does not, because that vision is something
like a third of the game's text and the golden ought to hold it. The four
turns spent are `desatarse`, `cortar tiras`, `reza` and `esperar`; the first
two are exactly what a bound person would try and both are written up as
eloquent failures, which is their purpose.

## The water loop

The only mechanical puzzle. A fountain sits at one end of a room and a broken
clockwork hippopotamus at the other, and the machine wants three mouthfuls of
water.

`coger agua` (T33) and `coger * agua * * manos` (T34) both fail on purpose —
your hands are no use to you here — and only **`coger agua con la boca`**
(T35) works. It is a real object move, not a flag: T35 is restricted on the
object `Agua` **not** being held and its action puts it in your mouth, and
T36–38 require it held and move it away again. Carrying water in your mouth
across three rooms is the fourth renunciation made literal.

The room is duplicated twice so the walk is identical each time, and the pour
is what advances you:

| From | Fountain trip | Pour |
|---|---|---|
| 12 Sala | `e` → 13, drink, `o` `o` → 14 | T36 → room 21 |
| 21 Ingenio mecánico 1 | `e` → 19, `e` → 17, drink, `o` `o` | T37 → room 22 |
| 22 Ingenio mecánico 2 | `e` → 20, `e` → 18, drink, `o` `o` | T38 → room 23 |

The pour command is `dejar el agua de la boca en la boca`; `… en la boca del
hipopotamo` is an ALTCMD and works too. On the third pour the machine
whistles, the caged bird is freed, it lands on the door frame, and the door
dissolves — *"Yo tambien soy libre."*

## The well

Room 24 is the game's real turn, and the task that carries it is **T42**,
whose ALTCMDs are `atacar * *`, `soltar * *` **and** `dejar * *` — attack and
let-go collapsed into a single task. Whichever you type you get the same
text: you take the hanging girl's face in both hands and press your thumbs
into her eyes until she falls into the water. This is the memory the whole
game has been circling, and the finale asks you to repeat the gesture on
yourself.

`Rezar` (T48) is also available here and does nothing, as it does everywhere.

## Spanish input

A `SYNONYM` table maps the Spanish forms onto the engine's:

| Spanish | Engine |
|---|---|
| `adelante` | `n` |
| `atras` | `s` |
| `derecha` | `e` |
| `izquierda` | `o` |
| **`o`** | **`w`** |
| `esperar`, `espera` | `wait` |
| `examinar` | `ex` |
| `mira`, `mirar` | `look` |

The double hop `izquierda → o → w` is the one to watch: a bare `o` (*oeste*)
is the west command for the whole run. Everything else the player types is a
task pattern, and the patterns are generous with `*` — `soltar a la chica`
and a bare `soltar` both match `soltar * *`.

Transcripts are CP1252 and full of accented text, so pipe through
`iconv -f cp1252 -t utf-8` before any `awk`. The harness marker was chosen to
be pure ASCII for the same reason.

## Notes

- **WINTEXT is a very long passage**, and being non-empty it suppresses the
  engine's `Congratulations!` exactly the way Asylum's `<br><br>` does. The
  transcript simply ends on the game's own text.
- `reza` exists five times over: T12 (rooms 0–1), T13 (the cubes), T14 (rooms
  12–14), and the one-shot T48 and T49 at the well and at the end. None of
  them has an action. Praying is the thing the character has already given up,
  and the game answers accordingly every time.
- The file has **64 identical `CONTAINER` entries for `Agua`** — the same
  Generator artefact seen in Asylum and Life.
- The `.taf` still carries the author's absolute media paths
  (`D:\descarga\parsers\Nano\Luz.wav`, `…\Renuntio0002.JPG`), so the vision
  scene originally had sound and a picture.
