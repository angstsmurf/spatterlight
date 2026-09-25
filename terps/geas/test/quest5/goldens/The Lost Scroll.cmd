# The Lost Scroll (peter edwards, 2016); no published walkthrough exists —
# the 117-step script was derived entirely from the game source (game.aslx).
# Win: place all 7 coloured stones (red/orange/violet/blue/green/yellow/indigo)
# in the "mosaic" object, which auto-follows the small black box's current room
# every turn (a `mosaic to box` turnscript), so `put <colour> stone in mosaic`
# works from wherever the box currently is. Once all 7 are in, the `Endgame`
# turnscript fires automatically and prints THE END, moving the player to the
# house of Mr de Sward. Verified: steps=117 emits=511 state=Finished errors=0
# scriptExhausted=True; transcript is clean of every silent-failure phrase.
#
# Gotchas found while deriving this from source (none discoverable in-fiction):
# - The dovecot combination lock ("dial #text#") checks the literal string
#   "0717"; there is no in-game clue anywhere for this code.
# - The small black box's reveal gate is `GetBoolean(box, "seen")`, but the
#   *only* place that flag is ever set is inside the "small ebony box"'s
#   <look> script in the vision puzzle (Tapestry Room) — an apparent authoring
#   mix-up between "small black box" and "small ebony box"/"box". You must
#   finish the full vision chain (chapel key -> unlock upper landing door ->
#   raise ploughman's head -> lower horse's head -> move plough -> look at
#   recess -> look through hole -> look at small ebony box, while carrying the
#   pulpit's "glass") before "feel up chimney" in the Great Hall will work.
# - The gunroom bench's field-glasses are internally named "fieldglasses" but
#   that identifier isn't parseable vocabulary; the working alias is "glasses"
#   ("take glasses"), not "take fieldglasses".
# - Several exits/objects are hidden until specific reveal actions fire:
#   "look at pulpit" (before "up" works from the chapel), "look at cracks"
#   (before the chapel key can be taken), "search undergrowth" (top of the
#   folly, needs the folly door unlocked with the large brass key first, to
#   see the tiny chapel's green stone route), and "knock on door" in the guest
#   room (swings a picture aside, revealing the east exit to the sewing room
#   and the indigo-stone neck button — without it, the room only lists a
#   south exit).
# - "lower glass" (the pulpit shelf's glass, carried since the vision puzzle)
#   is needed at the fishpond before "search blue pebbles" will find anything.
search gravel
take red stone
north
north
west
look at cracks
take key
look at pulpit
up
look at shelf
take glass
down
east
northeast
east
look at bench
take glasses
west
southwest
northwest
west
up
unlock door
south
raise ploughmans head
lower horses head
move plough
look at recess
look through hole
look at small ebony box
north
down
east
southeast
feel up chimney
move brick
pull candlestick
take small black box
put red stone in mosaic
south
northeast
northeast
lower glass
search blue pebbles
take blue stone
put blue stone in mosaic
east
search shade
take large brass key
west
southwest
southwest
northwest
search compost
take orange stone
put orange stone in mosaic
north
search daffodils
smell violets
take violet stone
put violet stone in mosaic
west
northwest
unlock door
in
up
look east
down
out
southeast
east
south
southeast
northeast
northeast
east
search undergrowth
east
in
search moss
take green stone
put green stone in mosaic
out
west
west
southwest
southwest
north
northeast
north
east
dial 0717
in
climb ladder
search straw
take yellow stone
put yellow stone in mosaic
down
out
west
south
southwest
northwest
west
up
east
east
east
east
north
knock on door
east
open wardrobe
look at dress
look at buttons
take neck button
put indigo stone in mosaic
