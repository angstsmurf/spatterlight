! "create exit <dir> <room; dest>" over a direction the room block already
! declared -- and what happens to a declared *script* exit when it does.
!
! From ASL 4.10 on a direction's exit is a long-lived object, and re-creating it
! reuses that object rather than building a new one.  RoomExits.SetDirection says
! why in as many words (RoomExits.cs:20-33, and the matching comment in
! RemoveExit, RoomExits.cs:575-591):
!
!     Don't remove directional exits, as if they're recreated a new object will
!     be created which will have the same name as the old one.  This is because
!     we can't delete objects yet...
!
! So "destroy exit <room; north>" only clears the exit object's Exists flag, and
! the next "create exit north <room; dest>" finds the direction still in the
! dictionary and hands the old object back with everything on it intact.
!
! What is on it includes the script.  AddExitFromCreateScript rewrites
! `create exit north <Cell; Freedom>` into the tag `north <Freedom>`
! (RoomExits.cs:199-256); AddExitFromTag then takes <Freedom> as the parameter,
! finds nothing after the ">", and so calls SetToRoom and *not* SetScript
! (RoomExits.cs:161-192) -- and SetScript would have ignored an empty string
! anyway (RoomExit.cs:82-88).  Go tests for a script before it looks at the
! destination (RoomExit.cs:240-259), so the new destination is dead weight: the
! declared script runs, exactly as it did before the create.
!
! Below 4.10 there are no exit objects and the statement is a field assignment:
! ExecuteCreateExit writes the destination into r.North.Data and stamps
! r.North.Type back to Text (V4Game.cs:5429-5450), wiping whatever script the
! room block declared.  "destroy exit" is a no-op there, because pre-4.10 it only
! ever searched the room's *places* list and a direction name matches nothing in
! it (V4Game.cs:3608-3654).  createexitscript.asl is this same source at 400.
!
! Three corpus games turn on this, two of them happily:
!
!  * Tim Hamilton's "The Maze" (410) holds a 15x20 hedge maze in one room; its
!    four compass exits are scripts that adjust %xpos%/%ypos% and re-enter the
!    room, and every move destroys and re-creates them to make the walls show up
!    in #quest.doorways#.  The re-creation passes "<Maze; Maze>", so if it
!    replaced the script the maze would become one room you can never leave.
!  * "Bear Campsite" (400) declares `south { msg <a Grizzly Bear blocks your
!    path> ... }` and opens the way out with `create exit south <Campsite;
!    Freedom>` once the bear has choked on the fish.  There the destination has
!    to win, and below 410 it does.
!  * Jhames' "Nearco" (410) wants Bear Campsite's behaviour out of the 4.10
!    engine and cannot have it.  Its three coin rooms sit behind
!    `north msg <...la losa que bloquea el paso...>` and friends, and the three
!    stone cylinders in the palace open them with `create exit north <PBE2;
!    PBF2>`.  The doors never open, so the chest, the scroll and the ending
!    behind them are unreachable and the game cannot be finished.  See the
!    walkthrough's header.
!
! Below: NORTH is a declared script exit, SOUTH a declared plain destination, and
! EAST is not declared at all.  Only NORTH tells the two versions apart.
define game <Createexitscript>
 asl-version <410>
 start <Cell>
end define

define room <Cell>
 look <A cell.  Barred door north, exercise yard south.>
 north msg <A guard blocks the way north.>
 south <Yard>

 command <opennorth> create exit north <Cell; Freedom>
 command <opensouth> create exit south <Cell; Freedom>
 command <openeast> create exit east <Cell; Freedom>
 command <shutnorth> destroy exit <Cell; north>
 command <home> goto <Cell>
end define

define room <Yard>
 look <The exercise yard.>
 up <Cell>
 command <home> goto <Cell>
end define

define room <Freedom>
 look <Free at last.>
 down <Cell>
 command <home> goto <Cell>
end define
