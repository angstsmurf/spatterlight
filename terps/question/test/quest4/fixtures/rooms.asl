! Two room-level details:
!
!   "drop nowhere <msg>"      refuses the drop and prints the object's message
!   "out <prefix; room>"      the prefix is the article before the destination
!                             ("out <the; town>" in King's Quest V): everything
!                             before the semicolon, and it must reach the room
!                             description via #quest.doorways.out.display#
define game <Rooms>
 asl-version <311>
 start <Hall>

 command <doorways> {
  msg <out: #quest.doorways.out#>
  msg <display: #quest.doorways.out.display#>
 }
end define

define room <Hall>
 look <A hall.>
 out <the; walled garden>

 define object <vase>
  take
  drop nowhere <The vase is far too heavy to put down here.>
 end define

 define object <coin>
  take
 end define

end define

define room <walled garden>
 look <A garden.>
end define
