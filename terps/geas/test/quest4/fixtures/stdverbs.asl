' `!include <stdverbs.lib>` is Quest's "Additional verbs" library.  It ships
' with Quest rather than with the games, so it is never on disk beside an .asl;
' geas used to answer that include by throwing it away, on the theory that it
' only restated verbs the engine already had.  It does not restate anything:
' the twenty-nine verb responses, the seven commands and the
' several-commands-on-one-line splitter are all content geas has no native
' equivalent for, so a game that included the library used to answer "buy the
' sword" with its own unrecognised-command error and drop the second half of
' "north, then east" on the floor.  geas now splices in a bundled copy
' (stdverbs_builtin.hh) the same way it does the type library.
'
' What is exercised here: a default verb response with its
' #(quest.lastobject):article# (which needs quest.lastobject to be set by the
' object lookup, as Quest's Disambiguate does); a verb the game overrides on
' one object, which must still win; one of the library's own commands; the
' `verb <smell; sniff>` synonym; and both splitters, the ", then " form spelt
' out over continuation lines and the bare-full-stop one on the last line.

!include <stdverbs.lib>

define game <Stdverbs>
 asl-version <410>
 start <Hall>
end define

define room <Hall>
 look <The hall.>
 north <Yard>
 define object <sword>
  prefix <a>
  article <the sword>
 end define
 ' No `article` of its own: the responses have to fall back to whatever the
 ' engine derives, which is the half of #(quest.lastobject):article# most of
 ' the corpus actually exercises.
 define object <stone>
  prefix <a>
 end define
 define object <cake>
  prefix <a>
  article <the cake>
  action <eat> msg <You eat the cake.  It was a good cake.>
 end define
end define

define room <Yard>
 look <The yard.>
 south <Hall>
end define
