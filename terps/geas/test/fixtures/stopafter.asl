' Script execution after the game has finished.
'
' Quest's ExecuteScript returns immediately for every line once the game is
' over (V4Game.Part2.cs:5695-5698):
'
'     Private Async Function ExecuteScript(scriptLine, ctx, ...)
'         If String.IsNullOrEmpty(Trim(scriptLine)) Then Return
'         If _gameFinished Then Return
'         ...
'
' and _gameFinished is set by GameFinished(), which FinishGame() calls after it
' has printed the win/lose text section (V4Game.Part2.cs:7424-7436).  playerwin,
' playerlose and stop all go through FinishGame -- stop as StopType.Null, i.e.
' with no text section (V4Game.Part2.cs:5790-5801).
'
' So those three statements do not merely end the game: they abandon the rest of
' the enclosing script, and every script that would have run after it.  A braced
' block is executed one line at a time by the same function (the vbCrLf split at
' V4Game.Part2.cs:5700-5723), so each following line hits the guard in turn --
' being inside the same { } as the playerwin buys nothing.
'
' geas used to run them.  Three games in the corpus are written as though the
' trailing lines still fire, which is how the divergence stayed invisible:
'
'   * Ponyville.asl:1752-1759 -- a bare `stop` in the middle of the chapter-2
'     ending, followed by more msg text and a playerwin.  Quest stops at the
'     `stop`; the chapter-end card and the playerwin are dead code.
'   * FHA.asl:44-47 -- `playerwin` then the msg holding the whole ending, in a
'     game with an empty win text section, so that ending is silent in Quest.
'   * Beam_1_10.cas -- `playerwin` then `do <customwin>`, where customwin is
'     four paragraphs of ending.  Also dead.
'
' This fixture is the `stop` half, shaped like Ponyville's ending: a bare stop
' with a msg and a playerwin behind it.  Nothing after the stop may run, so the
' win text section must not appear and the game must not end up won.  See
' winafter.asl for the playerwin half.
'
' The room's afterturn is a script too, so it announces the LOOK turn and not
' the HALT turn.

define game <Stopafter>
 asl-version <410>
 start <Hall>
 command <halt> {
  msg <before stop>
  stop
  msg <after stop>
  playerwin
  msg <after playerwin>
  do <tail>
 }
end define

define text <win>
You won.
end define

define room <Hall>
 look <The hall.>
 afterturn msg <(a turn passed)>
end define

define procedure <tail>
 msg <the tail procedure ran>
end define
