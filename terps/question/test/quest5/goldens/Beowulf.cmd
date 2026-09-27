# Beowulf -- "Beowulf's Fight with Grendel" (Quest 5.0.4303, Dec 2011) -- WINNING SCRIPT
#
# RESULT: state=Finished, errors=0, 11 steps -- the game's single winning ending
# ("-To Be Continued. ... Congratulations on beating the game."), reached by tearing
# Grendel's arm off in the mead hall at night, exactly as in the poem.
#
# SOURCE: no walkthrough exists for this game anywhere; derived entirely from
# game.aslx. The game is a short linear retelling with many death endings and one
# win, so the derivation was about avoiding the losing branches, not puzzle-solving.
#
# KEY DERIVATION NOTES (all verified against game.aslx):
# * `sail ship` (not `sail`): the Sail VERB dispatches to the ship's <Sail> script,
#   which moves the lowercase `ship` object. The sibling lowercase <sail> script is
#   dead author code referencing a nonexistent capital `Ship` -- reaching it would
#   error. `take ship` also works and hits the same correct branch.
# * The Field->Mead Hall exit refuses you while Got(Sword) (Wulfgar takes weapons at
#   the door). The author's check is buggy: the FIRST north merely destroys the
#   marker object `x`, and every later north then prints "You must drop your
#   weapon(s) before entering" forever. So `drop sword` must come BEFORE the first
#   north, or the hall is unreachable for the rest of the run.
# * `speak to hrothgar` gates on the scenery marker `y` being in scope, which it is
#   on the first visit; answering NO to "Do you still go on?" is an instant GAME OVER.
#   Answering yes hands the sword back and drops you into the Breca swimming-match
#   flashback (the Sea).
# * Sea monster: `use sword on sea monster` is the only survivable attack. Both
#   `swim toward monster` and any bare-hands attack are GAME OVER. The follow-up
#   "Do you hold on?" is answered NO deliberately -- every branch returns you to the
#   Mead Hall at night, but the yes-branch runs a RandomChance(75) roll, and the
#   no-branch is both deterministic AND the better outcome in fiction (you stay in
#   the race instead of losing it).
# * `wait` in the mead hall at night is the trigger that summons Grendel (as are
#   `rest` / `fake sleep` / `pretend to sleep`). Plain `sleep` asks a question whose
#   yes-branch is a GAME OVER, and `run` once Grendel is present is also fatal.
# * `tear off arm` is the win. The other combat options (kick/punch/grab/throw/
#   crush hand) only print flavour and loop back to "Now what?" -- they are safe but
#   never end the fight, so the shortest path skips them.
#
# ENGINE NOTE: this is the first corpus game to use the EXPRESSION form of Ask()
# (`if (Ask("..."))`), which QuestViva awaits mid-expression. The native engine had
# only the `ask` STATEMENT; Interp::ask_provider was added alongside the existing
# menu_provider to close the gap. Native replay is byte-identical to the oracle.
sail ship
west
north
drop sword
north
speak to hrothgar
answer:yes
use sword on sea monster
answer:no
wait
tear off arm
