# The Monster in the Mirror — walkthrough (**WIN** 100/100)

- **Engine:** ADRIFT 3.90, `monster.taf`, "By Mystery". You wake in a grassy
  field after a glass of water at bedtime. You build a ladder up a tree, find
  a riddle book in an abandoned cottage, get washed away to a deserted town,
  and finally smash the monster in your bedroom mirror. It was all a
  nightmare.
- **Result:** WIN, **100/100**, 47 commands, no env.
- **Row:** `monstermirror_solution.txt|monster.taf|So you figured it out|`

## Scoring (every `ACT type=4`)

| Task | Command | Pts |
|---|---|---|
| 0 | `search grass` (gives the hammer) | 3 |
| 3 | `nail planks to tree with hammer` | 3 |
| 2 | `take key` (tree hole) | 3 |
| 7 | `unlock trap door` | 3 |
| 8 | `read book with magnifying glass` | 3 |
| 9 | `turn handle` (pump, after the book) | 5 |
| 12 | `look under stone` (pass card) | 3 |
| 18 | `hit mirror with hammer` (ends the game) | 77 |
| 14 | `press red button` | **−10** (sends you back to the field; avoided) |

3·6 + 5 + 77 = 100, which is the declared maximum. TASK 14 is only allowed
before the green button and costs 10, so the route never presses red.

## Route

Field: `search grass` → hammer. Forest: nails. Cottage: `x wood` first (the
planks on the wood pile are unseen until you examine it; `get planks`
before that says "Take what?"). Back at the tree: nail the planks, climb,
`x hole`, `take key`. In the cottage: `unlock trap door`, then in the cellar
`get magnifying glass` (plain `get glass` gives "Take what?", see below).
Loft: `x shelf`, `get book`, `read book with magnifying glass` ("Turn my
handle, like a knob"). Outside: `turn handle`. The pump floods everything,
takes away your whole inventory (ACT type=0 v1=0), and puts you on Center
Street. Park: `look under stone` → card. Center Street: `put card in slot` →
Plain Building → Control Room. `press green button` moves the whistle into
the fountain spout. `press yellow button` drops the paper riddle ("I play a
tune, that no one hears") and puts the hammer back on your nightstand.
Fountain: `x spout`, `get whistle`, `blow whistle` → your bedroom. There,
`x stand`, `get hammer`, `x mirror`, `hit mirror with hammer`.

## Oddities

- `get glass` → "Take what?" for the object named "magnifying glass" (alias
  "looking glass"), while `get magnifying glass` works. This may be the known
  raw-name noun-matching rule, but I have not checked it against the Runner.
- Objects on surfaces and in containers stay unseen until you examine the
  holder: the planks on the wood, the key in the hole, the book on the shelf,
  the whistle in the spout, the hammer on the stand.
