' Surrounding whitespace is not part of a block name.
'
' A stray space inside the angle brackets is easy to type and impossible to see,
' and QDK preserves it, so a game can end up defining one thing and referring to
' two.  geas registers every block under a trimmed name (readfile.cc) and so
' trims the lookup key as well (build_name_key, geasfile.cc:53-72, and
' fold_lower_into, geasfile.cc:654-671); the two halves have to agree, or a game
' can never match its own definition and the object silently does not exist.
'
' Quest itself trims neither side -- it compares the raw parameter against the raw
' definition -- so there the two spellings are two different names and the game is
' simply broken.  Folding them is the deliberate divergence: the alternative is a
' game that cannot be finished at all, and no game depends on a space-only
' difference being meaningful.
'
' "Something 'Bout A Hex" is the game that cares.  Its journal is
' `define object <Journal >' and the knapsack hands it over with `give <Journal >',
' while `if got <Journal>' elsewhere drops the space; without the fold the journal
' arrived in the inventory but the game never agreed that you had it.
define game <Nametrim>
 asl-version <410>
 start <Camp>
end define

define room <Camp>
 look <A camp with a knapsack in the corner.>

 command <rummage> {
  msg <You dig through the knapsack.>
  give <Journal >
 }

 command <check> {
  if got <Journal> then msg <[got: yes]> else msg <[got: no]>
  if got <Journal > then msg <[got, spelled with the space: yes]> else msg <[got, spelled with the space: no]>
 }
end define

define object <Journal >
 examine msg <Pages of cramped handwriting.>
end define
