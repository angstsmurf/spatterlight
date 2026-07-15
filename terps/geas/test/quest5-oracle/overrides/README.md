# Curated winning override scripts

Each `<game>.cmd` here is a hand-authored command script that drives its game to a
genuine ending, for a game whose *raw* walkthrough does not reach the end as-is.
When `overrides/<game>.cmd` exists, `run_corpus.sh` uses it verbatim (including any
title-screen preamble baked into line 1) instead of running `extract_walkthrough.py`
on the walkthrough. Every override starts with a `#`-comment header documenting
exactly how it deviates from the raw walkthrough — the goal is "faithful to the
walkthrough's *intent*, corrected only where the walkthrough or the extractor could
not reach the win."

Why each game needs an override (all verified against the game source, seed 1234):

| game | state | reason |
|---|---|---|
| Dream Pieces | Finished | walkthrough's last step `unlock door` is rejected ("Try unlocking the lock."); the real verb is `unlock lock`. `new game` preamble baked in. |
| Jacqueline, Jungle Queen! | Finished | two mutually-exclusive endings (running both corrupts state); keep only Ending 2 "Radio for Help". Also fixes the `pirahna`→`piranha` typo that silently broke the swim ability, and one nav step vs the release map. |
| Guttersnipe- Carnival of Regrets | Finished | two walkthrough nav errors vs the actual room map (`w`→`e` at the Strong Man Tent; insert `in` before a spook-house descent). One early wrong turn cascaded through the whole tail. |
| Escape From the Mechanical Bathhouse | Finished | RNG-/timing-specific solve: the mechanical man wanders on a seeded turnscript (`z` exactly 7× to catch him) and the 5 puzzle symbols are seed-1234 draws; insert the opposite face each time. The 50s limit is real wall-clock, which the headless oracle never advances (see main README, DrainTimers), so turns are unlimited. |
| The Mouse Who Woke Up For Christmas | Finished | needs the oracle's `DrainTimers` (a `<dark>` room's reveal is on `SetTimeout(2)`; without firing timers the room stays black and the game is unwinnable). Script drops the walkthrough's stray `Continue` lines and a `put bowl in moonbeam` the walkthrough itself flags as wrong. |
| The Bony King of Nowhere | Finished (ERR=31) | fixes a `.s` extraction/typo artifact and adds `z` waits for a `SetTimeout(5)` guard change. Reaches the genuine ending + credits; the 31 errors are a QuestViva display-recursion bug on the mandatory `attack general`→prison POV swap and do NOT trip the real breaker (`_scriptErrorsFatal` stays false — see main README). |
| I Contain Multitudes | **Running** | best-effort: drives cleanly (0 errors) to the final story beat ("I am Chandra Fitz…"). The ending `finish` sits behind a nested `wait{…wait{…SetTimeout(7)…}}` continuation inside a menu response that the harness does not pump (the inner wait defers through the on-ready queue and never registers, so the timer is never even created). The game's own author warns its time-based events break Quest's *own* walkthrough runner. |
| Whitefield Academy of Witchcraft | **Wedged** | best-effort: drives cleanly to the *mandatory* Grislewood snare, which double-teleports the player into "Inescapable Cage" — a room reachable only by that teleport that never gets map coordinates. Its enter-script throws ~19 `DictionaryItem(coordinates,"x")` errors in one transition and trips QuestViva's 20-error breaker. In real Quest an enter-script error is non-fatal, so it is winnable there; under this oracle it is a genuine QuestViva-vs-Quest incompatibility, not a walkthrough error. |
