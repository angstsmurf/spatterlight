! An array slot past the end of a string variable reads as the empty string.
! GetStringContents logs "Array index of '...' too big" and returns ""
! (V4Game.Part2.cs:2637-2643); geas answered "!", the marker it uses for a
! property an object has not got, and it went straight into game text.
!
! Wizard's spell menu is the corpus case: twelve numbered slots, three spells,
! and nine lines reading "4) !".  The array here is filled the same way, one
! slot at a time, so the ubound is 3 and slots 4 and 5 are past it.
!
! `unset` is the other half of the finding, and it was already right: a string
! variable that was never defined answers "" (geas-runner.cc:245-246), matching
! the "No string variable ... defined." arm at :2634-2638.
define game <StrUnset>
 asl-version <410>
 start <Room>

 define variable <spell>
  type string
 end define

 command <fill> {
  set string <spell[1]; Fireball>
  set string <spell[2]; Frostbolt>
  set string <spell[3]; Mend>
 }
 command <menu> {
  msg <1) #spell[1]#>
  msg <2) #spell[2]#>
  msg <3) #spell[3]#>
  msg <4) #spell[4]#>
  msg <5) #spell[5]#>
 }
 command <unset> msg <nothing here: #nosuchstring#.>
end define

define room <Room>
 look <A room.>
end define
