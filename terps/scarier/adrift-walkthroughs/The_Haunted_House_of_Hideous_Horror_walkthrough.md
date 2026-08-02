# The Haunted House of Hideous Horror — walkthrough

- **Engine:** ADRIFT 4 (`hauntedhouse.taf`, **1st ADRIFT One-Hour Game
  Competition**, 2002). Desther wants his house back; you are the exorcist,
  and the fee is negotiable.
- **Result:** **WON**, 0/0 — no scoring system anywhere in the file.
- **Solution:** `harness/hauntedhouse_solution.txt` (golden blessed, in
  `run_v4_walkthroughs.sh`). Win marker:
  `you congraulate yourself on a job well done.` (the author's typo, kept
  verbatim).

## Structural verdict

19 tasks, 15 rooms, four ghost NPCs (Walter, Cedrick, Claudia, Desther). **Five
tasks end the game and four of them kill you**: tasks 2, 5, 6 and 7 are
`EndGame(death)` — climbing the stairs a second time, and attacking the stair
ghost, Claudia or Cedrick. Only task 11, `s` at the Entrance, is
`EndGame(win)`, and it is gated on task 10 (`give *comb*`), so the comb is the
last link in the chain.

Character restrictions in the dump are `RESTR type=3` with **`v3` = NPC index +
2**, which is what pins each gift to one specific ghost: `v3=2` is Walter,
`3` is Cedrick, `4` is Claudia.

## Route

```
sw
take knife
open oven
take severed head
ne
u
show head
u
take cleaver
d
sw
w
w
sw
s
give cleaver to walter
n
ne
e
e
ne
u
nw
throw bomb
take key
se
d
sw
s
d
x coffin
open coffin
x coffin
u
n
ne
u
w
give comb to claudia
e
d
s
```

## Notes

- The severed head is in the **oven**. Task 0 (`open *fridge*`) is a pure
  refusal, and the dead body on the kitchen floor is scenery you cannot even
  examine.
- `show head` (task 3) only works at the Entrance and only after one blocked
  `u` (task 1). The stair ghost flees and drops the meat cleaver upstairs.
- `give cleaver` (task 4) must go to **Walter**, the senile ghost in the deep
  woods. Cedrick and Claudia both answer *"doesn't seem interested in the meat
  cleaver"*. He pays in ghostly bomb.
- `throw bomb` (task 8) must happen in **Desther's bedroom**, at Cedrick. Doing
  it in the bathroom is task 9, which throws the bomb away for nothing and
  leaves the game unwinnable. Cedrick's remains leave the key.
- In the cellar the light switch is already stuck *on*; `flip switch` does
  nothing. `x coffin` → `open coffin` (task 14, needs the key) → `x coffin`
  again (task 13) is what produces the comb.
- `give comb to claudia` in the bathroom clears the last ghost, and `s` at the
  Entrance takes the money.
