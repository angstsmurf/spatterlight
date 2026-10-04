# Rogues Adventure (Four Suits Co, v580): full playthrough to the poker finale (`finish`, 153 script lines). Needs `#!clock=5` (real-time timers drive the DoomQuest endings); the first two lines are the username and password prompts (`alex`, `hunter2`).
# Five keys: Taxidermy Room `x pedestal` + `ermine` -> Obsidian; Arboretum `x letter`, `take acer rubrum`, `plant acer rubrum` -> Emerald; Smoking Lounge `move chess pieces` + `1. Qh8 Kh5 2. Bf5#` -> Ruby; Water Closet `wash hands with soap` -> Sapphire; Banquet Hall `x note`, `yes`, `the stowaway` -> Crystal. Ballroom `x north door`, `yes`, `Crystal Ruby Obsidian Sapphire Emerald` unlocks the Music Room; `play piano` + `C G E` opens the basement stairs; the vault service panel takes `11100`.
# Basement computers (menus = plain numbers in DISPLAYED order, not sorted): Red = DoomQuest II river crossing (spirit over, Alifain over, spirit back, totem, gems, spirit; `1` speed, `1` share, `2` exit); Green = DoomQuest VII (`1` care, `2` accept; any ending works); filler `i` commands let the real-time DQ timers return you to the basement. Blue = DoomQuest IV via the real Konami route: `x blue computer`, `yes`, `wwssadadba` -> void glitch -> grav/light/audio/rng/mem/object system checks -> `reboot` (a yes/no sequence of `1`s with `i` fillers) into Flamingo Hallway 1. KNOWN EXPECTED NATIVE DIFF: the `void` room's `msg` is a malformed source dump; the oracle reports it as an in-game script error (`Invalid token in expression at position (1:1)`, errors=1) while the native engine prints it as text, so `run_replays.sh` shows a diff on this row (see harness/oracle/README.md).
# Hallways south x3, east x4, north to the Rooftop talk: answer `deception` (opens the poker seat). Rogues Village (south, south from the rooftop): `sit down`, pot odds `3` (2:1), `2` yours, river odds `3` (20%), wait out the long pause with `i`s, `1` call -> 'Achievement Code ... 246603' and ~Fin~.
#!clock=5
alex
hunter2
look
e
x pedestal
ermine
take obsidian key
w
w
x letter
take acer rubrum
plant acer rubrum
take emerald key
e
n
w
move chess pieces
1. Qh8 Kh5 2. Bf5#
take ruby key
e
d
wash hands with soap
take sapphire key
up
e
x note
yes
the stowaway
take crystal key
w
x north door
yes
Crystal Ruby Obsidian Sapphire Emerald
n
play piano
C G E
d
x service panel
yes
11100
s
x red computer
yes
1
1
take spirit
n
drop spirit
s
take alifain
n
drop alifain
take spirit
s
drop spirit
take totem
n
drop totem
s
take gems
n
drop gems
s
take spirit
n
2
x green computer
yes
1
2
i
i
x blue computer
yes
wwssadadba
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
i
1
i
i
1
i
i
1
1
1
1
i
i
i
1
i
i
1
1
1
1
i
i
1
2
1
1
i
i
1
i
i
i
i
s
s
s
e
e
e
e
n
deception
s
s
sit down
3
2
3
i
i
i
i
i
1
