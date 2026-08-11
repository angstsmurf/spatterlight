# Silk Noil — walkthrough

- **Engine:** ADRIFT 3.90 Release 19 (`SILKNOIL.TAF`, 71,345 bytes), by **Heal
  Butcher**, 1 September 2001, ADRIFT Miniature Competition (Summer 2001).
  **4 rooms, 2 NPCs, 259 tasks, 9 events, 2 variables.**
- **Result:** ★ **WON** — `Congratulations!`. **There is no score**: the
  author's own `score` task (T23) answers *"There is no score for this game."*
- **Solution:** `goldens/silk_noil_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `Congratulations!`
- **Provenance:** **the author's own walkthrough**, shipped inside
  `sn_zip.zip` and kept as `downloaded/SilkNoil_walkthrough.txt`. It is ten
  commands long and this route replays it verbatim; the analysis below was
  read back out of the file with `SCR_DUMP_TASKS`.

## Route

```
In Silk Tunnel              up
Hanging with the Silk King  get key / get key / get key / get key
In Silk Tunnel (Again)      pull bolt / pull bolt / pull bolt / pull bolt
By the Green Door           unlock door with key        (EndGame win)
```

No waitkeys (`SCR_MARK_WAITKEY=1` finds none), no name prompt, no inventory
beyond the key, and nothing optional to miss.

## The whole game is two staged counters

The repeated commands are not padding and they are not the same task firing
four times — each is **four separate non-repeatable tasks sharing one
command**, chained on a variable:

| Tasks | Command | Room | Variable | The fourth one |
|---|---|---|---|---|
| 11–14 | `get key` | 1, Hanging with the Silk King | `var0`: −1 → 0 → 1 → 2 | T14 throws you out of the textile maze into room 2 and brings the King with you |
| 7–10 | `pull bolt` | 2, In Silk Tunnel (Again) | `var1`: −1 → 0 → 1 → 2 | T10 drops the King on the flagstones, puts `obj17 [key]` in your hands and moves you to room 3 |

T7 and T11 are unrestricted (beyond the room); T8/T9/T10 and T12/T13/T14 each
carry a `RESTR type=4` testing exactly the value their predecessor wrote. With
`rep=0` on all eight, the *n*-th identical command can only match the *n*-th
task, so the game answers differently every time and escalates on its own:
the key attempts go from *"you hesitate"* to the tiny women crawling over the
King (T13) to being thrown out; the bolt pulls go from a *whimper* overhead to
*"Who is pulling on my testicles?"* to the King coming down.

It is the cleanest example of this pattern in the v4 corpus, which is most of
why the row is worth having.

## The ending

T0 `unlock door` — with `ALTCMD`s `unlock * door with * key` and `open * door
with * key` — is room-locked to room 3, has one restriction (`RESTR type=0`:
you are holding the key) and one action, `ACT type=6 v1=0`, EndGame **win**.
The gag is that opening the green door does not let you out: the King oozes
back in through it, tells you to *"be a good customer like the rest of them"*,
and the game congratulates you anyway. `WINTEXT` is empty, so
`Congratulations!` is all the marker there is.

## Notes

- **`up` is the only real exit in the game.** Room 0's N and S both lead back
  to room 0 — the Tunnel is endless by design — and rooms 1, 2 and 3 are only
  ever reached by a task moving you there.
- **250-odd of the 259 tasks are refusals, not puzzles**: `break me with
  %object%`, `buy %character% with me`, `break * %character% with * %object%`
  and so on, each with its own written answer. A Miniature Comp entry that
  spends its whole budget on responses to things you will not think to type.
- **Nine events, none of them a clock.** Eight are flavour (the King eating
  beetles, spraying perfume, the shop automatons' `BotBabble`); `ThinkUpwards`
  and `PerhapsPullBolt` are 20-turn nudges toward the two commands above.
  Nothing in the game can time out.
- The prose is deliberately grotesque — the Silk King is *"a bloated testicle
  of a man"*, the booths sell *"genital perfumes"*, tiny naked women crawl over
  him — but it is body-horror imagery in a literary comp entry, not AIF, so it
  is committed like the rest of the corpus.
