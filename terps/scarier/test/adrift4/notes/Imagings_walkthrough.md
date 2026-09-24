# Imagings — walkthrough (demo; no score, no ending)

- **Engine:** ADRIFT 4.00, `imagings.taf` (3 KB). A surreal six-room dream
  fragment: an examination room, a familiar apartment hallway, a hotel room
  whose window shows only stars, a playroom with a little girl, and a lobby
  with a "Prescription Machine". A weather event cycle (fog → light rain →
  heavy rain → clearing) changes the Church Road text.
- **Result:** deepest point reached, **Church Road** (room 5). MaxScore 0,
  and there is no `ACT type=6` and no terminal room. Church Road has no
  tasks and its only exit is S, back to the lobby, so this is where the demo
  ends. 18 commands, no env.
- **Row:** `imagings_solution.txt|imagings.taf||`

## Route

`n w` → Hotel Room. `open window` (TASK 0 shows '865' in spraypaint).
`get bottle` (it's on the bed). `e d` → Apartment Lobby. `put bottle in
machine`, then `865` (TASK 1; the restriction wants the bottle *inside* the
machine. `put bottle on counter` is refused). The bottle comes back on the
counter with a blue pill: `get bottle`. `u e` → Playroom. `give bottle to
girl` (TASK 2: "I love you, daddy." She rots away and leaves the brass key).
`get key`, `w d`, `unlock door with key` (TASK 3), `n` → Church Road.

The cabinet in the Examination Room is locked (openable=7) and has no key
(key=-1). It never opens and nothing inside it matters.
