' The bundled standard.lib (standardlib_builtin.hh): A.G. Bampton's Quest 2
' container library, which Quest ships and splices in whenever a game
' !includes it and has no copy of its own next to the game file
' (V4Game.cs:1397-1415).  geas used to skip the include, which made a game
' written against the library play differently from Quest: the library's
' !addto synonyms rewrite GET X into TAKE X before any game or room command
' can match, and its LOOK AT override appends a contents line after an
' object's description.  A Certain Oscar is the corpus witness (byte-identical
' to QuestViva in the oracle sweep, 2026-08-16); this fixture is the committed
' one, since games/ is not.
'
' What the transcript pins, in order:
'  - ABOUT prints "Version:" with the colon (ShowGameAbout,
'    V4Game.Part2.cs:3648).  geas used to print "Version " bare.
'  - set <standard.lib.version;b003> FAILS: below ASL 2.80 `set' knows only
'    collectables (V4Game.Part2.cs:6157-6160) and no such collectable exists,
'    so the variable stays empty and the library's Look_Proc takes its Old_
'    (Quest 2.0 compatibility) path.
'  - LOOK AT CRATE runs Old_Look_Proc/Old_Contents_Proc: $locationof(crate)$
'    is the current room, `goto <crate>' finds no such room and leaves us in
'    place, and the room's visible objects print as " a coin." -- empty
'    prefix, space, #quest.formatobjects#, full stop.
'  - GET UP answers the baditem error, NOT the room's `command <get up>`:
'    the synonyms have already turned the input into TAKE UP, so the room
'    command is dead code under this library -- in real Quest too.
'  - TAKE COIN goes through Old_Special_Take_Proc down to a plain
'    `take coin', which the engine refuses ("You can't take it.") because
'    the coin has no take action -- the library passed it through instead
'    of swallowing it.
!include <standard.lib>

define game <standardlib>
asl-version <217>
start <ward>
game version <1.0>
game author <fixture>
game copyright <none>
game info <standard.lib splice fixture>
error <baditem;What? Where?>
startscript do <boot>
end define

define synonyms
carton = crate
end define

define room <ward>
look <A bare test ward.>
command <get up> msg <ROOM COMMAND RAN - MUST NOT HAPPEN>

define object <crate>
look msg <A slatted crate.>
invisible

end define

define object <coin>
prefix <a>
look msg <A dull coin.>

end define

end define

define procedure <boot>
set <standard.lib.version;b003>
msg <ready>
end define
