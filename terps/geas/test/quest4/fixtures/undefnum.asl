' inc/dec on a numeric variable the game never declared.
'
' Quest reads the variable with GetNumericContents, which answers -32767 when
' there is no such variable, and ExecuteIncDec (V4Game.cs:3705) then clamps
' anything at or below -32766 to zero before adding the change:
'
'     var value = GetNumericContents(variable, ctx, true);
'     if (value <= -32766) { value = 0d; }
'
' So "inc <ghost; 90>" against nothing at all leaves 90, and the variable now
' exists.  Without the clamp it leaves -32677 and every subsequent comparison
' against a sane number is dead.
'
' The corpus case is "Skate ur @SS off" (Wilson Trent, ASL 410), whose spin
' scoring incs an undeclared "degrees turned" by 90 per LEFT or RIGHT and then
' runs "select case <%degrees turned%>" over "case <180>" ... "case <720>".
' Unclamped, none of the four cases can ever match and the trick scores nothing.
'
' The clamp is on the value read, not on "is it declared", so a *declared*
' variable sitting at or below -32766 is reset the same way -- Quest cannot tell
' the two apart either, and `sunk` below pins that.
'
' dec is the same path: "dec <shade; 90>" on nothing counts down from 0 to -90,
' not from -32767.

define game <Undefnum>
 asl-version <410>
 start <Lab>

 define variable <sunk>
  type numeric
  value <-40000>
 end define

 define variable <deep>
  type numeric
  value <-32766>
 end define
end define

define room <Lab>
 look <The lab.>

 command <bump> {
  inc <ghost; 90>
  msg <ghost = %ghost%>
  select case <%ghost%> {
   case <90> msg <case 90 matched>
   case <180> msg <case 180 matched>
  }
 }

 command <drop it> {
  dec <shade; 90>
  msg <shade = %shade%>
 }

 command <step> {
  inc <tally>
  msg <tally = %tally%>
 }

 command <sink> {
  inc <sunk; 5>
  msg <sunk = %sunk%>
 }

 command <plumb> {
  inc <deep; 5>
  msg <deep = %deep%>
 }
end define
