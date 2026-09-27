! Container cycles.  "put bag in box" then "put box in bag" used to recurse
! forever through the parent chain (stack overflow); an object must also refuse
! to be put inside itself.
define game <Containers>
 asl-version <311>
 start <Room>
end define

define room <Room>
 look <A plain room.>

 define object <bag>
  take
  properties <container; opened; seen>
 end define

 define object <box>
  take
  properties <container; opened; seen>
 end define

end define
