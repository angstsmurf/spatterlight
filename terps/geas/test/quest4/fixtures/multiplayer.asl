' A "gametype multiplayer" game is an ordinary single-player game as far as the
' Quest runner is concerned.  "gametype" and "multiplayer" exist only as entries
' in the keyword table (Libraries/quest.dat:98,100); nothing in the runner ever
' reads the declaration back.  When play begins, the game block is scanned for
' exactly one thing (V4Game.Part2.cs:7992-7999):
'
'     var startRoom = "";
'     for (int i = gameBlock.StartLine + 1, loopTo4 = gameBlock.EndLine - 1; ...)
'     {
'         if (BeginsWith(_lines[i], "start "))
'         {
'             startRoom = await GetParameter(_lines[i], _nullContext);
'         }
'     }
'
' -- so the game starts in <lobby> here, and the multiplayer-only machinery
' (chat, challenges, "player startscript" per connection) simply never comes up.
'
' geas used to throw "Error: geas is single player only." on the multiplayer
' line.  The throw was raised from the middle of the loop that reads the game
' block, so it also skipped the "start" line that came after it, and the caught
' exception left state.location empty: Metal Sonic's Quest came up with "You are
' in " and listed all 148 of its rooms as objects in scope.  This fixture pins
' the fixed behaviour -- the declaration is ignored and the start room is the
' declared one -- rather than the refusal, because refusing is not what Quest
' does.

define game <Multiplayer gametype>
	asl-version <350>
	gametype multiplayer
	start <lobby>
	game author <geas test>
	game version <1.0>
end define

define room <lobby>
	look <A round room with a desk in it.>
	north <hall>

	define object <desk>
		look <A cyber-netic desk.>
		article <it>
	end define
end define

define room <hall>
	look <A corridor leading back south.>
	south <lobby>
end define
