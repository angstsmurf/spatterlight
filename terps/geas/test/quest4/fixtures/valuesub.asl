! A variable's initial value is a parameter, not a literal.  SetUpDisplayVariables
! walks the game block and runs every `value <...>` through GetParameter as it
! meets it (V4Game.Part2.cs:744), so the three conversion characters all apply
! before the game starts: "#" for a string variable, "%" for a numeric one, and
! "$" for a function.  Because it is one pass in source order, a variable can be
! built out of the ones defined above it -- and only out of those.  Each one
! comes into existence as the pass reaches it, so a reference to a variable
! further down reads a variable that does not exist yet: `early` gets the empty
! string for `greeting`, and `earlyn` gets Quest's undefined-numeric -32767 for
! `base`, not the 7 that `copied` gets a few lines later.
!
! geas stored the text raw, so all five of these printed their own source.  The
! whole-game case is Kingdom, which initialises 23 village statistics with
! `value <$rand(a;b)$>`: every one of them was zero, and because the rolls never
! happened the RNG stream was 23 draws ahead of Quest's before the first turn.
! `rolled` is that shape, with equal bounds so the draw has only one possible
! answer -- the two engines have different generators and cannot be compared on
! a real range, but they both consume a draw here.
define game <Valuesub>
 asl-version <410>
 start <Room>

 define variable <early>
  type string
  value <[#greeting#]>
 end define

 define variable <earlyn>
  type numeric
  value <%base%>
 end define

 define variable <greeting>
  type string
  value <Hello>
 end define

 define variable <full>
  type string
  value <#greeting# there>
 end define

 define variable <base>
  type numeric
  value <7>
 end define

 define variable <copied>
  type numeric
  value <%base%>
 end define

 define variable <rolled>
  type numeric
  value <$rand(4;4)$>
 end define

 command <dump> {
  msg <early [#early#]>
  msg <earlyn [%earlyn%]>
  msg <greeting [#greeting#]>
  msg <full [#full#]>
  msg <base [%base%]>
  msg <copied [%copied%]>
  msg <rolled [%rolled%]>
 }
end define

define room <Room>
 look <A plain room.>
end define
