! Quest arms an "auto intro" flag before running the startscripts and only shows
! the game block's <intro> text section afterwards if it is still set:
!
!     _autoIntro = true;                                    ' Part2.cs:7946
!     ... run startscript ...
!     if (_autoIntro & (_gameLoadMethod == "normal"))        ' Part2.cs:7987
!         await DisplayTextSection("intro", _nullContext);
!
! The "nointro" statement clears it (V4Game.Part2.cs:6000-6003).  Games with a
! credits sequence use that to place <intro> themselves with an explicit
! displaytext, after the pauses and the questions: "The Devil's Bargain" opens
! its <Beginning> procedure with nointro and closes it with displaytext <intro>.
! geas had no "nointro" statement and always ran displaytext <intro> after the
! startscript, so those games printed their intro twice.
!
! Note that the flag is session state, not game state -- Quest never saves or
! restores it -- so it stays cleared for the rest of the run.  See autointro.asl
! for the default path this fixture is the counterpart of.
define game <nointro>
    asl-version <410>
    start <r>
    startscript do <credits>
end define

define text <intro>
Here is the intro, exactly once.
end define

define procedure <credits>
    nointro
    msg <Credits first...>
    displaytext <intro>
    msg <...and now the game.>
end define

define room <r>
    look <A room.>
end define
