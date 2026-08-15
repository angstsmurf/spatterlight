' A modifier word between a statement and its <...> parameter.
'
' Quest reads a message with GetParameter, which returns whatever lies between
' the line's first opening angle bracket and its first closing one
' (V4Game.cs:1858-1874).  Nothing before the bracket is looked at at all, so the
' ASL keyword "nospeak" -- which suppresses the text-to-speech reading of a
' message (Libraries/quest.dat:204) -- is invisible to the printer and the
' message comes out unchanged.
'
' geas read the statement with next_token and demanded the parameter as the very
' next token, so "msg nospeak <...>" was reported as a script error and dropped.
' Five sites in three corpus games write it (The Former x2, Burglary x2, King's
' Quest V), and The Former's was that game's only unclassified oracle line.
'
' "helpmsg" goes through the same arm, and so does a nonsense modifier: Quest
' cannot tell one word from another there, because it never reads the word.

define game <Msgnospeak>
 asl-version <410>
 start <Hall>
end define

define room <Hall>
 look <The hall.>

 command <plain> msg <PLAIN>
 command <quiet> msg nospeak <QUIET>
 command <helpquiet> helpmsg nospeak <HELPQUIET>
 command <gibberish> msg wibble <GIBBERISH>
 command <spaced> msg    nospeak    <SPACED>
 command <substituted> {
   set string <who; world>
   msg nospeak <Hello #who#.>
 }
end define
