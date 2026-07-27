' The second form of GIVE: "give <character> the <item>".
'
' ExecGive splits the text after "give " itself, and it looks for two separators
' in a fixed order (V4Game.Part2.cs:4618-4643):
'
'     toPos = InStr(giveString, " to ")
'     if toPos = 0 then
'         toPos = InStr(giveString, " the ")
'         if toPos = 0 then BadGive : return
'         item      = Trim(Mid(giveString, toPos + 4, ...))
'         character = Trim(Mid(giveString, 1, toPos))
'     else
'         item      = Trim(Mid(giveString, 1, toPos))
'         character = Trim(Mid(giveString, toPos + 3, ...))
'
' So " to " wins if it is there at all, and only a command with no " to " in it
' falls back to " the " -- where the operands are the other way round, the
' recipient first.  Both splits are at the *first* occurrence, and the recipient
' half keeps whatever article the player typed, which is why "the gatekeeper"
' has to resolve as well as "gatekeeper" does.
'
' geas implemented the " to " form only, and answered the other with "I don't
' understand your command."  Tai & David's Amazing Maze (TDAMAZINGMAZE.cas) is
' unfinishable that way: the gate north out of the first room is unlocked by the
' Gatekeeper's `give <Document of Approval2>` script and nothing else, and the
' game's own hint for it is `msg <Type "Give The Gatekeeper the Document of
' Approval">`.
'
' The last command below is the precedence check: it contains both separators,
' so the split is on " to " -- item "the officer the letter", recipient "sign" --
' and both halves are nonsense, so it has to fail rather than quietly fall
' through to the reading that would have worked.

define game <Givethe>
 asl-version <400>
 start <Gate>

 startscript {
   give <letter>
 }
end define

define room <Gate>
 look <A gate, and a man in front of it.>

 define object <gatekeeper>
  look <He has a clipboard and all the time in the world.>
  prefix <the>
  alt <man>
  gender <he>
  article <him>

  give <letter> msg <"That's the one," he says, and stands aside.>
  give anything msg <He turns #quest.give.object.name# over and hands it back.>
 end define

 define object <letter>
  look <A letter of approval.>
  prefix <a>
 end define

 define object <apple>
  look <Bruised.>
  prefix <an>
 end define
end define
