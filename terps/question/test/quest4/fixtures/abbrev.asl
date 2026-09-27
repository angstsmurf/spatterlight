' Loose noun matching: a word from the middle of an object's name resolves to it,
' but only when the game allows abbreviations.
'
' Disambiguate matches in two passes (V4Game.cs:4700-4790).  The first compares
' the typed noun against each alias and alt name in full, lowercased.  Only if
' that finds nothing does the second pass look for the noun as a whole word
' inside a name, and that pass is gated:
'
'     if (ASLVersion >= 391) & (numberCorresIds = 0) & _useAbbreviations & ...
'
' _useAbbreviations starts true and is cleared by `abbreviations off' in a
' `define options' block (SetUpOptions, V4Game.Part2.cs:885-896).
'
' This fixture is the abbreviations-on half of the pair: at ASL 410 with no
' options block, X OAK reaches the heavy oak door.  See abbrevoff.asl for the
' same game with the option switched off, and altcase.asl for the pre-391 half
' of the version gate.
define game <Abbrev>
 asl-version <410>
 start <Lab>
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
