' The four "help" script statements.
'
' Quest 4 had a second output window for help text, and four statements that
' wrote to it.  The Quest 5 player has no such window, so ExecuteScript sends two
' of them to the ordinary output and drops the other two:
'
'     helpmsg <text>          Print(text)               (V4Game.Part2.cs:5778)
'     helpdisplaytext <name>  DisplayTextSection(name)  (ibid. 5972)
'     helpclose               nothing                   (ibid. 5782)
'     helpclear               nothing                   (ibid. 5988)
'
' geas has one output stream too, so it follows the same reading.  helpmsg and
' helpdisplaytext used to be no-ops here, which silently ate text the game meant
' the player to read; the corpus case is ThunderClan Mystery 1 (Hazelstiltskin,
' ASL 410), where Jayfeather's
'
'     action <GET HERB> helpmsg <***TYPER GET HERB***>
'
' is the game's answer to its own instruction to type GET HERB, and printed
' nothing at all.
'
' helpclose and helpclear are still no-ops, but they must not stop the rest of
' the script running, which `shut` below pins.

define game <Helpmsg>
 asl-version <410>
 start <Study>
end define

define room <Study>
 look <The study.>

 command <hint> helpmsg <The key is under the mat.>
 command <story> helpdisplaytext <Legend>
 command <shut> {
  helpclose
  helpmsg <Still printing after helpclose.>
  helpclear
  helpmsg <And after helpclear.>
 }
 command <both> {
  msg <msg first,>
  helpmsg <then helpmsg,>
  displaytext <Legend>
  helpdisplaytext <Legend>
 }

 define object <scroll>
  look <A scroll.>
  action <read> helpmsg <It says: mind the step.>
 end define
end define

define text <Legend>
Here beginneth the legend.
It is two lines long.
end define
