' The type library's own synonyms.
'
' typelib.qlb does not only define types: before its type definitions it opens
' with an `!addto synonyms' block (typelib.qlb:52-61) that rewrites a set of
' phrasings onto the verbs its `!addto game' command layer registers --
'
'     back; back from; out of =from
'     inside; in to; in =into
'     look into = examine
'     put =drop
'     don; drop on = wear
'     take off; remove = unwear
'     shut = close
'     talk = speak
'
' -- so a game that includes the library answers PUT HAT IN CRATE and DON HAT
' and SHUT CRATE, none of which its command layer names.  Geas splices the
' library in from typelib_builtin.hh, and that copy used to be missing this
' block, which left every one of those phrasings unrecognised.
'
' The commands below are the ones the block is for.  Each is followed by the
' phrasing it rewrites to, so a fixture line that stops matching its neighbour
' says the synonym has been lost rather than the verb behind it.
!include <typelib.qlb>
define game <TypelibSyn>
 asl-version <410>
 start <Lab>
 game author <fixture>
 startscript TLPstartup
end define

define room <Lab>
 look <A bare lab.>

 define object <crate>
  look <A wooden crate.>
  type <TLTcontainer>
  type <TLTclosable>
 end define

 define object <hat>
  look <A felt hat.>
  type <TLTobject>
 end define

 define object <guard>
  look <A bored guard.>
  type <TLTactor>
  speak msg <"Nothing to report," he says.>
 end define
end define
