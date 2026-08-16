' What the disambiguation menu calls each of the objects it is choosing between.
'
' Quest asks "Please select which <noun> you mean:" -- printed into the main
' text as "- |i...|xi" ahead of the menu, and echoed back under it as
' "- <the chosen option>" (V4Game.cs:4802-4803, 4854).  Each option is labelled
' by the object's `detail', and where there is none by its `prefix' followed by
' its alias (ibid. 4810-4820; ObjectType.Prefix is stored with the trailing
' space already on it, V4Game.Part2.cs:3273).  So the prefix is what tells two
' same-named objects apart, and a `detail' replaces both.
'
' Measured in Quest 4.1.5 itself, not only in the C# port: three beds aliased
' `bed', the first with a prefix, the second with neither, the third with both a
' prefix and a detail, list as "a bed", "bed" and "a rickety bed".  Note the
' third: the detail wins outright, and its own prefix is not prepended to it.
'
' geas hands the caption and the labels to the host and lets it lay the menu out
' (GeasGlkInterface::make_choice numbers the options in the main window; the
' test harness does the same), so the "- " marker and the italics -- the Quest
' window's own presentation of a menu -- are not part of what the engine emits.
define game <Disambig>
 asl-version <410>
 start <Hall>
 game author <fixture>
end define

define room <Hall>
 look <A bare hall.>

 define object <bed1>
  alias <bed>
  prefix <a>
  look <The first bed.>
 end define

 define object <bed2>
  alias <bed>
  look <The second bed.>
 end define

 define object <bed3>
  alias <bed>
  prefix <the>
  detail <a rickety bed>
  look <The third bed.>
 end define
end define
