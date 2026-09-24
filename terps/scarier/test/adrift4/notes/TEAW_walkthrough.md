# TEAW (introcomp) — walkthrough (**NO WIN CONDITION, 4 moves**)

- **Full title:** *To End All Wars*, by Duncan Bowsman. "Intro Version: 31
  May 2009" -- an entry for IntroComp, a competition for the opening of an
  unfinished larger game.
- **Engine:** ADRIFT 4.00. One room (a WWI front-line trench, "SUICIDE
  DITCH"), two NPCs (Private Smythe, Corporal Butcher), 44 objects, a large
  scripted `TRENCH_TURNS` event driving a fixed multi-turn sequence.
- **Result:** **No win condition exists.** This is only the intro/demo
  segment of a planned larger game; the scripted sequence always ends with
  the player's death by poison gas exactly four turns after the name prompt,
  regardless of what the player does. Wired as
  `teaw_solution.txt|TEAW_(introcomp).taf|YOU ARE GOING TO DIE|`, no env.
- **Source:** self-derived; there is no external walkthrough for an
  IntroComp intro fragment with no winnable state.

## The game

```
[blank -- press enter past the title quote]
[blank -- press enter past the content warning]
Player
wait
wait
wait
wait
```

Confirmed empirically that the outcome does not depend on player action:
standing on the fire-step, trying to calm/assist Private Smythe, and simply
waiting all converge on the identical scripted death -- an artillery barrage
kills Smythe, a lull follows, then a gas attack kills the player because
their gas mask is "GONE" (donning the small box respirator preemptively
changes nothing; the game reports it as already worn and kills the player
anyway). The barrage/gas sequence is a fixed narrative cutscene, not a
puzzle.

## Content note

The game's own splash screen warns of "scenes of strong, graphic violence &
some coarse language" and recommends against play by children -- this is an
audience-appropriateness warning, not a claim that any character in the game
is a minor. All named characters (the player, Private Smythe, Corporal
Butcher) are adult WWI soldiers. Per the no-minors content policy, dark,
non-sexual violent themes wire normally; this game contains no sexual
content. Clean to wire.
