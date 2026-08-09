# Crimson Detritus — walkthrough (**WON 100/100**, 16 commands)

- **Author:** "Mystery" — the game's own `about` gives *"Crimson Detritus", 22
  Aug 2003, Mystery*. You are caught playing *To Hell in a Hamper* at work and
  your boss zaps you onto a planet covered in crimson dust.
- **Engine:** **ADRIFT 4.00**
  (`xxd -p -l 12 games/CD.taf | cut -c17-22` → `93453e`).
- **Result:** **WON, 100/100** — the game contains eight `ACT type=4` and this
  route fires all eight.
- **Source:** `downloaded/CrimsonDetritus_walkthrough.txt`, from `cd.zip` — a
  full session transcript rather than a command list. It replays with one
  repair.
- Row: `crimsondetritus_solution.txt|CD.taf|until the next victim comes along to take your place.|SCR_SKIP_WAITKEY=1`.

## The one repair

The transcript has a line

```
take uniform and wear it
You take the uniform from the hook.
        You flip the uniform around every which way, ...
```

Two responses to one line, so it was two commands in the original session and
the file was pasted together wrong. SCARE reads it as one and answers
`[take uniform and wear the hook] / I only understood you as far as wanting to
take the uniform.` Split into `take uniform` / `wear uniform`.

Getting the uniform *on* is not optional: the corridor's north door answers
"Didn't you read the sign???" until you are wearing it, and the uniform is also
the reason you can never take it off again — which is the joke the ending turns
on.

## Where the hundred points are

| task | command | points |
| --- | --- | --- |
| TASK 3 | `rub cream on` | 10 |
| TASK 4 | `in` (past the creature) | 5 |
| TASK 8 | `wear uniform` | 10 |
| TASK 10 | `press red button` | 10 |
| TASK 16 | `take remote` | 10 |
| TASK 19 | `slide` | 10 |
| TASK 15 | `flip switch` | 10 |
| TASK 20 | `press white button` | 35 |

TASK 20 carries the win (`ACT type=6 v1=0`) as well as the 35, and the points
are awarded before the ending prints, so the game really does close at 100.

## The two ways to lose

Both are `ACT type=6 v1=2`, and both sit directly on the route:

* **TASK 5** — walking `in` past the creature without having rubbed on the
  vanishing cream first.
* **TASK 18** — pressing the white button while still *inside*, at the
  Assembly line control panel. The white button only does its job from outside
  the building, after the slide has dumped you back on the dust and you have
  flipped the antenna switch (TASK 15, `where=3`, so it works anywhere).

## A cosmetic divergence, left alone

The endgame text stored in the game contains three literal `{}` sequences —
one after each `<br><br>` that precedes the teenager's three speeches — and
SCARE prints them. The author's transcript does not show them. Whether run400
strips brace groups from output text or the author simply tidied the file has
not been arbitrated; no other game in the corpus emits `{}` at all, so there is
nothing else to reason from. Recorded in the golden as-is.

## Shape of it

`goldens/crimsondetritus_solution.txt`, 16 lines:

```
take tube / rub cream on / in           past the creature invisible
x hook / take uniform / wear uniform    <- the transcript's one bad line
in                                      the sign-guarded door
x control panel / press red button      stops the conveyor belt
l / take remote                         the remote is under the belt
up / inventory / slide                  back outside the building
flip switch / press white button        -- WIN, 100/100
```
