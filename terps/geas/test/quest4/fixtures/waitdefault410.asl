' A `wait' with no message of its own is not silent.
'
' ExecuteWaitAsync (V4Game.cs:6103-6118) has three branches:
'
'     if (!string.IsNullOrEmpty(waitLine))  Print(GetParameter(waitLine))
'     else if (ASLVersion >= 410)           PlayerErrorMessage(DefaultWait)
'     else                                  Print("|nPress a key to continue...")
'
' geas took only the first of them and passed the message straight to
' GeasInterface::wait_keypress, so a bare `wait' stopped the turn without
' printing anything at all.
'
' This file is the 4.10 branch, where the text is the overridable `defaultwait'
' error message -- see waitdefault410err.asl for a game that overrides it, and
' waitdefault400.asl for the fixed string below 4.10.
'
' Quest tests the *unevaluated* rest of the line, so `wait <>' takes the first
' branch and prints its (empty) message; only a `wait' with no parameter at all
' reaches the default.
'
' Only the `wait' statement does this.  The |w code in a printed string waits
' with no message of its own (V4Game.Part2.cs:6747-6753), which is why the
' default is built at the statement and not inside wait_keypress.

define game <Waitdefault410>
 asl-version <410>
 start <Hall>
end define

define room <Hall>
 look <A stone hall.>

 ' No message: the engine supplies `defaultwait'.
 command <bare> {
  msg <[before]>
  wait
  msg <[after]>
 }

 ' A message of the game's own is printed instead of it.
 command <own> {
  msg <[before]>
  wait <[the game's own wait line]>
  msg <[after]>
 }

 ' `wait <>' is a message, an empty one: Quest tests the text after `wait'
 ' before evaluating it, so this prints nothing rather than the default.
 ' Prison Break and MetroidLite both open with one.
 command <empty> {
  msg <[before]>
  wait <>
  msg <[after]>
 }

 ' |w is a wait too, and prints nothing extra.
 command <code> msg <[before the code]|w[after the code]>
end define
