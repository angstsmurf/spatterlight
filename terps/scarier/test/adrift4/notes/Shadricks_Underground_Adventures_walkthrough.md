# Shadrick's Underground Adventures — walkthrough

- **Engine:** ADRIFT 4 (`ShadricksUnderground.taf`, "Mystery" / Dana Crane,
  InsideADRIFT Summer Comp 2004). You are twelve, your family has just bought
  Ye Old Pub in Willingsdale, and the cellar turns out to open onto a tunnel
  system that bank robbers are using.
- **Result:** ★ **WON, 100/100** — a genuine full score. The dump's seventeen
  awards sum to exactly 100 and this route collects every one.
- **Solution:** `goldens/ShadricksUnderground_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `the robbers were caught red handed in the vault`.

## Route

The author's own walkthrough, from the comp package as
`summercomp04.zip → competition/wthroughs/Shadrickwalkthrough.txt`. It replays
verbatim; the only edit is a `score` inserted before the final `up`, so the
golden records the tally (85) before the closing +15.

```
(blank line — intro waitkey)
look / up / take lantern / d / give lantern to mum / look / open trapdoor / d
(blank line)
x shelf / x jars / open pickled eggs / take egg / open black jar / take foot / x pile / take brick
in / n / smash spider / e / open grate / pull lever / again / e
n / feed rats / again / w / climb ladder / take ladder / w / cross bridge
put ladder over pit / n / nw / n / take boulder / w / put boulder on tall plinth / e
s / se / ne / n / e / take boulder / w / s
sw / nw / n / w / put boulder on medium plinth / e / s / se
n / n / ne / nw / push boulder sw / push boulder se / push boulder s / push boulder s
push boulder nw / push boulder n / push boulder w / put large boulder on short plinth / e / s / se / n
ne / n / nw / ne / s / take ladder / n / sw
se / s / s / s / put ladder over pit / cross ladder / e / s
e / w / w / s / in / score / up
```

(one command per line in the solution file; two more blank lines fall inside
the final stretch.)

## Notes

- **The plinth puzzle is the spine.** Three boulders go on three plinths —
  tall, medium and short. The first two can be carried; the third is a *large*
  boulder that has to be shoved a square at a time (`push boulder sw`,
  `push boulder se`, …), which is why the route's middle is a long string of
  compass pushes with no other commands between them.
- **The ladder is used twice**, over two different pits, and has to be
  retrieved between them (`take ladder` after the plinths are set). Missing
  that leaves the last pit uncrossable.
- **The game declares no MaxScore**, so its own `score` verb prints the comic
  `My score is 85 out of a maximum of 0.  (0%)`. That is the game's own
  omission, not a harness artefact — the per-task awards are all present and
  correct, and the ending is reached with all seventeen collected.
- `again` (twice, after `pull lever` and after `feed rats`) is the author's
  own repeat, and each repeat is a distinct effective turn.
- Five `<waitkey>` pauses, located with `SCR_MARK_WAITKEY=1` rather than
  bisected.
