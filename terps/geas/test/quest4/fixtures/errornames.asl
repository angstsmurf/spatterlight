' Quest keeps one array of player-error messages indexed by its PlayerError
' enum and lets the game block overwrite entries with "error <name; text>"
' lines (SetDefaultPlayerErrorMessages / SetUpUserDefinedPlayerErrors,
' V4Game.Part2.cs:425-471 and 1392-1625).  Three things about that switch are
' easy to get wrong, and geas got all three wrong:
'
'  * the ASL name of PlayerError.DefaultTake is "defaultake", with one t;
'  * an unrecognised name leaves currentError at 0, so the line silently
'    redefines badcommand instead of being ignored -- which is what a game
'    that spells it "defaulttake" actually gets;
'  * the loop overwrites as it goes, so the LAST matching line wins, where
'    geas returned on the first.
'
' Plus the "error <defaultlook; ...>" line doubles as a defaultexamine line
' unless a defaultexamine line was seen before it, and an unresolved #@object#
' is answered with the overridable badthing message (from ASL 3.91; baditem
' below it) rather than a sentence baked into the engine.

define game <Errornames>
 asl-version <410>
 start <Hall>

 ' The correctly spelt name, and the misspelling geas used to honour.  The
 ' second line is an unknown name, so it redefines badcommand.
 error <defaultake; You scoop up #quest.error.article#.>
 error <defaulttake; That spelling redefines badcommand.>

 ' Last one wins.
 error <badgo; This badgo line is overwritten.>
 error <badgo; You must GO somewhere.>

 ' Overridable in Quest, absent from geas's table until now.
 error <alreadytaken; You are holding #quest.error.article# already.>
 error <badthing; There is no #quest.error.object# here.>

 ' No defaultexamine line, so this one sets both messages.
 error <defaultlook; It looks ordinary enough.>
end define

define room <Hall>
 look <The hall.>
 define object <rock>
  prefix <a>
  take
 end define
 define object <coin>
  prefix <a>
  take
 end define
end define
