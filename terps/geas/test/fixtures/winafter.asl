' The playerwin half of stopafter.asl -- read that fixture's header first for the
' Quest source references (V4Game.Part2.cs:5695-5698, 7424-7436).
'
' playerwin prints the win text section and then sets _gameFinished, so the msg
' and the `do` behind it are both dead, whether or not they sit inside the same
' braced block.  Two corpus games are written this way round and lose their whole
' ending to it: FHA.asl (which has an empty win section, so its car ending is
' silent) and Beam_1_10.cas (playerwin followed by `do <customwin>`).
'
' The last thing this transcript may show is the win text section, "You won." --
' neither "after playerwin" nor "the tail procedure ran" may appear.  That text
' section is the difference from stopafter.asl, where stop (StopType.Null) has no
' section to print and so ends the game in silence.

define game <Winafter>
 asl-version <410>
 start <Hall>
 command <win> {
  msg <before playerwin>
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
