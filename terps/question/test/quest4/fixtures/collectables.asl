' Quest 2.x "collectables" -- the whole subsystem, as an ASL 210 game sees it.
'
' A collectable is a named number with a display string, declared once in the
' game block (SetUpCollectables, V4Game.Part2.cs:6838-6935) and shown in the
' status pane, one line each, below ASL 2.84 (UpdateItems, Part2:7398-7420).
' Everything about it is peculiar, and every peculiarity below is Quest's:
'
'   * The declaration is one parameter holding a list of "Name type=value
'     [display]" items.  The separator is the first *comma* after the item
'     starts and only a semicolon when the whole declaration has no comma at
'     all, so a comma anywhere -- inside a display string, say -- shreds the
'     list.  This fixture keeps clear of commas for that reason.
'   * "type" is a range that the value is clamped to after every change
'     (CheckCollectable, Part2:3968-4022): "%" tops out at 100, "p" cannot go
'     below zero, "2r6" bounds both ends, and "r50" sets a maximum only.  A
'     leading "d" on the type is not part of the range: it means "leave this
'     line out of the status pane while the value is zero".
'   * The maximum-only form is read with Mid(type, Len(type) - 1), which is
'     right for a two-digit bound and wrong for anything else -- "r100" yields
'     a maximum of 0, and Cap below is here to pin that down.
'   * The display string puts the value where its "!" is, and drops the text
'     between a pair of "*"s when the value is not 1, which is how a game gets
'     "1 bullet" and "3 bullets" out of one string.  No "[...]" at all means the
'     default "You have N Name"; an empty "[]" means the line is never shown.
'   * "set <Name; +N>" adds, "-N" subtracts, "=N" assigns
'     (ExecuteSetCollectable, V4Game.cs:6050-6100), and below ASL 2.80 that is
'     what *every* bare "set" means -- numeric and string variables do not exist
'     yet (ExecuteSet, Part2:6157-6160).  The name match is case-sensitive.
'   * An operand that is not a number is read as another collectable's name, 0
'     if there is no such collectable (GetCollectableAmount, Part2:6221).
'   * "if has <Name; +N>" is *strictly* greater than N and "-N" strictly less;
'     "=N" is equality, and anything else -- including the "has <x; 1>" that
'     games write when they mean "= 1" -- is simply false (ExecuteIfHas,
'     V4Game.cs:7395-7448).
'   * "~Name~" in a parameter is the collectable's value, a Quest 2.x syntax
'     that stops working at ASL 3.20 (ConvertParameter, V4Game.cs:1884-1893).
'
' The "[status]" lines are the runner asking for the status pane, which is where
' the display strings and the "hide at zero" flag show up.  The script ends with
' an EARN and an UNDO, because the values are kept in ordinary numeric variables
' under a reserved name and so have to travel with save, restore and undo.

define game <Collectables>
 asl-version <210>
 start <Vault>
 collectables <Money dp=100 [! Dollars]; Health %=90 []; Debt p=5 [You owe ! dollar*s*]; Ammo n=3 [! bullet*s*]; Rank 2r6=4 [Rank !]; Cap r100=10 [Cap !]; Sacks d=0 [! sack*s*]; Plain n=7>
end define

define room <Vault>
 look <The vault.>

 ' The value, straight out of the pane, and via the 2.x "~name~" syntax.
 command <report> msg <Money ~Money~ Health ~Health~ Debt ~Debt~ Ammo ~Ammo~ Rank ~Rank~ Cap ~Cap~ Sacks ~Sacks~ Plain ~Plain~>

 ' has: +N is strictly greater, -N strictly less, =N equal, anything else false.
 command <test rich> if has <Money; +100> then msg <over 100> else msg <not over 100>
 command <test poor> if has <Money; -100> then msg <under 100> else msg <not under 100>
 command <test exact> if has <Money; =100> then msg <exactly 100> else msg <not exactly 100>
 command <test bare> if has <Money; 100> then msg <bare matched> else msg <bare failed>
 command <test unknown> if has <Nonesuch; +0> then msg <unknown matched> else msg <unknown failed>

 ' A non-numeric operand is another collectable -- and 0 when there is none.
 command <borrow> set <Debt; +Ammo>
 command <phantom> set <Debt; +Nonesuch>

 command <spend> set <Money; -100>
 command <earn> set <Money; +40>
 command <overspend> set <Money; -500>

 command <heal> set <Health; +50>
 command <wound> set <Health; -200>

 command <promote> set <Rank; +10>
 command <demote> set <Rank; -10>

 command <raise cap> set <Cap; +1>

 command <fire> set <Ammo; =1>
 command <bag> set <Sacks; +2>
 command <unbag> set <Sacks; -2>

 ' Case-sensitive: this one finds no collectable at all and does nothing.
 command <lowercase> set <money; +1000>
end define
