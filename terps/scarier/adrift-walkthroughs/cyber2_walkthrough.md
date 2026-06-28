# Cyber Warp 2 — walkthrough

- **Engine:** ADRIFT 4. (The "fights" are scripted task kills via the Wave Motion
  Cannon; no ADRIFT Battle System combat must be won.)
- **Result:** **WIN, full 355/355 (100%), deterministic.** Sequel to *Cyber
  Warp* — same hero ("Flame"), now rescuing Lightning from the evil ruler
  Mr. Bissoff. Win screen: *"Congrtulatons, you have beaton Cyber Warp 2!"*
- Solution file: `harness/cyber2_solution.txt` (no name/gender prompts).

## Scope (the 355 maximum, and the two traps)

Dump of 14 tasks. **Positive `ChangeScore` actions (8):** `use teleporter` +50,
`enter warp` +50, `reveal hooded figure` +50, `give electric uniform to
lightning` +50, `shoot chef` +50, `shoot guards` +50, `shoot guard` +50, and
the password `never` +5 → **355 total.** Two **trap** tasks subtract score and
end the game and must be avoided:

- `use teleporter#2` — using the *Teleporter booth's* unit (−50, death `var1=2`).
- `shoot lightning` — shooting your ally instead of arming him (−150, lose
  `var1=1`).

There are **two win endings** (`var1=0`): `feel wrath of flame` (Mr. Bissoff's
Room) and `use transporter` (the Transporter Room). This walkthrough takes the
first; both leave the score at the already-maxed 355.

## Setup objects

- **Electric Uniform** starts in *Your room* (W of the Coffee House) — pick it up
  to give to Lightning later.
- **Wave Motion Cannon** is bought from the Merchant in the *Shop* (N of the
  Coffee House) for your starting *ten bucks*; it's the weapon for all three
  `shoot` tasks.
- **The Teleporter** in the Teleporter Room is openable and must be opened
  before `use teleporter` will fire (its restriction needs it open) — and do
  **not** use the lookalike unit in the Teleporter *booth* (the −50 death trap).

## The win (full 355/355)

```
w                                  <- Your room
take electric uniform
e                                  <- Coffee House
n                                  <- Shop
buy wave motion cannon             <- spends the ten bucks
s                                  <- Coffee House
s                                  <- Teleporter Room
open teleporter                    <- REQUIRED before use
use teleporter                     <- +50; teleports to the BluDeath booth
out                                <- BluDeath Mountains
w                                  <- The Cave (the Cyber Warp)
enter warp                         <- +50; through to Bisstonia
ne                                 <- Back Ally (the "Hooded figure" is here)
reveal hooded figure               <- +50; it's Lightning!
give electric uniform to lightning <- +50; arms him for the final fight
in                                 <- Kitchen
shoot chef                         <- +50 (Wave Motion Cannon)
e                                  <- Dining Area
out                                <- Main street
n                                  <- Large Building
shoot guards                       <- +50; opens the way up
u                                  <- Stairway
u                                  <- Hall (a password door blocks the north)
w                                  <- Guard Room
shoot guard                        <- +50; the dying guard gives the password
e                                  <- Hall
never                              <- +5; "never" is the password (opens the
                                      door and walks you into Mr. Bissoff's Room)
use firebolt                       <- Mr. Bissoff snatched the cannon, so finish
                                      him with your fire powers
feel wrath of flame                <- the win
```

## Notes / gotchas

- **Give the uniform to Lightning the moment you reveal him**, in the Back Ally.
  The give-task is room-independent (`Where = all rooms`), but doing it on the
  spot is simplest and keeps him armed when he reappears in the finale.
- **`never` is both the +5 password and the door key.** Answering it in the Hall
  scores the last 5 points *and* opens (and steps you through) the north door
  into the boss room.
- **The cannon is taken from you on entry** to Mr. Bissoff's Room, which is why
  the kill uses `use firebolt` / `feel wrath of flame` (your innate fire power),
  not the gun.
- **Avoid both traps:** never use the booth teleporter, and never `shoot
  lightning`.
- Like its prequel the win text ends "THE END, or is it?" — but here it is a
  genuine `var1=0` victory, not a lose-stop.
