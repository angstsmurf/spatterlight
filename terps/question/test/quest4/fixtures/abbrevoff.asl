' `abbreviations off' switches the loose noun pass back off, at any ASL version.
'
' The abbreviations-off half of the pair described in abbrev.asl: the second,
' whole-word-inside-a-name pass of Disambiguate is gated on _useAbbreviations
' (V4Game.cs:4762-4772), which `abbreviations off' in a `define options' block
' clears (SetUpOptions, V4Game.Part2.cs:885-896).  So X OAK misses the heavy oak
' door here even though the game is ASL 410, while the first pass keeps working
' for the door's real name and for the man's capitalised alt.
'
' "Exits of The World" (ASL 410) is a game that cares: `define options' /
' `abbreviations off' at Exits of The World.asl:34.  QDK writes that line
' whenever the author unticks the abbreviations box, and an engine that ignores
' the option lets a player reach objects by any word in their name -- including
' names the author deliberately made specific.
define game <Abbrevoff>
 asl-version <410>
 start <Lab>
end define

define options
 abbreviations off
end define

define room <Lab>
 define object <Man in Black Suit>
  alias <Man in a black suit>
  alt <Man>
  examine msg <There's nothing special about the man.>
 end define

 define object <heavy oak door>
  examine msg <A heavy oak door.>
 end define
end define
