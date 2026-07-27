' A timer's period is exactly its interval, and switching one on or off neither
' costs it a tick nor restarts its countdown.
'
' Quest counts up and compares (Tick, V4Game.Part2.cs:92-119):
'
'     if (_timers[i].BypassThisTurn) { _timers[i].BypassThisTurn = false }
'     else {
'         _timers[i].TimerTicks += elapsedTime
'         if (_timers[i].TimerTicks >= _timers[i].TimerInterval) {
'             _timers[i].TimerTicks = 0
'             timerScripts.Add(_timers[i].TimerAction)
'         }
'     }
'
' so an `interval <2>' timer fires on every second tick -- and `timeron' /
' `timeroff' set BypassThisTurn but never touch TimerTicks (SetTimerState,
' V4Game.Part2.cs:561-574), so the turn a timer is switched spends that tick
' doing nothing, and a timer switched off and on again resumes from where it
' stopped instead of starting over.
'
' geas counts down instead, which is the same thing only if the countdown is
' decremented and tested in one step; testing before decrementing gives a period
' of interval + 1 and drifts a timer chain further out of step every firing.  And
' geas used to reset timeleft in `timeron', which deadlocks any pair of timers
' where a short one re-arms a longer one.  "Something 'Bout A Hex" is the game
' that cares: its Louisville sightseeing drive is `christchurch' (interval 2)
' arming `christchurch2' (interval 3), and only christchurch2 can switch
' christchurch off, so with a resetting timeron christchurch2 was re-armed before
' it could ever reach its interval and the drive never got past its first stop.
'
' The turn-by-turn expectation below, one tick per turn:
'
'   1  t            pulse 2->1                drive 3->2
'   2  t            pulse fires, re-arms      drive 2->1
'   3  stop drive   pulse 2->1                drive bypassed, still 1, now off
'   4  t            pulse fires               (off)
'   5  t            pulse 2->1                (off)
'   6  start drive  pulse fires               drive bypassed, still 1, now on
'   7  t            pulse 2->1                drive 1->0, fires
'
' Note the last line: after being off for three turns the drive needs one more
' counted tick, not three.  This fixture is the one place in the suite that runs
' with --tick, through fixtures/timerperiod.args.
define game <Timerperiod>
 asl-version <410>
 start <Car>
end define

define timer <pulse>
 interval <2>
 action msg <[pulse]>
 enabled
end define

define timer <drive>
 interval <3>
 action msg <[drive]>
 enabled
end define

define room <Car>
 look <The inside of a car.>
 command <t> msg <[turn]>
 command <stop drive> {
  msg <[switching the drive off]>
  timeroff <drive>
 }
 command <start drive> {
  msg <[switching the drive back on]>
  timeron <drive>
 }
end define
