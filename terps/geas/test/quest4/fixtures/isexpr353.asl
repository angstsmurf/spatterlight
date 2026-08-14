! The pre-3.91 half of isexpr.asl: ExecuteIfIs only evaluates its operands as
! expressions from ASL 3.91 on (V4Game.cs:7479), so in an older game every
! comparison here is a comparison of text and "1+1" is not 2.  The same applies
! to the text block printed by "greet": DisplayTextSection only substitutes
! variables from ASL 3.92 on (V4Game.Part2.cs:4089-4096), so an older game shows
! the line exactly as written.
define game <IsExpr353>
 asl-version <350>
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
