! "create exit <dir> <room; dest>" over a direction the room block already
! declared, below ASL 4.10 -- where the statement is a field assignment rather
! than an operation on an exit object.  ExecuteCreateExit writes the destination
! into r.North.Data and stamps r.North.Type back to Text (V4Game.cs:5429-5450),
! wiping whatever script the room block declared for that direction; and
! "destroy exit <room; north>" does nothing at all, because pre-4.10 it only ever
! searched the room's *places* list and a direction name matches nothing in it
! (V4Game.cs:3608-3654).
!
! So NORTH below opens for good the first time OPENNORTH runs, and SHUTNORTH
! cannot shut it again.  "Bear Campsite" (ASL 400) is the corpus game that wants
! exactly this: it declares `south { msg <a Grizzly Bear blocks your path> ... }`
! and opens the way out with `create exit south <Campsite; Freedom>` once the
! bear has choked on the fish.
!
! createexitscript410.asl is this same source at 410, where the declared script
! survives both the destroy and the create and keeps running -- with the full
! set of citations, and with what that costs Jhames' "Nearco".
!
! Below: NORTH is a declared script exit, SOUTH a declared plain destination, and
! EAST is not declared at all.  Only NORTH tells the two versions apart.
define game <Createexitscript>
 asl-version <400>
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
