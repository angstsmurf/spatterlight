# The Secret of the Lost World — Walkthrough (3300/3300, WIN)

- **Game:** *The Secret of The Lost World* (ADRIFT 4 `.taf`, `games/SecretOfLostWorld.taf`)
- **Result:** **Won, full 3300/3300 (100%)**, deterministic.
- **Solution:** `harness/secret_of_lost_world_solution.txt` (the first two lines
  answer the start-up prompts: a name — `Hero` — then `female`; see below).
  Verified identical 3× under the seeded headless SCARE build.

A genuine treasure-hunt adventure: shipwrecked on the island of Atlantis, you
recover the gods' treasure, defeat the time-god Kronos, and sail away. 33
scoring tasks at +100 each sum to the maximum 3300; the win is `turn wheel`
aboard the rescued ship.

## Important: the game asks your name and gender at the start

Like many ADRIFT games it first asks *"Please enter your name:"* (type anything;
the solution uses `Hero`). Then the gender prompt below.



The player's gender is stored as **Unknown**, and the original ADRIFT Runner
pops up a *"Please choose player gender"* dialog at game start (Runner form
`Form8`). The choice matters: the castle door's seal-ring lock, and the
end-game rescue, are gated on it (a *male* player rescues the Prince with the
sword; a *female* player rescues the Princess with the ring). Either choice is
fully winnable for 3300.

**SCARE never implemented this prompt**, so before this fix the gender stayed
Unknown forever, *both* lock tasks (T34 male / T35 female) failed, and the
entire castle — and therefore ~3000 of the 3300 points and the win — was
unreachable. This was **not a game-data dead end**: the game is winnable in the
real Runner once you pick a gender. The fix adds the prompt to SCARE (see
"Engine fix" below). This walkthrough answers **`female`** at the prompt and
takes the Princess route.

## Phases

**1. Beach & the Sea Goddess (→300).** Take the shell, knife and scroll. Go
`n e se` to the Rocks; take the pebbles and `cut stones with knife` (+100,
yields a **seal ring** — and wakes a roaming **Ghost**). Carry the ring back
`nw w s s` to the Ocean and `put pebbles in water` (+100) to summon the Sea
Goddess; `give shell to goddess` and she rewards you with **Excalibur**.

**2. Troll, animals, treehouse window (→800).** Collect berries (Forest) and
mushrooms (East Forest), grab the apple (West Forest). `give mushrooms to
troll` (+100) to enter the hut; take the sack, green potion and ladder. `put
ladder against window` (+100) and `open window` (+100) — you jump into the
**Jungle**. `give berries to lioness` (+100) then `give berries to tiger`
(+100); take the dropped **tiger tooth**.

**3. Sun stone & blue gem (→1000).** Out via hut→forest→Crossroads→Right Path:
take the **pocket knife**, go to the **Volcano**, `take lava` then `cut lava
with pocket knife` (+100, reveals the **Sun stone** and opens the throne). At
**Crystal Lake**, `put tooth in skull` on the boat's skeleton (+100, **blue
gem**); take the blue gem and the **decayed scroll**.

**4. The castle & the planets (→2100).** Loop back to the **Castle Entrance**
and `use ring on lock` (the gender-gated seal-ring task). In the **West
Garden** the **Princess** appears: `give princess decayed scroll` to get the
**white lily**. Now place the nine planet stones plus the Sun on the **throne
in solar order** — Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus,
Neptune, Pluto, Sun (each +100). *You can't carry them all (weight limit), so
fetch-and-place as you go* — the towers and yards all ring the central throne
room. The last stone makes the **Queen** appear: `give queen lily` (+100,
**red gem**). (Pick up the **strawberries** in the East Garden on the way.)

**5. The Caves — statue, ghost, chest (→2800).** Down into **The Caves**. Pick
up the **lighted torch** at the Caves Entrance first — the caves are pitch dark
without it. Put the **red, green, blue** gems on the statue (red→green→blue,
+100 each); the rainbow reveals a drawer — `open drawer`, `take key`, then
`use key on gate` (+100). `drink green potion` (+100, +Str/+Acc) **before**
descending to the Rainbow Caves, where the Ghost guards the chest. Kill it fast
(`kill ghost with excalibur` ×2). Then loot the trunk: `open trunk`, `take
potion` (the **red potion** — *keep it for Kronos*), and `open chest` then `get
chest` (+100, the **treasure**, which also drops Kronos into his temple).

**6. Hammer, skeleton, rats (→3000).** Up to the **Treehouse** and `give baby
strawberries` (+100, **hammer**). Back in The Caves, `use hammer on wall` to
open the secret tunnel: it sends the **Princess to the ship**, and puts a
sword-and-golden-ring **skeleton** and the **rats** in the tunnel. `take sword`
and `take golden ring`; up to the rats and `give apple to rats` (+100), which
reveals a **small key**.

**7. Kronos, the small chest, the Goddess (→3300).** Head for the **Temple of
Kronos** via the Volcano. *Kronos one-shots a weakened player on entry*, so
`drink potion` (the saved red potion, +100, +50 stamina) at "To The Temple"
just before you step in, then `use excalibur on kronos` (+100 — a scripted
kill; his 1000 stamina is irrelevant). `open chest with small key` (+100, the
soul chest). Finally, the Sea Goddess has resurfaced at the **Ocean**: `give
excalibur to goddess` (+100) — **3300/3300**.

**8. Win.** Return to the **ship**, `give princess golden ring` (this is what
arms the winning wheel — it must be done *after* returning Excalibur), and
`turn wheel`. The ship sails from Atlantis — **you win**.

## Order gotchas (why the obvious route fails)

- **Gender first.** Without answering the start-up prompt, the castle lock can
  never open. (Pre-fix SCARE made the game unwinnable here.)
- **Torch before the dark caves.** Items can't be taken in an unlit cave; the
  lighted torch is waiting at the Caves Entrance (placed there when you give the
  Goddess the shell).
- **Save the red potion for Kronos.** Beating the Ghost needs only the green
  potion if you attack immediately; spend the red potion's +50 stamina just
  before the Temple or Kronos kills you on entry.
- **Goddess before Princess.** `give ring to princess` (which enables the
  winning `turn wheel`) is gated on having already returned Excalibur to the
  Goddess. Do the Ocean, then the ship.
- **Don't reveal both Prince and Princess.** Turning the wheel *loses* if tasks
  19 + 44 + 45 are all done. The female route never does the Prince's
  hammer-wall (T45), so this is moot — but mixing routes would strand the win.

## Engine fix (committed)

SCARE had no equivalent of the Runner's *"choose player gender"* dialog, so any
ADRIFT game shipping with an Unknown player gender that gates content on
"player is male/female" was unwinnable in Spatterlight. Added
`run_prompt_player_gender()` to `scrunner.c` (asked once at game start, only
when gender is Unknown), backed by a new `prop_put_integer()` in `scprops.c`
that records the answer in the session-persistent property bundle — so the
choice survives save/restore/undo within a session and a fresh load re-asks,
exactly like the Runner. No save-format change. Verified: this game wins
3300/3300 with no other intervention, and already-gendered games (Sun Empire,
Orient Express, …) are unaffected (no prompt).
