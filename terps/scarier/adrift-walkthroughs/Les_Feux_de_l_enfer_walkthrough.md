# Les Feux de l'enfer — analysis + partial max-score route

*"Les Feux de l'enfer"* ("The Fires of Hell"), by ? — native **ADRIFT 4.0**
(header byte 8 `0x93`), **French**. A Battle-System dungeon crawl: you return to
your home town of Calah and descend into the demon **Anarazel**'s dungeon in the
Wastes of Chaos to slay him. **289 tasks, 23 NPCs, 36 rooms, 59 exits, 1 event.**
Solution file: `harness/les_feux_solution.txt` (verified deterministic).

Boot: start menu → type `1` (Nouvelle partie), then `intro` or `passer`. The
alchemist offers 3 potions — type `2` for the **fiole bleue** (+24 endurance heal).
Stats: Skill 10, Endurance 13 (max), Luck 12, holding a long sword.

## Headline findings

1. **There is NO win ending** — confirmed structurally (5 `EndGame` actions, all
   `v1=2` death/lose; zero `v1=0` win). The Préface promises slaying Anarazel, but
   the finale (kill the dying demon → `enter lumière`) just teleports you back to
   the entrance with no victory. **Score-only.** (See `WALKTHROUGH_TODO.md`.)

2. **The stored max of 115 is UNREACHABLE — true max ≤ 105.** The room-12
   **disarm-trap +10** (task 116, fired by `disarm piège` = task 115) is an
   **orphaned/unreachable point — a faithful authoring bug**: the crystal key is
   initialised **inside** the trap (container #4 — `get cristal` fires task 112,
   restriction "object in place 39,**4**,5" = inside-container, PASS, verified by
   trace), but *every* disarm-family task (113/114/115/118/119/120) requires the
   key to be **on top of surface #2** (restriction type 5), and **no action
   anywhere in the game ever moves the key onto a surface** (all obj39 move-actions
   use dest-type 4=to-player or 0=to-room/hidden). So the +10 disarm can never
   fire. The real ADRIFT Runner fails identically (same `.taf` data) — this is not
   a SCARE divergence. Same class as the circus/FunHouse orphaned points.

3. **Even 105 is RNG-gated and not reliably deterministic.** The room-19 capo+chef
   (+10 of the +15) spawn from a **one-shot** random roll (var#16 via task 153,
   `rep=0`): you `e` to spawn the guards and the count (1/2/3) is fixed by the
   upstream RNG-draw count, not tunable by neutral turns. Under seed 1234 you get a
   lone garde (+5 only). The rope (room 33, +10) is RNG too but **retryable**
   (`throw grappin` is repeatable, so you re-roll var#17 until outcome 4). And
   **all combat is RNG-stream-position-dependent** — the number of `attack` swings
   to kill each enemy shifts if any upstream turn changes, so the whole sequence
   must be tuned holistically.

## Combat / interaction model (cracked)

- **Verbs are SCARE's built-in ENGLISH library** (the game is French, but SCARE has
  no French Runner library): `attack <enemy>`, `open`/`ouvre`, `get`/`take`,
  `drink`, `push`, `give`, `examine`, `look`. Directions `n/s/e/o` (o = ouest).
  French verbs like `tue`/`attaque`/`bois` are NOT recognised.
- **Combat works, no assist needed:** `attack <enemy>` → "Vous frappez X" →
  "...coup est fatal/en plein cœur" (enemy dies). Some swings do "aucun domage"
  (enemy defence) — repeat until dead. Max endurance is **13**; the fiole bleue
  heals to that cap (drink it mid-fight when low, not early).
- **Triggers:** many enemies are dormant until you act — e.g. the goule (room 8)
  only rises on `look`; the room-6 ogre bursts out on `ouvre coffre`; the petit
  homme (room 10) appears on `open porte`. **Two-step doors:** a `[n]` task may
  grant an item without moving (e.g. room 9 grants a random key), so a *second*
  `n` is needed to actually pass through.
- **Flee = death** only while the enemy is still present (the `flee/fuir`
  EndGame tasks); once the enemy is dead, normal movement is safe.

## Verified deterministic route — 25/115 (5 kills)

`harness/les_feux_solution.txt`, verified 3× identical at 25/115:
`1` → `passer` → `2` (fiole bleue) → `s s s` to room 6 → `ouvre coffre` →
`attack ogre`×4 (**+5**) → `o` `n` to room 8 → `look` (wakes goule) →
`attack goule` (**+5**) → `n` to room 9 → `look` → `attack demon`×~12 (**+5**) →
`open porte` → `n` (grants skeleton key) → `drink fiole bleue` → `n` to room 10 →
`open porte` (spawns petit homme) → `examine petit homme` → `1` (diplomatic — NOT
`3`, whose task-105 chain eats the voleur points) → `attack voyou` (**+5**) →
`attack voleur` (**+5**) → `ne` `n` to room 12 → `get cristal` (crystal key, +2
stat boost). **Score 25/115.**

## Remaining scoring events (analysed; methods known)

| Pts | Room | Task | Method | Gating |
|----|----|----|----|----|
| +5 (of 15) | 19 | killgarde | `e` spawns guards, `attack garde` | reliable |
| +10 | 19 | killcapo/chef | as above | **one-shot RNG (var16==5,6) — not tunable** |
| +10 | 21 | vieillardgive | `give <scroll> vieillard` (illusion/papyrus/longévité) → ivory ring | needs a scroll item |
| +5 | 28 | killtarator | `attack tarator` | reliable combat |
| +5 | 17 | push buttons | `push left`, `push central`, `push right` (in order) | reliable |
| +10 | 31 | give potions femme | `give potion femme` ×2 → **magic mirror** | needs healing potions |
| +5 | 32 | assassin_kill | `attack assassin` | reliable combat |
| +5 | 32 | examine objet | `examine objet` → **grappin** | reliable |
| +10 | 33 | randomcorde4 | `throw grappin on rocher` (repeat until outcome 4) | RNG but **retryable** |
| +5 | 34 | note mi do | `jouer note mi do` after the note sequence | needs ivory flute |
| +10 | 8 | Hors d'ici | `Hors d'ici` in room 8 (needs the magic mirror) — do LAST | needs mirror |
| ~~+10~~ | ~~12~~ | ~~disarm~~ | **UNREACHABLE ORPHAN** (see finding 2) | — |

Item web: descend (rooms 6→8→9→10→12→13→19→21→22-28) collecting the crystal &
skeleton keys, a scroll (for the vieillard), healing potions (for the femme, who
then gives the **magic mirror**), the **grappin** (room 32, opens the room-33 rope
puzzle → room 30), and the **ivory flute** (for the room-34 note lock). The magic
mirror is the late-game key: it enables `Hors d'ici` (+10) back at room 8.

**Reliably-deterministic ceiling ≈ 85–95** (all non-RNG points); **≤ 105** only if
the one-shot room-19 capo/chef RNG happens to align. **115 is impossible** (disarm
orphan). Completing the full holistic RNG-tuned route is a multi-session job.
