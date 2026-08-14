' Where a $function(...)$ call ends.
'
' Quest finds the closing paren of a function call with InStrRev -- it searches
' backwards from the end of the expression (DoFunction, V4Game.cs:6745) -- so the
' *last* right paren in the call closes it, and any parens inside the argument
' list are just characters in an argument.  geas used to stop at the first right
' paren instead, which silently truncated the argument list: a two-argument call
' whose first argument contained a paren was handed a single argument, missed the
' arity check, and returned the empty string.
'
' Barbarian (Wonderjudge, ASL 410) walks straight into this.  Wielding a weapon
' renames it, so the axe in your hands is the object "Battle axe (wielded)", and
' the game's own UNWIELD command strips the suffix back off arithmetically:
'
'     set numeric <length1; $lengthof(#command1#)$>
'     dec <length1; 10>
'     set string <weapon1; $left(#command1#; %length1%)$>
'
' With the first-paren rule $left$ saw one argument, returned nothing, and the
' game then hid an object called "" and showed nothing in its place -- the axe was
' gone for good and the endgame duel, which requires "Battle axe (wielded)", was
' unreachable.
'
' STRIP below is that same computation.  CHUNK checks the same rule with a
' three-argument call, whose parens sit in the middle of the argument list, and
' BARE checks that a literal paren is passed through untouched when it is not at
' the end of the call either.

define game <Funcparen>
 asl-version <410>
 start <Workshop>

 command <strip #thing#> {
  set numeric <len; $lengthof(#thing#)$>
  dec <len; 10>
  set string <weapon; $left(#thing#; %len%)$>
  msg <base is [#weapon#]>
 }

 command <chunk #thing#> {
  msg <chunk is [$mid(#thing#; 8; 3)$]>
 }

 command <bare> {
  msg <right is [$right(a (b) c; 5)$]>
 }
end define

define room <Workshop>
 look <A workshop.>
end define
