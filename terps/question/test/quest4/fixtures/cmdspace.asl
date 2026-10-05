' A command's pattern is trimmed even when it is the only one -- on purpose.
'
' A DELIBERATE deviation from Quest.  ExecUserCommand walks a command's
' `;'-separated alternatives by cutting at each semicolon and trimming both
' halves -- but when there is no semicolon it takes the text as it stands:
' `curCmd = commandList' (V4Game.Part2.cs:2324-2332, and again at 2375-2383 for
' the game block).  So in Quest the trailing space QDK lets an author leave in
' `command <ask one >' is part of the pattern, and since the player's own input
' is trimmed nothing they type can ever match it: Quest answers ASK ONE, ASK
' TWO and ASK FIVE below with "I don't understand your command", and only the
' alternatives of a list, every one of which has been through a Trim, work.
'
' That is an accident no author could have wanted -- it silently deletes the
' answer they wrote -- so Question trims every pattern and all seven lines here
' are answered.  "Gaiaonline Q&A" (QA.asl) is the corpus game with one:
' `command <How do I get a MC? >', the only one of its 187 questions Quest
' cannot answer.  See FINDINGS.md, "A lone command pattern keeps its stray
' spaces".
define game <Cmdspace>
 asl-version <410>
 start <Lab>
 command <ask one > msg <One.>
 command < ask two> msg <Two.>
 command <ask three ; ask four > msg <Three or four.>
end define

define room <Lab>
 command <ask five > msg <Five.>
 command <ask six ;ask seven > msg <Six or seven.>
end define
