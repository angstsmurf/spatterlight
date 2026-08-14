' Case-insensitive name lookup for an untyped `set <name; value>`.
'
' Quest's SetUnknownVariableType decides whether a bare `set` means a string
' variable, a numeric variable or a collectable by scanning the three arrays in
' that order, and all three comparisons are LCase on both sides
' (V4Game.Part2.cs:592-618).  The set it then dispatches to is case-insensitive
' as well for variables (SetNumericVariableContents, V4Game.Part2.cs:495-505,
' which also rewrites the stored name to the casing just used) but NOT for
' collectables (ExecuteSetCollectable, V4Game.cs:6059-6067), so a collectable
' named with the wrong case is found by the scan and then rejected by the set.
'
' geas compared the variable names exactly, so a game that created a variable
' with one casing and reset it with another silently did nothing.  Zombies
' Attack.cas is built on that: its startscript makes "Chance factor" with
' `set numeric <Chance factor; 0>`, and every one of its random rolls resets the
' counter with `set <chance factor; 0>` before `inc <chance factor;
' $rand(1; 10)$>`.  With the reset lost the counter only ever grows, so after
' two or three rolls every check against it fails and the game becomes
' unwinnable -- the lock cannot be picked, no fight can be won, and the plank
' always gives way.
'
' Note the deliberate asymmetry in the expected output: `set <mixedcase; ...>`
' reaches both variables, but `set <gold; ...>` (a collectable defined as
' <Gold>) reports "No such collectable" and leaves it alone -- and `set
' <Gold; +5>` works.  The `inc` and the %...% / #...# reads were already
' case-insensitive, which is why the bug showed up as a variable that would not
' reset rather than as one that would not read.

define game <Setcase>
 asl-version <410>
 start <Hall>
 startscript {
  set numeric <MixedCase; 7>
  set string <StringVar; original>
 }
 collectables <Gold p=10 [! Gold]>
 command <shownum> msg <MixedCase is %mixedcase%>
 command <showstr> msg <StringVar is #stringvar#>
 command <showgold> {
  if has <Gold; =20> then msg <Gold is 20> else {
   if has <Gold; =15> then msg <Gold is 15> else msg <Gold is 10>
  }
 }
 command <resetnum> {
  set <mixedcase; 0>
  inc <mixedcase; 3>
 }
 command <resetstr> set <STRINGVAR; rewritten>
 command <addgoldlower> set <gold; +5>
 command <addgoldexact> set <Gold; +5>
end define

define room <Hall>
 look <The hall.>
end define
