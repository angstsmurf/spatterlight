# HMS Victory (Peter Edwards) — derived winning walkthrough.
#
# No published walkthrough exists for this game anywhere, and it ships no
# <walkthrough> steps, so this script was derived entirely from the game source
# (like The Shack, The Tree and The Zen Garden). wt="-" in corpus.tsv.
#
# Goal (Great Cabin `enter`, game.aslx:827): carry all eight of Nelson's relics
# — Prayer Book, telescope, egg, portrait, glass, carving, medal, sextant — into
# The Great Cabin. The check is eight nested ListContains(ScopeVisible(), …), so
# they need only be held; entering with seven does nothing.
#
# Things the source forces, each of which silently breaks a naive run:
#
# * Exit aliases are NOT bare directions. The rooms use aft/forward/port/
#   starboard, but Core's `go` pattern only matches compass points, up/down and
#   in/out. A bare `aft` is "I don't understand your command", so every move
#   below is spelled `go <alias>`. Only `south` (the pub) and `go up`/`go down`
#   would work unprefixed.
#
# * Four rooms kill you outright if the lantern is not in scope (carpenter's
#   workshop, orlop deck aft, main hold, grand magazine) — `finish` with "you
#   lose all interest in the mission". The lantern (Great Cabin) is the only
#   portable light; every other lantern in the game is scenery. Take it first.
#
# * The Grand Magazine is the inverted case: carrying the lantern in there is a
#   powder explosion (`finish`), but arriving dark is a fatal head-crack. The
#   only survivable state is lantern NOT carried *and* lantern flag "a" set.
#   Flag "a" is set by entering the light room with the lantern (game.aslx:2287)
#   and cleared by entering the narrow passage with it (game.aslx:2512). So the
#   solve is: carry it in via the narrow passage, DROP it in the light room, and
#   walk back out empty-handed — the passage cannot clear the flag if the lantern
#   is not with you. It then burns behind the reinforced window, lighting the
#   magazine from safety. The lantern is retrieved afterwards for the main hold.
#
# * The wardroom is gated by a ghostly Marine sentry who ejects you unless
#   Got(hat) (game.aslx:1578). The lieutenant's hat is loose in the purser's
#   store, which is only reachable through orlop deck aft — so the Prayer Book
#   cannot be collected until after that descent.
#
# * Orlop deck aft spawns a starving rat pack on first entry; without the rat
#   poison (Bosun's store shelf) in scope it is Weil's disease and `finish`.
#   The poison is consumed on use, which is why it is fetched before the descent
#   even though the route reaches the pump well the other way round.
#
# * The sick-bay store-room (medal) and Nelson's sea chest (sextant) are locked.
#   `unlock door with key` answers "That doesn't work" — this game has no unlock
#   verb wired up; `open` auto-unlocks when the matching key is carried. The
#   storeroom key is under the Captain's carpet (`lift carpet`, then `x dust`);
#   the gold key is in the pump well, which must first be drained by working the
#   bilge pumps on lower gun deck aft exactly four times.
#
# * The climb to the Fighting Top (telescope) is an invisible exit until the
#   main mast is examined — the game start does MakeExitInvisible(deck to top)
#   and only `x mast`'s firsttime block reveals it.
#
# * Decoys: the Great Cabin holds a "telescope by the window" and a "glass on
#   the table" that are NOT the required `telescope` (Fighting Top) and `glass`
#   (galley cupboard). The route never types those nouns in the Great Cabin.
#
# * The sextant is deliberately collected last: Nelson's night cabin opens off
#   the Great Cabin, so the final `go port` is the winning entry.
#
# Result: 78 steps, state=Finished, errors=0 — the full "THE END" ending.
#
# NOTE: winning this game also required a harness fix. Its ending runs
# `play sound ("Eight bells.wav", true, false)` — synchronous — immediately
# before the closing msg and `finish`. A synchronous play sound parks the script
# on WorldModel._waitTcs and only resumes on FinishWait(), so the headless
# player used to strand the ending forever while still accepting commands. See
# HeadlessPlayer.PlaySoundAsync in Program.cs.
south
go up
# --- Great Cabin: the lantern, the only portable light ---
go up
go aft
take lantern
go forward
# --- Captain's cabin: the storeroom key under the carpet ---
go up
go aft
lift carpet
x dust
take key
go forward
# --- Fighting Top: the telescope (x mast reveals the climb) ---
go forward
x mast
go up
take telescope
go down
# --- Sick bay: the medal (open auto-unlocks with the key) ---
go forward
go down
open door
take medal
# --- Upper gun deck midships forward: the egg in the hen coop ---
go aft
open coop
x straw
take egg
# --- Galley: the Venetian glass in the cupboard ---
go down
x cupboard
take glass
# --- Lower gun deck aft: pump the bilge dry (four strokes) ---
go down
go aft
go aft
use pumps
use pumps
use pumps
use pumps
# --- Carpenter's workshop: the carving on the bench ---
go forward
go forward
go down
x bench
take carving
# --- Bosun's store: rat poison ---
go down
x shelf
take poison
# --- Light room: leave the lantern burning behind the window so it lights
#     the Grand Magazine, and walk back out without it (see header) ---
go port
go forward
drop lantern
go aft
go starboard
# --- Grand Magazine: survivable now ---
go forward
x barrels
take portrait
go aft
# --- Recover the lantern for the main hold ---
go port
go forward
take lantern
go aft
go starboard
# --- Pump well: the gold key in the drained bilge ---
go aft
go aft
x water
take gold key
# --- Orlop deck aft (the poison sees off the rats), then the purser's store ---
go forward
go up
go aft
take hat
# --- Wardroom: the hat gets you past the Marine sentry ---
go forward
go up
go up
go aft
x chair
take prayer book
go forward
# --- Great Cabin holding seven, then Nelson's sextant next door ---
go up
go aft
go starboard
open sea chest
take sextant
# --- Returning with all eight ends the game ---
go port
