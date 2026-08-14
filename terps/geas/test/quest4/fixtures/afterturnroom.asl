! Which room's `afterturn` runs at the end of a turn that moved the player.
!
! Quest reads the room index once, at the top of ExecCommand
! (V4Game.Part2.cs:4116: `var roomID = GetRoomID(_currentRoom, ctx)`), and uses
! that one index for both hooks: beforeturn at 4177-4188 and afterturn at
! 4567-4581.  Nothing recomputes it after the command has been dispatched, so a
! room you walk into does not run its afterturn on the turn you arrive -- the
! afterturn that fires belongs to the room you *left*.  It only starts firing
! for the new room from the following turn on.
!
! A room-level afterturn beginning with `override` still suppresses the game
! block's afterturn (4571-4575), and it is the *old* room's override that
! decides that, not the new room's.
!
! "Blight of Elantria" is built on this.  Its throne room ends every turn with
! `if not flag <icequeendead> then { msg ... playerlose stop }`, and the Ice
! Queen stands in that room, so the player gets exactly one turn inside to
! `use warhammer on ice queen`.  Firing the new room's afterturn on arrival
! would kill the player as they step in and make the game unwinnable.
define game <AfterTurnRoom>
 asl-version <410>
 start <hall>
 afterturn msg <[game afterturn]>
end define

define room <hall>
 look <A plain hall.>
 north <plain>
 east <quiet>
 west <deadly>
 afterturn msg <[hall afterturn]>
end define

! Moving north from the hall must show the hall's afterturn, not the plain's;
! standing still in the plain then shows the plain's.
define room <plain>
 look <An open plain.>
 south <hall>
 afterturn msg <[plain afterturn]>
end define

! No afterturn of its own, so the game block's afterturn is all that runs --
! but only from the turn after arrival, since arrival still belongs to the hall.
define room <quiet>
 look <A quiet corner.>
 south <hall>
end define

! An `override` afterturn suppresses the game block's.  Arriving here suppresses
! nothing (the hall is still the turn's room); leaving here does suppress.
define room <deadly>
 look <A deadly cell.>
 east <hall>
 afterturn override msg <[deadly afterturn -- game hook suppressed]>
end define
