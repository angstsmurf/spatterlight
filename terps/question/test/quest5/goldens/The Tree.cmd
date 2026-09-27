# The Tree (Father Thyme, 2014/2023 v2.0, ASL 580) — winning script
# RESULT: state=Finished, errors=0 — full ending: sit in the Aries control chair
#   carrying the dog Cassie -> distress call -> Sector Marshal Travis holo ->
#   "I shall return, educate people then put it to the vote." -> THE END, finish.
# SOURCE: none — no published walkthrough exists for this game anywhere in the
#   corpus. This script was derived from the game source (game.aslx inside the
#   .quest zip). The win condition is the Control chair's sit script, gated on
#   Got(Dog1) (a registered crew member: the dog Cassie from the small capsule).
# Solution chain, in brief:
#   - Crater: Outside theShip, then leave + return (onexit reveals the dog) and
#     SPEAK TO DOG -> it runs inside, unlocking the ship. On the Looksee, PUSH
#     ROCK (Thu Hand Rock) reveals the metal spike; take it.
#   - Wooden ship: CLOSE CURTAIN drops the Holy Gun; bedroom: X LIEUTENANT
#     (dead) reveals his robe; TAKE ROBE (worn, reveals the false hand); PUT
#     SPIKE IN HOLE -> hologram tube pops out, Crusher Bayliss blocks the
#     airlock; X TUBE plays Captain Kaminski's 4-part holcast (backstory: the
#     Aries, the ATV, the induction fluid, the password hint "the name of his
#     beloved"). SHOOT CRUSHER with the Holy Gun (the game's one kill; Merciful
#     cruelty, no death possible), TAKE TAG (dog picture, '-ASSIE').
#   - Leaving Outside theShip with Crusher dead drops a live branch into the tea
#     bushes (X BUSHES, TAKE BRANCH — a personal oxygen supply).
#   - With tag+spike+branch, the shimmer beetle appears On the Looksee; TAKE
#     BEETLE tumbles the player outside the crater (the only way out). Cave:
#     TAKE TURTLE SHELL shakes out the finger bone (Andre du Pont's DNA); take
#     bone + metal flask (his replicated induction fluid). Both are required to
#     reach By the dot.
#   - ATV: USE BONE ON BUTTON (DNA door), SIT ON SEATS reveals the Updater
#     headphones; TAKE + WEAR HEADPHONES educates the savage (player.Clever,
#     nested SetTimeout education scene — needs the oracle's DrainTimers).
#     X DASHBOARD (hidechildren) then PUT SPIKE IN CONTROL PORT (activator) and
#     PUT FLASK IN INPUT ORIFICE -> the ATV drives itself to the Aries.
#   - Aries: CLIMB RUNGS, SAY CASSIE (password from the tag) to enter. Bunk
#     room lockers hold the picture of Cassie with '4976' on the back; in the
#     Suspended Animation Centre, X CAPSULES reveals the small capsule and SAY
#     4976 opens its audio lock, waking the dog. TAKE DOG, then bridge, SIT ON
#     CONTROL CHAIR -> win (nested wait{} ending, auto-continued).
# Flavor beats included (all verified non-state-breaking): Dolly and the lost
#   shimmer-beetle earring in the morgue (motivates the beetle grab), the
#   pre-Clever ghost holcast, DNA-denied button press, bio chef meal (feeds the
#   dog), first toilet/shower. answer:no declines the sound-effects prompt.
answer:no
x table
x bed
out
n
x tree
ne
s
in
speak to dolly
ask dolly about beetle
out
n
speak to dog
w
u
push rock
take spike
d
e
in
in
close curtain
take gun
e
x bed
x lieutenant
take robe
x hand
put spike in hole
x tube
w
out
shoot crusher
x crusher
take tag
x tag
out
sw
w
x bushes
take branch
ne
u
x beetle
take beetle
n
e
x turtle shell
take turtle shell
take bone
take flask
w
n
x cart
up
press button
use bone on button
sit on seats
take headphones
wear headphones
x dashboard
put spike in control port
put flask in input orifice
out
d
climb rungs
press button
say cassie
in
x droid
s
x lockers
take picture
x picture
w
x capsules
x small capsule
x locking device
say 4976
x dog
take dog
out
n
n
use bio chef unit
w
use toilet
use shower
out
s
e
x control chair
sit on control chair
