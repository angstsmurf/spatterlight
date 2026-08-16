' Cached-block keyword case, the ensure_cached half that blockcase.asl does
' not reach: command / lib command / beforeturn / afterturn lines, and the
' "override" word after the turn keywords.  Quest reads every one of these
' with the lowercasing BeginsWith (V4Game.cs:1094) -- room "command "
' V4Game.Part2.cs:1215, the game block's "command"/"lib command" tags
' 2350-2375, "beforeturn "/"afterturn " 1276/1281 (rooms) and 1387/1392
' (game), "override" 4205/4595 -- so mixed case works there, and now works
' in geasfile.cc's block cache too.
'
' The hooks also pin the override shape: the Lab's BeforeTurn OVERRIDE must
' suppress the game's BEFORETURN (room script runs first, override skips the
' global one), while its plain AFTERTURN suppresses nothing, so the room
' line prints before the game line.  In the Hall, with no room hooks, only
' the game pair runs.
define game <TurnCase>
 asl-version <410>
 start <Lab>
 Command <wave> msg <You wave grandly.>
 LIB Command <zap> msg <Zap!>
 BEFORETURN msg <(game before)>
 AfterTurn msg <(game after)>
end define

define room <Lab>
 look <A tidy lab.>
 north <Hall>
 CoMmAnD <probe> msg <Probed.>
 BeforeTurn OVERRIDE msg <(lab before)>
 AFTERTURN msg <(lab after)>
end define

define room <Hall>
 look <A bare hall.>
end define
