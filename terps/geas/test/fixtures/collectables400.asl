' Collectables as an ASL 4.00 game sees them -- the other half of
' fixtures/collectables.asl, which covers the Quest 2.x side.
'
' Three things change once a game declares ASL 2.80 or later:
'
'   * "set" is no longer a synonym for "set a collectable".  From 2.80 it takes
'     an explicit kind -- "set string", "set numeric", "set collectable" -- and a
'     bare "set" searches for the name, string variables first, then numeric
'     ones, and collectables only last (SetUnknownVariableType,
'     V4Game.Part2.cs:576-621).  So a numeric variable named Money shadows a
'     collectable named Money, and there is no way left to reach the
'     collectable except by saying "set collectable".
'   * That search is case-insensitive, but the set it hands off to is not
'     (ExecuteSetCollectable, V4Game.cs:6059-6066): "set <money; +1>" finds the
'     collectable Money, decides this is a collectable set, and then reports
'     "No such collectable" and changes nothing.
'   * From 2.84 the status pane is built from "define variable" display strings
'     instead of the collectables, which stop appearing in it altogether
'     (UpdateItems, V4Game.Part2.cs:7398-7420), and from 3.20 "~Name~" is no
'     longer a collectable reference -- a tilde is just a tilde
'     (ConvertParameter, V4Game.cs:1884-1893).  Games that still want the value
'     read it back with "if has".
'
' Purse exists to be shadowed; Stash is only ever reached by name.

define game <Collectables400>
 asl-version <400>
 start <Vault>
 collectables <Purse p=100 [! Dollars]; Stash p=7 [! in the stash]>
end define

define variable <Purse>
 type numeric
 value <42>
 display <Purse: !>
end define

define room <Vault>
 look <The vault.>

 ' The tilde is inert here, and "has" is the only way to read a collectable.
 command <report> msg <Purse variable %Purse% tilde ~Purse~>
 command <count purse> if has <Purse; =100> then msg <purse 100> else msg <purse not 100>
 command <count stash> if has <Stash; =7> then msg <stash 7> else msg <stash not 7>
 command <count stash 9> if has <Stash; =9> then msg <stash 9> else msg <stash not 9>

 ' Untyped: Purse is a numeric variable, so this never reaches the collectable.
 command <fill purse> set <Purse; 500>
 ' Explicit: this one does.
 command <fill collectable> set collectable <Purse; =500>

 ' Untyped, with no variable of that name -- falls through to the collectable.
 command <fill stash> set <Stash; +2>
 ' Right collectable, wrong case: found by the search, rejected by the set.
 command <fill stash badly> set <stash; +100>
end define
