! The room's look text is not a fact about the room, it is a property read at
! every display.  ShowRoomInfo calls GetObjectProperty("look", ...) on each run
! and writes quest.lookdesc from the answer, using the room's own `look` tag
! only when the property is empty (V4Game.Part2.cs:3879-3893).  So a script that
! rewrites the property while the player is standing in the room changes what
! the next LOOK prints, with no need to leave and come back.  geas refreshed the
! variable on arrival only, and went on showing the old text for as long as the
! player stayed put; Beam's !intproc116 does exactly this to its Inside Machine.
!
! `dump` reads the variable back so the fix is visible even when a description
! tag would swallow the printed line.  `blank` is the empty case: the room keeps
! no separate copy of its tag to fall back on -- writing the property rewrites
! the room record's own Look with it -- so a room whose look property has been
! emptied prints no look line at all.
!
! The room's displayed name is read at every display too: ShowRoomInfo builds it
! from the room's alias, prefix and suffix properties a few hundred lines above
! the look text (ibid. 3724-3742), so `property <room; alias=...>` shows on the
! next display without a goto either.  The Hall is that case, and it also pins
! what is *not* in that name: Quest has a Suffix field on objects and on
! characters and none on a room, so `suffix <of mirrors>` is stored and never
! displayed, where geas used to hang it off the end (finding 73).
!
! The third part is the ordering rule.  Quest reads the property *before* it
! looks for the description tag, so a description that rewrites the property
! prints, this turn, the text as it stood when the display began -- the new text
! only appears on the display after.  The Study is that case.
define game <Lookprop>
 asl-version <410>
 start <Cave>

 command <lit> property <Cave; look=A bright cave.>
 command <blank> property <Cave; look=>
 command <dump> msg <lookdesc [#quest.lookdesc#] formatroom [#quest.formatroom#]>
 command <study> goto <Study>
 command <hall> goto <Hall>
 command <rename> property <Hall; alias=Great Hall>
 command <pre> property <Hall; prefix=the>
 command <suf> property <Hall; suffix=of mirrors>
end define

define room <Cave>
 look <A dark cave.>
end define

define room <Hall>
 look <A wide hall.>
end define

define room <Study>
 look <A dusty study.>
 description {
  msg <You peer around.>
  property <Study; look=A tidy study.>
  msg <#quest.lookdesc#>
 }
end define
