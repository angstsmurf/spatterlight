! Both sides of an "is <a;b>" comparison are arithmetic from ASL 3.91 on:
! ExecuteIfIs runs each operand through ExpressionHandler and keeps the result
! when the operand evaluates (V4Game.cs:7479-7494).  An operand that is not
! numeric all the way through fails the expression and is compared verbatim, so
! a name with a hyphen in it stays a name and "1/0" stays a string.
! See isexpr353.asl for the pre-3.91 reading, where nothing is evaluated.
!
! geas used to compare both sides as plain text at every ASL version, so
! Forward and Back's "if is <%diceout%+1;%totaldice%>" compared "1+1" with "2",
! the game's losing ending never fired, and play ran on past the end.
!
! The "greet" command pins down a second ASL 3.92 gate in the same file:
! DisplayTextSection runs each line of a text block through GetParameter from
! 3.92 on (V4Game.Part2.cs:4089-4096), so a text block can name variables.  geas
! printed those lines raw, which left "#playername#" in Riddle Run's win text.
!
! The last command also pins down "stop": that is FinishGame(StopType.Null)
! (V4Game.Part2.cs:5798-5801), which ends the game just as playerwin does but
! prints nothing, so the commands after it in the script do nothing at all.
define game <IsExpr>
 asl-version <400>
 start <Room>

 define variable <one>
  type numeric
  value <1>
 end define

 define variable <two>
  type numeric
  value <2>
 end define

 define variable <name>
  type string
  value <Jean-Luc>
 end define

 command <plus> if is <%one%+1;%two%> then msg <plus: yes> else msg <plus: no>
 command <minus> if is <%two%-1;%one%> then msg <minus: yes> else msg <minus: no>
 command <times> if is <%one%*2;%two%> then msg <times: yes> else msg <times: no>
 command <divide> if is <%two%/2;%one%> then msg <divide: yes> else msg <divide: no>
 command <paren> if is <(%one%+1)*2;4> then msg <paren: yes> else msg <paren: no>
 command <notequal> if is <%one%+1;!=;%two%> then msg <notequal: yes> else msg <notequal: no>
 command <bare> if is <5.0;5.0> then msg <bare: yes> else msg <bare: no>
 command <hyphen> if is <#name#;Jean-Luc> then msg <hyphen: yes> else msg <hyphen: no>
 command <divzero> if is <1/0;1/0> then msg <divzero: yes> else msg <divzero: no>
 command <trailing> if is <1+;1+> then msg <trailing: yes> else msg <trailing: no>
 command <greet> displaytext <greeting>
 command <stopnow> {
  msg <Stopping.>
  stop
 }
end define

define room <Room>
 look <A room.>
end define

define text <greeting>
Hello #name#, you have %one% coin.
end define
