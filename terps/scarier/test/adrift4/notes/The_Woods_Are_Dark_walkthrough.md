# The Woods Are Dark — walkthrough (**WIN, 100/100 — full score**)

- **Game:** *The Woods Are Dark* by **Cannibal**, 2003 (`thewoods.taf`,
  71,216 bytes). `games.manifest.tsv`,
  `https://ifarchive.org/if-archive/games/adrift/WoodsAreDark.zip`
  (`Cannibal - The Woods Are Dark.taf` inside the zip).
- **Engine:** **ADRIFT 3.90** (`SCR_DUMP_TASKS=1` prints `GAME version=390`).
- **Size:** 23 rooms, 82 tasks, 18 objects, **10 variables, 0 events**.
- **Result:** **WIN** through T48 `take head` in the Graves (`ACT type=6 v1=0`).
- **Score: 100 out of a declared 100** — the file's 21 `ACT type=4` awards sum
  to exactly the maximum, and every one of them is on the critical path.
- **Harness row:**
  `thewoods_solution.txt|thewoods.taf|You scored 100 out of the maximum 100!|SCR_SKIP_WAITKEY=1`,
  **73 input lines**, PASSing golden. One `<waitkey>` fires in the title text
  before the menu; it is the only one on the route, and no fixed seed or assist
  is needed anywhere.
- **Source:** none. No published walkthrough (Key & Compass, IF Archive, CASA,
  IFDB); the whole route was derived from `SCR_DUMP_TASKS` / `SCR_DUMP_OBJLOC`
  and confirmed live.

You have driven to **Black Hill** looking for your friends Stephen and
Catherine, who went into the woods and did not come out. The cottage where the
**Doherty** family — John, Cheryl, ten-year-old Melissa and her brother Drew —
were murdered five years ago is still standing, and the game is the single
night you spend inside it. Each ghost you satisfy gives you one more piece of
what really happened, until the picture on the bedroom wall sends you out to
the graves and the man with the machete.

## The structure: one chain, no clocks

There are **no events and no NPCs on a timer**. The whole game is a dependency
chain enforced by ten variables, of which seven do the gating:

| var | set by | gates |
|-----|--------|-------|
| `cat` = 1 | T16 `unlock window` | T39 (feeding needs `cat > 0`) |
| `cat` = 2 | T39 `feed cat` | T49 in the Toilet |
| `trunk` = 1 | T21 `lift trunk` | itself — T21 needs `trunk == 0` |
| `hearth` = 1 | T24 `make fire` | T25 |
| `hearth` = 2 | T25 `sit chair` | T28, the diary |
| `hook` = 1 | T41 `hook broom` | T40, and the `u` into the Attic (T80) |
| `attic` = 1 | T40 `look at trap` | T40 itself, and three flavour tasks — *not* the `u` |
| `melissa` = 1 | T31 the dolls house | itself — T31 needs `melissa == 0` |
| `drew` = 1 | T23 `sing song` | T32, `s` from the Landing into the Master Bedroom |

`attic` is the odd one out: it does not gate the way its name suggests. T40
`look at trap` is its only writer, and its four readers are all `attic == 0` —
T40's own one-shot guard plus the three End-of-Landing flavour tasks (`close
trap`, `stand`, `look at ceiling`) that stop making sense once the ladder is
down. The `u` into the Attic (T80) gates on `hook == 1`, not on `attic`, so
skipping the trapdoor scene costs the +5 and nothing else — the loft is open to
anyone holding the hooked broom.

Because nothing expires, there is no wrong order that loses the game — only
orders that stall. Two steps are genuinely order-sensitive rather than merely
dependent, and both are the same shape (a task that gates on its own variable
being still zero):

- **`lift trunk` before anything else touches the trunk.** T21 needs
  `trunk == 0`. Its payload is the "tiny writing" smudge behind the trunk,
  which is not read until T22, forty moves later, so it is easy to file the
  trunk under "already dealt with" after `open trunk` and never come back.
- **The dolls house.** T31 needs `melissa == 0` and sets it to 1, so the
  figures have to go in on the first visit that has them.

## Map

| # | Room | Exits |
|---|------|-------|
| 0 | Rutted Road | N → 17, E → 1 |
| 1 | Lark Fall Bridge | — off the route |
| 2 | Outside Cottage | S → 17, NW → 3, IN → 6 |
| 3 | Side of Cottage | NE → 4, SE → 2 |
| 4 | Back Yard | E → 18, SW → 3, IN → 5 |
| 5 | Kitchen | E → 6, OUT → 4 |
| 6 | Front Room | N → 7, W → 5, OUT → 2 |
| 7 | Hallway | E → 8, S → 6, U → 9 |
| 8 | Toilet | W → 7 |
| 9 | Top of Stairs | N → 12, E → 10, W → 11, D → 7 |
| 10 | Landing | N → 13, E → 19, S → 15, W → 9 |
| 11 | Bathroom | E → 9 |
| 12 | Drew's Bedroom | S → 9 |
| 13 | Melissa's Bedroom | S → 10 |
| 14 | Attic | D → 19 |
| 15 | Master Bedroom | N → 10 |
| 17 | Dirt Road | N → 2, S → 0 |
| 18 | Outhouse | W → 4 |
| 19 | End of Landing | W → 10, U → 14 |
| 20 | Clearing | — no exits; T45 `[*]` forwards to 21 |
| 21 | Graves | — no exits; the game ends here |

The cottage is the whole map: two roads in, a yard with an outhouse, four rooms
downstairs and seven up. Three connections are conditional rather than
geographical — `s` from the Landing into the Master Bedroom is T32 and needs
`drew == 1`, `u` from the End of Landing is T80 and needs the hooked broom, and
the Clearing/Graves pair is reachable only by hanging the picture.

Two more rooms are off the map. Room 22 [The Woods Are Dark] is the title screen
the game starts you in — `3` at its menu declines the intro, and T3 there is the
`[*]` that walks you out onto the Rutted Road. Room 16 [Dead Location] is the
usual ADRIFT bit bucket that consumed objects are moved to.

## The awards

| task | pts | command | where |
|------|-----|---------|-------|
| T12 | +5 | `look at metal` | Outhouse — the paint brush |
| T13 | +2 | `look at oil` | Outhouse — the duct tape |
| T16 | +5 | `unlock window` | Drew's Bedroom — lets the cat in |
| T21 | +5 | `lift trunk` | Drew's Bedroom — the tiny writing |
| T19 | +3 | `open trunk` | Drew's Bedroom — the firewood |
| T49 | +5 | `look at walls` | Toilet — the lighter in the cubbyhole |
| T24 | +5 | `make fire` | Front Room |
| T25 | +5 | `sit chair` | Front Room |
| T28 | +5 | `look at fireplace` | Front Room — the half burnt diary |
| T41 | +5 | `hook broom` | anywhere — hook + handle + tape |
| T40 | +5 | `look at trap` | End of Landing — the loft ladder |
| T43 | +5 | `look at crate` | Attic — the framed picture |
| T44 | +5 | `look at suitcase` | Attic — the dolls house figures |
| T31 | +5 | `put figures in dollshouse` | Melissa's Bedroom — the ball |
| T11 | +5 | `look at diary` | anywhere |
| T10 | +5 | `bounce ball` | anywhere — Melissa appears |
| T50 | +5 | `paint pram` | anywhere — the looking glass |
| T22 | +5 | `look at writing` | Drew's Bedroom |
| T23 | +5 | `sing song` | anywhere — Drew appears |
| T52 | +5 | `hang picture` | Master Bedroom |
| T48 | +5 | `take head` | Graves — and the game ends |

19 × 5 + 3 + 2 = **100**.

## Three things that cost time to find

**`unlock window` is T16, not T51.** Both exist. T51 `* lock * window *` has
`* unlock * window *` as an ALTCMD and would swallow the command — except that
its `WHERE_ROOMS` list deliberately omits room 12, so in Drew's Bedroom the
command falls through to T16, whose own ALTCMD[1] is the same pattern. The
scoring version only exists in the one room where the cat is sitting outside.

**`bounce ball` moves you.** T10's actions include `ACT type=1` to the Back
Yard: the text has you playing against the back wall while Melissa walks up
behind you. Every route note that plans the return trip from Melissa's Bedroom
is wrong; the walk back to the Bathroom starts downstairs.

**The Clearing eats a turn.** T52 `hang picture` drops you in room 20
[Clearing], and T45 there is `[*]` with no restrictions — *any* command is
consumed by the forwarding into the Graves. The solution spends a bare `look`
on it. Typing `take head` in the Clearing does not win; it just walks you to
the graves and leaves the head for next turn.

## Ending

Tanner — the man the town let take the blame, and the man who actually did it —
is waiting at the open graves with a machete and the shotgun on the ground
between you. `take head` throws Melissa's father's severed head at him and you
get to the gun first. The epilogue puts the narrator in a room with bars on the
window, being looked after by priests, still insisting it was all real, and
signs off with *"The Woods Are Dark II...available 2003"* — a sequel that, as
far as the archive knows, never appeared.
