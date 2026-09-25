# Hammurabi — walkthrough (**WIN**)

- **Game:** `hammurabi.taf`, ADRIFT 4.00, Ron Moore, after the Rick Merrill /
  David Ahl BASIC *Hamurabi*. It is played through menu codes. Each year you
  choose `1a`–`1e` (buy 0/25/50/75/100 acres), `2a`–`2e` (sell the same
  amounts) and `3a`–`3e` (guard allotment of 0/1/2/3/5 bushels per citizen).
  Then the year report runs: feeding, harvest (`rand(24,72)`), rats, settlers,
  and guard raids.
- **Result:** win at the end of year 10. TASK 35 (people > 200 AND acres >=
  1250) prints *"Congratulations, Hammurabi! You have proven to be an able
  ruler. Your efficiency rating is 231."* 30 commands. The only loss is
  impeachment (TASK 26, `ACT type=6 v1=2`, when more than a third of the people
  go hungry).
- **Score:** MaxScore 0, no `ACT type=4`. The "efficiency rating" is
  `people + (acres-1000)/4 - year*10`. It is flavour text, not a game score.
- **Env:** none. The route depends on the RNG seed. It was found under
  `SCR_RNG=xoshiro` (the harness default), and **any** change to the RNG stream
  voids it.

## How it was found

The harness RNG is deterministic, so a beam search played every menu
combination year by year through the harness. It kept the best states by
people, acres and grain, with a penalty for a grain buffer too thin to survive a
bad harvest. Population grows only about 10% a year, so no year-9 win turned up.
Year 9 peaked at 195 people and 1299 acres. The first winning line wins in year
10. The final year was then swept over all 25 buy × guard combinations.
`1e 2a 3b` rates 231, against 190 for the beam's `1a 2a 3a`: 100 acres are
bought, and any guard allotment above 0 draws a raid that takes 65 acres from the
barbarians.

## Route (buy / sell / guard, per year)

```
1 1c 2a 3b    6 1e 2a 3a
2 1b 2a 3e    7 1c 2a 3a
3 1b 2a 3a    8 1a 2e 3b
4 1a 2c 3e    9 1a 2a 3a
5 1c 2a 3a   10 1e 2a 3b
```

## Win marker

`Congratulations, Hammurabi!`
