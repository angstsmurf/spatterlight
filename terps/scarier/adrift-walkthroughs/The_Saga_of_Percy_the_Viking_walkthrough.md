# The Saga of Percy the Viking — walkthrough

- **Engine:** ADRIFT 4 (`Percy.taf`, **2nd ADRIFT One-Hour Game Competition**,
  2003). Not a parser game at all: a five-stage choose-a-number CYOA driven
  entirely by variables.
- **Result:** **best reachable ending** — *"You're a prince among vikings!"*
  There is no scoring system and no `EndGame` action anywhere in the file.
- **Solution:** `harness/percy_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker: `prince among vikings`.

## Structural verdict

50 tasks, 2 rooms (*Mead Hall* and *End*), one NPC (Throgbert). The eight
"— final …" tasks 40–47 print one of eight ranks and teleport you to room 1;
none of them carries `EndGame`, so the session simply sits in *End* until you
quit. "Maximum" here means the best rank the arithmetic allows.

## Route

```
z
z
1
1
2
1
1
```

## Notes

- **Turns 1–2 are load-bearing.** Throgbert has to walk in before anything
  parses, and `EVENT 0` puts him in the hall at the end of turn 2 *if you simply
  wait*. Typing at him earlier earns *"no idea how to do that"*, and those
  random unrecognised-command messages perturb the RNG stream enough to delay
  his entrance to turn 7. Hence two plain `z`s. After that every stage's options
  print in the same turn you answer the previous one, so the five digits run
  back to back.
- Stage effects, read off the `ChangeVariable` actions. Note the dump's
  restriction variable index is the action's index **+ 2**, so action `v1=0` is
  *men* and `v1=1` is *coins*:

  | stage | choice taken | task | effect |
  |---|---|---|---|
  | 1 | Erinbor | 2 | −5 men, +20 coins (alternatives: −1/+10, −10/+1, 0/0) |
  | 2 | attack everyone | 10 | −10 men, +30 coins (alternatives: −5/+15, 0/+10, 0/0) |
  | 3 | let her pass | 19 | no change (kidnapping Yasmina costs 5 men) |
  | 4 | raid Hell Cove | 22 | −5 men, +30 coins; needs men > 14 |
  | 5 | pay 125 | 29 | needs coins > 124; sets *ogwald*, and task 36 then grants the bodyguards flag and +5 men |

  Starting purse is 30 men and 50 coins, so the run ends on 15 men and 5 coins.

## Unreachable by arithmetic — the "king" ending

Task 40, *"king among vikings"*, wants the **bodyguards flag AND men > 19**.
The flag exists only on the 125-coin bribe, and 125 coins is only affordable by
taking the maximum haul at all three paying stages (50 + 20 + 30 + 30 = 130;
any downgrade lands on 120 or less). Those three hauls cost 5 + 10 + 5 = 20
men, leaving 10, and the bodyguards add 5 for 15 — **five short, on the only
route that can pay**. Every other branch either loses Ogwald or buys him a
worse rescue, so *prince* is the ceiling.
