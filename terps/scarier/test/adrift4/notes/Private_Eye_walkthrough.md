# Private Eye — walkthrough

- **Engine:** ADRIFT 4, David Whyld, 2006.
- **Result:** **WIN at the author's best ending — score 4, "Better than
  Sherlock Holmes himself."**
- Solution file: `goldens/private_eye_solution.txt` (74 lines: one title-menu
  choice + 73 in-game choices).
- Harness row: `private_eye_solution.txt|Private Eye.taf|You achieved a score of 4.|SCR_SKIP_WAITKEY=1`
- Source document: `downloaded/PrivateEye_guide.pdf` (a 116-page guide; the
  walkthrough proper starts at its page 13).

## Derivation

Private Eye is a **pure numbered-choice game** — there is not a single parser
verb in the whole route. Every turn the game prints a numbered menu and the
player types a digit. The solution file is therefore just a column of digits:

```
3 1 1 2 2 2 1 1 1 1 2 2 1 1 1 1 2 1 1 2 1 2 1 1 1 1 2 3 1 1 2 2 2 1 1 2 1 4
2 3 2 1 1 3 1 2 3 1 1 7 2 1 2 2 1 2 3 1 2 1 1 1 1 2 2 1 1 2 2 3 1 1 1 1
```

The guide's walkthrough section replays **VERBATIM** at the best ending — 73
choices, no repair.

Two harness-level facts were all that stood between the PDF and a passing row:

1. **The leading `3`.** The PDF's walkthrough opens at *"The first thing I did
   was read through the file she had left me"*, which is already inside the
   game. It silently omits the title menu — `(1) Read the introduction`,
   `(2) A few notes about the game`, `(3) Play Private Eye`. The script carries
   the `3`.
2. **`SCR_SKIP_WAITKEY=1` is mandatory.** The game leans on `<wait>` constantly;
   the title sequence alone eats several, and without the skip the run never
   gets past the menu.

## The one textual difference, and why it isn't ours

Word-diffed against the PDF, the blessed transcript is **19293 / 19371 words
identical (ratio 0.9975)**, and the whole delta is one 12-word sentence that
appears in the PDF and not in the run:

> "No sooner have I put the phone down than Jim ambles in."

That is the author bridging two scenes in the write-up, not output the engine
drops. Three independent checks:

- The game's own wording for that beat is longer — *"…than Jim ambles in **and
  plonks himself down in the other chair**"*.
- It lives on the **ex-girlfriend** phone-call tasks, not on Layla's, so it is
  not even on this route.
- The exact short form appears **nowhere in the inflated .taf** — grepping the
  decompressed task/text blocks finds no match.

So the transcript is complete; the PDF has an extra authorial sentence.

## Scoring

The endgame prints a rank table:

```
Score 4 or more - Better than Sherlock Holmes himself.
Score 3        - Almost on par with Sherlock Holmes.
Score 2        - Private eye extraordinaire.
Score 1        - Reasonable private eye.
Score 0 or less- Hang your head in shame. Really. Jim could have done better.
```

The route reaches the top band, so the marker is the score line itself,
`You achieved a score of 4.`

## Status

Wired and blessed; `golden = goldens/private_eye_solution.expected.txt`
(1842 lines). PASS.
