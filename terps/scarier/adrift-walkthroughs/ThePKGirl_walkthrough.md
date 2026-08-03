# The PK Girl — walkthrough (**WIN, Katryn 55/60**)

- **Game:** *The PK Girl* by Robert Goodwin (a.k.a. Robert Street), copyright
  2002, 4th release Aug 2006. Graphics by Nanami Nekono, music by Helen
  Trevillion and OYA-G. You park your motorcycle outside Majesty Mall on an
  ordinary evening and end the week storming a research complex in the desert.
- **Engine:** **ADRIFT 4.00** (`xxd -l 16 games/the_pk_girl.taf` →
  `… c2 cf 93 45 3e 61 …`). 116 rooms, **2260 tasks**, 332 events, 29 NPCs,
  187 variables, 1 645 515 bytes — by a wide margin the largest game in this
  corpus, and about eighteen times the task count of a typical entry.
- **Result:** **WIN** on Katryn's ending —
  *"Congratulations! You got Katryn's ending. Your Secret Letter is: E"*.
- **Score: Katryn 55 out of a possible 60**, Laurie 11, in **407 commands**.
- **Harness row:**
  `thepkgirl_solution.txt|the_pk_girl.taf|Your Secret Letter is: E|SCR_SKIP_WAITKEY=1`,
  PASSing golden.
- **Sources:** three partial ones, none of them a scoring route.
  - `downloaded/ThePKGirl_walkthrough.txt` — a 290-line chapter-by-chapter
    command list whose own first line promises only *"a basic ending"*. It is
    the spine of the route below, but every timing-dependent stretch in it is
    bracketed prose rather than commands (`wait (for 15 turns, until
    kidnapping)`, `wait (for 37 turns, while Monika makes dinner)`,
    `[walk around the general vicinity until you find the umbrella peddler]`),
    and it deliberately courts nobody.
  - `downloaded/ThePKGirl_hints.htm` — chapter-by-chapter hints, not a script.
  - `UITests/Supporting Files/Command scripts/The PK Girl command script.txt` —
    an 86-line Spatterlight UI-test recording of the opening through Chapter 2.

  Nothing ships *inside* the game: `hint` answers that hints are not available,
  and the 1.6 MB decompressed source (unpacked with `taftool.py`; the author
  password block decodes to **`icecream`**) has no "walkthrough" or "spoiler"
  string anywhere. Everything below the basic route — the whole scoring
  structure and every point in it — came out of the task table.

## There is no score. There are eight girls.

The game says so itself in the banner: *"Important note — Scoring in this game
works differently than in other ADRIFT games."* Concretely:

* **No task in the game has a `score=` value, and there is no `ACT type=4`
  anywhere.** The usual proof-of-completeness trick — count the add-score
  actions, count the award lines in the golden — does not apply here.
* Instead there are **eight relationship variables**, VAR 158–165:
  `laurie_score`, `cassie_score`, `saffy_score`, `monika_score`,
  `aileen_score`, `katryn_score`, `bengte_score`, `josie_score`. Each is
  reported *"out of a possible 60"* by the `score` command.
* A scoring task sets **`change_score` (VAR 168)** and then redirects to one of
  eight per-girl adder tasks — **2141** josie, 2142 bengte, 2143 katryn, 2144
  aileen, 2145 saffy, 2146 monika, 2147 cassie, **2148** laurie — each of which
  does `<girl>_score = <girl>_score + change_score` and then latches
  `name_of_girl`.

The ending is chosen by TASKs **2211–2218**, evaluated in the fixed order
**Laurie, Cassie, Monika, Saffy, Aileen, Katryn, Bengte, Josie**. Each wants
its girl's score **≥ 40**, its `know_<girl>` flag set, and `name_of_girl` still
empty. **First match wins.**

Two consequences shape the whole route:

1. **Courting two girls at once is actively harmful.** Laurie is tested first,
   so a Laurie score that drifts over 40 would steal the ending away from
   whoever you actually spent the game on. This route deliberately leaves
   Laurie at 11 and every other girl at 0.
2. Each ending prints one letter of a password. The eight letters spell
   **ICECREAM** — which is also the .taf's own author password, recovered
   independently from the "Wild" trailer with `taftool.py`. Katryn's letter is
   the **E**.

The hints document says the threshold is 45. The task table says 40. The task
table wins: this route ends at 55 either way, but the discrepancy is worth
knowing if you are trying to squeak an ending out of a low score.

## The route, chapter by chapter

| Ch. | Commands | Title | What happens |
| --- | --- | --- | --- |
| 1 | 0–52 | An Evening of Incident | Majesty Mall; the kidnapping; **the bar detour** |
| 2 | 53–81 | The Rescue of Cassandra | the warehouse district |
| 3 | 82–154 | Friends and Clues | the apartment, the plaza, the peddler |
| 4 | 155–244 | A Suspicious Encounter | the cafe: Katryn, then Dustin |
| 5 | 245–296 | The Enemy Revealed | the drive into the desert |
| 6 | 297–346 | Missing | the R.O.S.A. complex, the magnets, Chadwick |
| 7 | 347–406 | Psychokinetic Showdown | the silo, Octal, the inhibitor band |

### Where Katryn's 55 points come from

| Ch. | Trigger | Task | + |
| --- | --- | --- | --- |
| 4 | `sit` at her table in the cafe, then the `3` / `1` answers | conv. | 3 |
| 5 | you got yourself out of the warehouse unaided | 1682 | 8 |
| 5 | the `1` answer on the drive into the desert | conv. | 3 |
| 5 | `kiss katryn` in the car | 1688 | 5 |
| 6 | seeing her face on the security-booth monitor | 1911 | 3 |
| 6 | answering `2` then `3` to the monitor | 1234 | 3 |
| 6 | `search chadwick` — you finished him rather than merely disabling him | 1949 | 8 |
| 7 | Katryn clutches you when Dustin goes down | 1999 | 10 |
| 7 | `head butt octal` | 2018 | 5 |
| 7 | `hug katryn` on the silo | 2134 | 2 |
| 7 | `put band on octal` | 2038 | 5 |
| | | | **55** |

Laurie's 11 are incidental: four +2s during the domestic scenes of Chapter 3
and a +3 for going to her aid on the silo. There is no way to reach the
showdown without them, and 11 is comfortably under 40.

## The seven things that had to be re-derived

### 1. The bar detour is worth ten points, three chapters later

In Chapter 1, from the street in front of the bar: `north` / `north` /
`talk to dustin` / `3` / `1` / `south`. That is TASK 443, and all it does is
set `know_dustin`.

Six chapters later, `know_dustin` is what puts Dustin in the R.O.S.A. complex
(TASK 1817), which is what lets you hand him the magnets, which fires the chain
1917 → 1982 → EVENTs 271/272/273 → 1998 *"# Dustin down"* → EVENT 279 → **TASK
1999 "# Katryn clutches you", +10**. Skip the bar and the single largest award
in the game is unreachable.

### 2. …but it also desynchronises the cafe

`know_dustin` inserts an extra Dustin beat into the Chapter 4 cafe scene:
EVENT 125 fires TASK 1245 one turn after Katryn leaves. Because the cafe is a
long run of numbered menu answers, that one extra turn shifts every subsequent
`1`/`2`/`3` by one and the conversation collapses (in an early attempt Katryn
finished the chapter on 3 points instead of 30, with nothing obviously wrong in
the transcript).

The fix is to *spend* the turn deliberately: after Katryn's last `1`, the route
does `wait` — the Dustin scene fires on that turn — then answers `2` (*"Do you
know her?"*), then `talk to dustin`, which is TASK 434 and hands you the magnet
errand.

### 3. The peddler walks, and the whole route is his clock

NPC 26 patrols a long plaza circuit — 68 ↔ 70 ↔ 63 … 69 ↔ 72, roughly nine
turns a leg. `give money to peddler` fails with *"Please be more clear, who do
you want to give to?"* whenever he is not standing where you are, and **every
turn added or removed anywhere earlier in the route changes his phase**. This
row broke twice while the earlier chapters were still being tuned. As
committed, he is in Center Plaza on the turn the route arrives:

```
north / north / east / give money to peddler
ask peddler about silo / ask peddler about valley
west / south / south
```

If you edit anything before Chapter 3, expect to re-find him.

### 4. `x machinery` is a trap in Research Lab C

The magnets are found by **`x equipment`** (TASK 1847, also `search equipment`
and `look behind equipment`). `x machinery` — the word the room description
uses — is swallowed by a generic scenery task that gives a brush-off, and the
lab looks empty.

### 5. The silo fight has a six-turn fuse and a four-turn window

`punch octal` is TASK 2020, gated on task2009 *"# Laurie fights back"*. Try it
early and you get *"You are not close enough to do that"*; the route waits
eleven turns after climbing out of the hatch and punches on the sixth after
Laurie's counter-attack, then head-butts on the next turn.

`head butt octal` (TASK 2018, +5) and `knee octal` (TASK 2019, +5) both
redirect to 2025 *"# Octal runs"* and both require it **not** done, so exactly
one of the two can ever land. They are alternatives, not a sequence.

Then the endgame window is **exactly four turns wide**:

```
398 get band
399 kiss katryn        (refused -- see below)
400 hug katryn         +2
401 put band on octal  +5
```

TASK 2039 *"# Katryn has a solution"* fires on the fifth turn and takes the
band award off the table; a run that puts the band on one turn late finishes on
50 instead of 55. Putting the band on *early* is worse still — it ends the
scene, and the hug can no longer be had at all (probed: 47).

### 6. The refused kiss is not a bug

`kiss katryn` on the silo answers *"I'm not sure she would appreciate that!"*,
and for a while that looked like a missing restriction. It is not: TASK 2135
(`* kiss *katryn *`, +3) is **`rep=0`** and the route already spent it on the
kiss in the car in Chapter 5. The engine trace confirms the task runs to
completion the *first* time and awards its 3 points there.

It is still worth issuing on the silo — it is a free turn inside the four-turn
window, and it has to be spent on something for the hug to land where it does.

### 7. The monitor conversation is easy to walk straight past

When Katryn's face appears on the security-booth monitor, a three-option menu
is printed and it is tempting to ignore it and get on with the level. Answering
**`2` then `3`** is worth +3 (TASK 1234).

The mechanism is worth spelling out because it generalises to every
conversation in the game. Answering option *n* sets
`katryn_talk_state = katryn_talk_state * 3 + n`, so the state encodes the whole
path taken through the tree, and the awards are written as tests on exact
(situation, state) pairs:

```
TASK 1233 (option 2)  change_score = if(situation=7  & state=5, 3, 0)
                      change_score = if(situation=10 & state=2, 3, change_score)
TASK 1234 (option 3)  change_score = if(situation=8  & state=9, 3, 0)
```

The monitor is situation 8; `2` takes state 0 → 2, and `3` takes 2 → 9.

## Why 55 and not 60

Three conversational +3s exist for Katryn. The route takes one of them. The
other two are not reachable behind it:

* **situation 7, state 5** belongs to TASK 1684 *"# Katryn advances"*, a
  room-87 scene that is the **alternative** to the Chapter 5 kiss. The kiss
  pays +5 (TASK 1688); taking the +3 branch instead is a net loss of 2.
* **situation 10, state 2** needs `katryn_done_talking` (VAR 115) back at 0.
  Every conversation task sets it to 1 on the way out and nothing in the task
  table resets it, so once the monitor exchange closes, situation 10 can never
  be entered. Probes at four different turns inside situation 10 all scored 0.

That leaves the head-butt/knee pair (mutually exclusive, +5 either way) and the
disable/finish pair for Chadwick (TASK 1948 +4 vs TASK 1949 +8, the route takes
the 8). **55/60 is the practical ceiling for Katryn**, and the ending it buys
is the same one 40 would have bought.

## Reproducing

```sh
cd terps/scarier/adrift-walkthroughs/harness
sh run_v4_walkthroughs.sh thepkgirl     # PASS against the committed golden
```
