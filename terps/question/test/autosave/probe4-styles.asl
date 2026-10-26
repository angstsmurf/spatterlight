' Probe game for run_autosave_tests.py (Quest 4 half): the game-derived
' styles across a relaunch.  The startscript prints a centred bold 20-point
' title and pauses for a key under it; the room and `big` use bold, italic
' and 14-point text separately.  The style table is built from a census of
' this file before the first window opens, on every launch, so a relaunch
' -- from the pause under the title or after a turn -- must print every span
' in the style the single session gave it.
'
' Not a fixture, like probe4.asl.
define game <Autosave Styles Probe>
 asl-version <410>
 start <Hall>
 startscript {
  msg <|jc|s20|bThe Great Title|xb|s00|jl>
  wait <Press a key to begin.>
  msg <An ordinary introduction in the game's own size.>
 }
 command <big> msg <Here is |s14a larger sentence|s00 and |bbold words|xb and |iitalic words|xi.>
end define

define room <Hall>
 look <A hall with |bbold words|xb in it, and |s14a larger sentence|s00 after.  Then |iitalic|xi text.>
end define
