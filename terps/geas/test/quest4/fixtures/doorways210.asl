! Below ASL 2.80 the exits line is built by reading the room block back.
! ShowRoomInfoV2 walks the block's own source lines and appends each direction
! as it meets it (V4Game.Part2.cs:1935-1980), so the order in the line is the
! order the author wrote the tags in -- not the fixed compass order that
! UpdateDoorways uses from 2.80 on (ibid. 7229-7286).  QDK writes its diagonals
! NW, NE, SW, SE, so the signature of the old behaviour was a swapped diagonal
! pair; Fade To White (210) and Koww (200) both show it in the corpus.  The tags
! below are deliberately out of compass order for that reason.
!
! That loop also has no branch for "up" or "down".  Both are perfectly walkable
! below 2.80 -- the `up` line here goes somewhere -- but neither is ever named in
! the line or in quest.doorways.dirs.
!
! The rest of the pre-2.80 display is here as the frame around it: the three
! lines come out in the order out, directions, places (ibid. 2022, 2056, 2092),
! the places list keeps its Oxford comma (it splits at the last comma rather
! than one character before it, ibid. 2073-2085), and the 4.10 folded variable
! is untouched at this version.
define game <Doorways210>
 asl-version <210>
 start <Hall>

 command <dump> {
  msg <folded [#quest.doorways#]>
  msg <dirs [#quest.doorways.dirs#]>
  msg <places [#quest.doorways.places#]>
  msg <out [#quest.doorways.out#]>
 }
end define

define room <Hall>
 look <A hall.>
 northwest <Pantry>
 north <Kitchen>
 up <Loft>
 out <Yard>
 place <the; Shed>
 place <Work Truck>
 northeast <Study>
 down <Cellar>
end define

define room <Pantry>
 look <A pantry.>
end define

define room <Kitchen>
 look <A kitchen.>
end define

define room <Study>
 look <A study.>
end define

define room <Loft>
 look <A loft.>
end define

define room <Cellar>
 look <A cellar.>
end define

define room <Yard>
 look <A yard.>
end define

define room <Shed>
 look <A shed.>
end define

define room <Work Truck>
 look <A truck.>
end define
