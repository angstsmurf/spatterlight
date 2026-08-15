' A turn's timers tick at its first `wait', not at its end.
'
' Quest's SendCommand starts the turn, waits for it to *suspend*, and only then
' ticks (V4Game.Part2.cs:78-88):
'
'     _turnSuspendedTcs = new TaskCompletionSource()
'     _ = HandleCommandAsync(command)
'     await _turnSuspendedTcs.Task
'     if (elapsedTime > 0) await Tick(elapsedTime)
'
' A turn suspends when it finishes -- but also at a `wait' or a `pause', which
' is exactly what those two do: DoWaitAsync and PauseAsync both call
' SignalTurnSuspended before awaiting the player (V4Game.cs:3674-3680 and
' 6663-6669).  Only the first one in a turn counts, since the turn's
' TaskCompletionSource is made once per SendCommand and completing an already
' completed one does nothing.  So in a turn that stops for a keypress the
' timers fire *at the keypress*, and the rest of the turn's script runs after
' them.  geas had no notion of suspension: wait_keypress returned and the tick
' came when the whole turn was over.
'
' The quieter half of the same rule is the second command below.  Tick skips a
' timer on the turn it was armed (BypassThisTurn, V4Game.Part2.cs:118-123, which
' geas has too), but a `timeron' that runs *after* the turn's wait has already
' missed that turn's tick, so its bypass survives into the next turn and is
' spent there instead -- the timer starts one whole turn later than it does if
' the tick comes at the end of the turn.
'
' King's Quest V is the corpus case, and it is built out of this pattern: its
' rooms arm their cut-scene countdowns from a script that opens with a wait,
'
'     script {
'         if not flag <bandit_temple_first> then {
'             wait <press any key>
'             do <desert bandit>
'
' where the procedure ends in a `timeron'.  The bandits at the cliff wall, the
' rat that gnaws Graham free in the inn basement and the wolf that takes Cedric
' on the snowy slope all moved a turn early before this fix.
'
' The |w code inside a printed string is a wait as well -- Quest's own printer
' runs it through the same DoWaitAsync (V4Game.Part2.cs:6747-6753) -- and since
' geas prints in GeasInterface::print_formatted, on the far side of the engine,
' it reaches the tick by calling back through GeasRunner::turn_suspended.
'
' What is left are the suspension points that put a question to the player -- a
' yes/no ask (ExecuteIfAskAsync, V4Game.cs:7355-7360), a menu (ShowMenuAsync,
' V4Game.Part2.cs:8172-8179) and `enter' (ExecuteEnterAsync, ibid. 6106-6113) --
' and geas deliberately does not tick at any of those.  The last two commands
' below are what that looks like.
'
' Unlike `wait', they *print* while they wait, and the two engines print at
' different moments: Quest hands the question to the host and suspends, so the
' tick lands before the prompt, while geas's make_choice and choose_yes_no print
' the prompt and read the answer in one call, which leaves nowhere to put the
' tick but after it.  Neither order can match the other, the semantic difference
' is confined to scripts that both answer a question and then read a timer they
' armed in the same turn, and ticking there costs three corpus walkthroughs
' their timing for nothing.  So the tick stays at the end of such a turn.
define game <Timerwait>
 asl-version <410>
 start <Hall>
end define

define timer <clock>
 interval <1>
 action msg <[tick]>
 disabled
end define

define room <Hall>
 look <A stone hall.>

 command <t> msg <[turn]>

 ' Arming a timer after the wait: the tick has already gone by, so the bypass
 ' is spent on the NEXT turn and the clock's first firing is the turn after
 ' that.
 command <arm> {
  msg <[before the wait]>
  wait <[press a key]>
  timeron <clock>
  msg <[after the wait]>
 }

 ' A plain turn with a wait in the middle of it: the tick lands between the
 ' two messages, not after them.
 command <go> {
  msg <[before the wait]>
  wait <[press a key]>
  msg <[after the wait]>
 }

 ' A |w inside a printed string is a wait like any other: the tick lands at the
 ' keypress, between the two halves of the message.
 command <code> msg <[before the code]|w[after the code]>

 ' `pause' suspends the turn in the same way.
 command <p> {
  msg <[before the pause]>
  pause <10>
  msg <[after the pause]>
 }

 ' A question and a menu suspend the turn in Quest, but geas ticks at the end
 ' of these two turns rather than at the prompt -- see the head of the file.
 command <q> {
  msg <[before the question]>
  if ask <Ready?> then msg <[yes]> else msg <[no]>
  msg <[after the question]>
 }

 command <m> {
  msg <[before the menu]>
  choose <pick>
  msg <[after the menu]>
 }
end define

define selection <pick>
 info <Which one?>
 choice <One> msg <[one]>
 choice <Two> msg <[two]>
end define
