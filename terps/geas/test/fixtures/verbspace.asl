' Whitespace in a game-scope "verb" name list.
'
' ExecVerb walks the names with a do-while (V4Game.cs:2978-3003):
'
'     scp = InStr(verbsList, ";")
'     if scp = 0 then thisVerb = LCase(verbsList)
'                else thisVerb = LCase(Trim(Left(verbsList, scp - 1)))
'     if BeginsWith(cmd, thisVerb & " ") then ...
'     if scp <> 0 then verbsList = Trim(Mid(verbsList, scp + 1))
'
' and GetParameter does not trim what it reads from between the angle brackets
' (V4Game.cs:1874).  So every name split off *at* a semicolon is trimmed, and the
' remainder is trimmed before the next pass -- but the final name, which is the
' whole parameter when there is no semicolon at all, is used exactly as written.
' "verb <use >" is therefore tested as BeginsWith(cmd, "use  ") with two spaces
' and can never match anything the player types.
'
' QDK emits these whenever an author leaves a trailing space in the verb box, and
' games depend on the resulting dead verbs without knowing it.  In "final of
' social studies.cas" (Holocaust WW2 History 2011 Project, ASL 410) the header
' declares "verb <use >" ahead of "verb <use key in>"; only because the first is
' unmatchable does USE KEY IN CLOSED DOOR reach the door's "action <use key in>"
' and open the one locked door in the game.  geas trimmed, so "use " claimed
' every USE in the file and the north half of the map was unreachable.
'
' The asymmetry is worth pinning both ways: "verb <jab >" is dead, but the same
' trailing space in "verb <poke; jab >" is not, because that segment reaches the
' loop through Trim(Mid(...)).
'
' The list splits on ';' only, so the "verb <pick up; take, take>" QDK writes when
' the author separates synonyms with a comma registers "pick up" and "take, take"
' -- not "take", which is why TAKE falls through to the later "verb <take>" and
' gets that verb's property instead.
'
' The property or action an object is asked for is the first name in the list
' (or whatever follows a colon), so "jab" arrives as "poke" and "take, take"
' arrives as "pick up".

define game <Verbspace>
 asl-version <410>
 start <Hall>

 verb <use > msg <DEAD: the trailing-space use verb fired.>
 verb <use key in> msg <You can't use key in that.>
 verb <use> msg <You can't use that.>
 verb <pick up; take, take> msg <DEFAULT pick up.>
 verb <take> msg <DEFAULT take.>
 verb <jab > msg <DEAD: the trailing-space jab verb fired.>
 verb < shove> msg <DEAD: the leading-space shove verb fired.>
 verb <poke; jab > msg <DEFAULT poke.>
end define

define room <Hall>
 look <The hall.>

 define object <box>
  properties <use = You press the box.>
 end define

 define object <door>
  action <use key in> msg <The door swings open.>
  properties <use = The door has no button.>
 end define

 define object <coin>
  properties <pick up = PICKED UP the coin.; take = TOOK the coin.; poke = POKED the coin.>
 end define
end define
