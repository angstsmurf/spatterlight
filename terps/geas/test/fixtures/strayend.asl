' An extra "end define" at the top level of the file.
'
' Quest finds its sections by scanning for lines that begin "define", counting
' nested defines up to the matching "end define", recording the block and then
' setting the loop counter past it (V4Game.cs:1745-1787):
'
'     for i = 1 to l
'         if not BeginsWith(_lines[i], "define") then continue
'         defineCount = 1 : curLine = i + 1
'         do
'             if BeginsWith(_lines[curLine], "define") then defineCount += 1
'             else if BeginsWith(_lines[curLine], "end define") then defineCount -= 1
'             curLine += 1
'         loop while defineCount <> 0
'         ... record block i..curLine-1 ...
'         i = curLine - 1
'
' A stray "end define" outside every block is therefore a line the scan steps
' over, and the sections after it are found as usual.  The one place that objects
' is CheckSections, which calls it "Extra 'end define' after block '...'", marks
' it a fatal error and refuses the file (V4Game.cs:248-259) -- except that Quest
' skips CheckSections outright for three games it names in its own source:
' "bargain.cas", "easymoney.asl" and "musicvf1.cas" (V4Game.cs:106-110,
' 1735-1741).  So for those three, and only those three, the file loads and the
' stray line does nothing.
'
' geas has no CheckSections, and its own section scan let the block depth go
' negative on the stray line.  From then on a top-level "define" took the depth
' from -1 to 0 instead of to 1, and the "depth == 1" test that decides whether to
' read a block was never true again -- so every later block in the file was
' silently dropped.  "Venus flytrap: Romantic Music" (musicvf1.cas, ASL 210)
' closes `define procedure <noclothesdie>` twice, two thirds of the way in, and
' lost two thirds of its procedures, the whole of its copied-in standard library
' and its win text: the game loaded, printed its first room, and then no command
' did anything at all.
'
' Below, "two" and "three" are what a truncated parse loses.

define game <Strayend>
 asl-version <400>
 start <Hall>
end define

define room <Hall>
 look <The hall.>
 command <one> do <early>
 command <two> do <late>
 command <three> goto <Attic>
end define

define procedure <early>
 msg <The early procedure ran.>
end define
end define

define procedure <late>
 msg <The late procedure ran.>
end define

define room <Attic>
 look <The attic.>
 command <back> goto <Hall>
end define
