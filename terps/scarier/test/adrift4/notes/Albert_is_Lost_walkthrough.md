# Albert is Lost! An Adventure in Real Life — walkthrough (**WIN, no score**)

- **Author:** Nathan Simpson (2003). You are Tiberius, a boy at a small county
  fair, looking for his dog Albert. Four rooms — the fair grounds with a Sketch
  Artist, a booth area east with the Fortune Teller and Grumpy Vendor, a hill
  south with Rhymin' Simon, and the change-machine corner.
- **Engine:** **ADRIFT 4.00** (`xxd -p -l 12 "games/Albert is Lost! An Adventure in Real Life.taf" | cut -c17-22`
  → `93453e`).
- **Result:** **WON** — "And so she drew them, and Tiberius and Albert went home
  happily, with many tales to tell of their exciting adventures that day. / THE
  END". The game has **no score at all**, so the suite marker is that closing
  line. Wired as
  `albert_is_lost_solution.txt|Albert is Lost! An Adventure in Real Life.taf|Tiberius and Albert went home happily|SCR_SKIP_WAITKEY=1`.
- **Source: none external.** Derived from the game's own hint menu.

## Where the route came from

`SCR_DUMP_TASKS=1` dumps three hint questions, all three tiers filled:

| HINTQ | HINT2 |
| --- | --- |
| Where is Albert? | One of the fair workers is not what he seems. |
| How do I find the silver key? | Something at the fair has changed. Ask the workers what is strange. |
| How do I use the magic word? | Only a true wizard will teach it. Prove yourself with money first. |

Those three sentences are the whole game, but they deliberately do not say
*which* worker or *what* changed — because both are drawn at runtime.

## Why this route is not transplantable

The game rolls **two** facts off the RNG as it runs, not at load:

1. **Which of the four scenery objects has changed** — walnut tree, buskin'
   bucket, vendor's trailer, or the stalls — and therefore which one hides the
   silver key. Exactly one worker will report it when asked `about strange`; the
   others shrug.
2. **Which worker is the real wizard.** Give the quarter motherload to the wrong
   one and they hand back a *false* magic word; saying it dooms Albert. The
   Fortune Teller's own advice is to vet them first with `ask X about wizards`.

Because the engine's RNG stream advances per turn, **inserting or deleting a
single command re-rolls both**. The first assembled solution failed exactly this
way — `unlock change machine with silver key` answered "Tiberius lacketh the
proper device" after a couple of extra flavour turns were added, because the key
had moved to a different object. The file was rebuilt by fixing the command
prefix first and extending it one line at a time, re-running the whole file each
time. **Do not reorder or pad `albert_is_lost_solution.txt`.**

Under the committed turn sequence the draws land as: **the Sketch Artist reports
the walnut tree**, and **Rhymin' Simon is the true wizard**.

## The tells

- The impostor wizard *sneers* — "wizards are dumb", roughly — while the real one
  answers with respect. The tell is attitude, not knowledge.
- The true magic word is always **LOOKFROTHO** regardless of who teaches it; only
  the false words vary.
- Simon is himself the transformed Albert, so saying the word on his own hill
  does nothing. He has to be back at the booth area (he hides behind the bush
  there) when you say it — hence the `n` before `lookfrotho`.

## The route

`goldens/albert_is_lost_solution.txt`, 30 lines.

```
x me / i                              you have a pouch
ask artist about albert               the "have you seen my dog" opener
e / x teller / open pouch / get quarter
put quarter in coin slot              pays the Fortune Teller
ask teller about strange              she names the hint, not the object
w / ask artist about strange          <- the Artist has it: the walnut tree
s / ask simon about strange
e / ask grumpy about strange          (both blanks, kept for the RNG shape)
w / n / x tree                        silver key found in the walnut tree
e / unlock change machine with silver key
x change machine / get motherload     a pile of quarters
ask teller about wizards              she is not the one
w / s / give motherload to simon      pay the real wizard
ask simon about wizards / ask simon about magic
n                                     back to the booths, where Simon now hides
lookfrotho                            Albert restored
ask artist about business             she draws boy and dog -- THE END
```

## Env

`SCR_SKIP_WAITKEY=1` is required: the two-page intro ends on keypress prompts
that would otherwise swallow the first solution lines.
