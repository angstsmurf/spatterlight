# Tombs & Mummies — Matthew Warner, 2020 (ASL 580)
# No published walkthrough exists — derived from game source (game.aslx).
# Escape King Haputet's tomb: ending = climb the propped ladder out of the
# entry-room hatch ("You escaped!" + finish in above_ground).
# Chain: ignite the sconce torch (a match resets the wick, which burns
# 5%/turn — this script fits in one wick) and carry it for light; the Ark of
# Magic's papyrus teaches `open sesame` (the hidden burial-chamber door) and
# the burial-chamber hieroglyphs teach `appum` ("lighten the heavy, and
# enliven the dead"); the Flail of Anubis threads the sarcophagus latch
# (`lock sarcophagus`) so the appum-animated mummy stays imprisoned (its
# lid-beating is a real-time timer, dormant headless — as are the snake and
# mummy combat timers); appum makes the Horus-shrine ladder light enough to
# carry; prop it in the entry room and climb. TRAPS avoided: the Amulet of
# Disorientation (randomises every exit) and the Snake Statue (body-swaps
# you into the mummy). No RNG on this path.
ignite torch
take torch
e
s
open chest
translate papyrus
n
n
take flail
w
open sesame
w
translate hieroglyphs
lock sarcophagus
say appum
s
s
take ladder
n
e
prop ladder
up
