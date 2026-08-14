! Manual fixture: a 2-second recurring timer that prints while the parser
! prompt is up.  Classic-engine counterpart of aslx/timertest.aslx: drives
! the prompt-retract path in geasglk's evtype_Timer arm (not part of the
! scripted fixture suite: needs the real app, wall-clock timers and a
! keyboard).  Type a partial command, let a few ticks pass, then finish
! typing and submit: the transcript should show one prompt at the bottom
! carrying the typed text, with no stranded "> " above the tick lines.
define game <Timer Prompt Test>
 asl-version <410>
 gametype singleplayer
 start <clock room>
end define

define timer <ticker>
 interval <2>
 action msg <Tick tock.>
 enabled
end define

define room <clock room>
 look <A bare room with a loudly ticking clock.>
end define
