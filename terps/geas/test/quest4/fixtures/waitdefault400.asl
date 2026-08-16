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
' This file is the branch below 4.10, where the text is a string literal in
' the engine rather than the error table: the `error <defaultwait; ...>' line
' this game carries is not reached, and the literal begins with |n, so the line
' comes out with a blank line above it.  waitdefault410.asl and
' waitdefault410err.asl are the two 4.10 branches.
'
' Quest tests the *unevaluated* rest of the line, so `wait <>' takes the first
' branch and prints its (empty) message; only a `wait' with no parameter at all
' reaches the default.
'
' Only the `wait' statement does this.  The |w code in a printed string waits
' with no message of its own (V4Game.Part2.cs:6747-6753), which is why the
' default is built at the statement and not inside wait_keypress.

define game <Waitdefault400>
 asl-version <400>
 start <Hall>
 error <defaultwait; [the game's own defaultwait]>
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
