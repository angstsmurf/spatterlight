! The other side of doorways410: below 4.10 quest.doorways is the variable
! that is never set, and the three older ones carry the exits.  UpdateDoorways
! fills .dirs and the "out" pair (V4Game.Part2.cs:7275-7365) and ShowRoomInfo
! fills .places (ibid. 3838-3874); the folded quest.doorways belongs to the
! 4.10 branch alone.
!
! The place tag reads the other way round here too.  Its second parameter is
! not a lock message but the destination, and the first is the prefix to print
! in front of it -- "place <the; Shed>" -- while the destination room's own
! prefix is not consulted, which is why the truck loses its "A".
!
! The room display still differs by one line break: Quest builds the out line
! and the directions line into a single roomDisplayText and prints them
! together (finding 8), where geas prints two.  The four variables themselves
! are word for word.
define game <Doorways400>
 asl-version <400>
 start <Hall>

 command <dump> {
  msg <folded [#quest.doorways#]>
  msg <dirs [#quest.doorways.dirs#]>
  msg <places [#quest.doorways.places#]>
  msg <out [#quest.doorways.out#] display [#quest.doorways.out.display#]>
 }
end define

define room <Hall>
 look <A hall.>
 north <Kitchen>
 out <Yard>
 place <Work Truck>
 place <the; Shed>
 up <Loft>
end define

define room <Work Truck>
 prefix <A>
 look <A truck.>
end define

define room <Shed>
 look <A shed.>
end define

define room <Kitchen>
 look <A kitchen.>
end define

define room <Yard>
 look <A yard.>
end define

define room <Loft>
 look <A loft.>
end define
