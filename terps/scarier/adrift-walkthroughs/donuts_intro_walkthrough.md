# Donuts (intro) — walkthrough

- **Engine:** ADRIFT 4. A very short, dark domestic vignette / scenario intro:
  your wife snaps and comes at you with a pot; you flee and hide.
- **Result:** **WIN reached, deterministic. 0/0 (no score)** — the game is a
  pure win/lose intro with no score.
- Solution file: `harness/donuts_intro_solution.txt`.

## Structural verdict & the win

6 tasks, 3 rooms (Den / Kitchen / Master Bedroom). The only win `EndGame`
(`var1=0`) is **`#endScenario1`**, fired when you successfully **hide under the
bed** in the Master Bedroom; the alternative is the `#kill wife` branch (a bad
end). There are no `ChangeScore` actions, so the result is simply reaching the
scripted scenario-end (which cuts to a bar, where it's revealed the wife is
alive and hunting you — the hook for the main game).

```
            <- press Enter at the "«Continue»" prompt
hide        <- react to the attack
e           <- into the Master Bedroom
hide under bed   <- the win ("#endScenario1" → Congratulations!)
```
