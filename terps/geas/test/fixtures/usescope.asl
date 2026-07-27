' USE X ON Y resolves its two blanks in two different scopes, and neither of them
' is the combined "room plus inventory" scope the rest of the parser uses.
'
' ExecUse (V4Game.Part2.cs:5289, 5376-5390):
'
'     id = await Disambiguate(useItem, "inventory", ctx);
'     ...
'     useOnObjectId = await Disambiguate(useOn, _currentRoom, ctx);
'     if (useOnObjectId > 0) foundUseOnObject = true;
'     else { useOnObjectId = await Disambiguate(useOn, "inventory", ctx); ... }
'
' The item is looked for in the inventory alone -- a like-named object standing
' in the room is neither offered nor counted -- and the thing it is used on is
' looked for in the room first, the inventory only as a fallback.
'
' Getting the scopes wrong does not merely pick the wrong object: Disambiguate
' shows a numbered menu when more than one candidate matches, and a walkthrough
' answers that menu with what was meant to be its next command.  "Something 'Bout
' A Hex" is the game that cares.  It pours three drinks that all answer to
' "Jim-n-Ginger" and leaves the ones you have not been handed yet standing in the
' bar, so USE JIM-N-GINGER has exactly one candidate in Quest and three in a
' combined scope.
'
' The three drinks here are that arrangement.  The napkins cover the target half:
' one is in the room and one in your pocket, so the room's must answer.  The
' wallet covers the fallback: it is only in the inventory, and USE ... ON WALLET
' has to reach it anyway.
define game <Usescope>
 asl-version <410>
 start <Bar>
end define

define room <Bar>
 look <The bar. Drinks everywhere.>

 define object <drink poured first>
  alias <Jim-n-Ginger>
  examine msg <A drink still on the bar.>
  use anything msg <You slide the barman's drink over the #quest.use.object.name#.>
 end define

 define object <drink poured second>
  alias <Jim-n-Ginger>
  examine msg <Another drink still on the bar.>
  use anything msg <You slide the other drink over the #quest.use.object.name#.>
 end define

 define object <drink poured for you>
  alias <Jim-n-Ginger>
  alt <my drink>
  take
  examine msg <The one the barman pushed at you.>
  use on anything msg <You tip your Jim-n-Ginger over the #quest.use.object.name#.>
 end define

 define object <bar napkin>
  alias <napkin>
  use anything msg <You mop the bar with the bar's own napkin.>
 end define

 define object <pocket napkin>
  alias <napkin>
  alt <my napkin>
  take
  use anything msg <You mop your hands with your own napkin.>
 end define

 define object <hex circle>
  examine msg <A circle chalked on the floor.>
 end define

 define object <wallet>
  take
  use anything msg <You wipe the #quest.use.object.name# on your wallet.>
 end define
end define
