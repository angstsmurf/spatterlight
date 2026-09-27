' Below ASL 2.80 the inventory *is* the game block's item table.
'
' SetUpItemArrays reads every "items"/"possitems" line in the game block into
' _items (V4Game.Part2.cs:6961-7014), and both the "inventory" command and the
' inventory pane then walk that table from 1 to _numberItems and print every
' entry whose Got flag is set (ibid. 4526-4535 and 7409-7417).  Three
' consequences, and this fixture pins all three:
'
'   * ORDER is the order the items line declares, never the order the player
'     collected them.  Here "rope" is given last and listed first.
'
'   * SCOPE is the table and nothing else.  "give <torch>" names something the
'     table does not declare, so PlayerItem walks the table, matches nothing and
'     falls off the end (ibid. 6681-6690) -- the player is handed nothing, and
'     "if got <torch>" stays false.  An *object* filed under "inventory" is not
'     in the table either, so it is not listed at all: STASH moves the coin
'     there and the inventory never mentions it.  (From 2.80 on it is the other
'     way round -- the table is ignored and the object list is the inventory --
'     which is why the same move in a 2.80 game does show up.)
'
'   * The MATCH that give and lose use is exact and case-sensitive, unlike the
'     case-insensitive one "if got" reads with.  So "lose <Rope>" against an
'     `items <...; rope; ...>' declaration takes nothing away, while the player
'     still has it by every other test.  Only a script line can reach this --
'     what the player typed has been lowercased long before it gets here -- and
'     The Dream Weaver has two: "lose <hooka>" and "lose <good nugget>", both
'     against an `items <Good Nugget; Hooka; ...>' declaration, so the character
'     who takes his bong back in fact takes nothing.
'
' A second items/possitems line contributes only its *first* entry.  Quest
' declares the loop's "reached the end of the list" flag once, ahead of the walk
' over the game block, and never resets it (ibid. 6963), so the do/while behind
' the second line runs one pass and falls straight out.  "lantern" is therefore
' declared and "spade", on the same line, is not -- which is why GIVE SPADE
' hands over nothing.  A VB6 bug, but a fixture is the only way to keep it.
'
' The itemhide217/itemhide280 pair covers the other half of the 2.80 split, the
' give-then-hide idiom.

define game <Itemtable>
 asl-version <217>
 start <Yard>
 possitems <sack, rope, key>
 items <lantern; spade>
 startitems <key>

 command <check #thing#> if got <#thing#> then msg <got #thing#: yes> else msg <got #thing#: no>
 command <grab #thing#> give <#thing#>
 command <ditch #thing#> lose <#thing#>
 ' Written out in the wrong case on purpose -- a typed "ditch Rope" would not
 ' do, since Quest lowercases the player's input before it ever reaches here.
 command <miscase> lose <Rope>
 ' moveobject rather than TAKE: below 2.80 Quest's take handler runs whatever
 ' follows the object's "take" line as a *script* and never moves anything, so
 ' it could not put an object in the inventory here even if asked to.
 command <stash> moveobject <coin; inventory>
end define

define room <Yard>
 look <The yard.>

 define object <coin>
 end define
end define
