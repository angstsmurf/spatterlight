' A noun the player types is matched against an object's alt names
' case-insensitively.
'
' Disambiguate collects the candidate names from ObjectAlias plus AltNames and
' compares each one lowercased against the lowercased word the player typed
' (V4Game.cs:4717-4731):
'
'     if LCase(validNames[j]) = LCase(name) or "the " & LCase(name) = ...
'
' geas ran that comparison through starts_with, which is case-sensitive
' (geas-util.cc:238), while the noun arriving from the parser has already been
' lowercased -- so a capitalised alt could never match.  The bug hid for years
' behind the looser whole-word pass, which is case-insensitive and used to run
' for every game; once that pass was correctly gated on ASLVersion >= 391 and
' `abbreviations' the way Quest gates it (Disambiguate's second loop,
' V4Game.cs:4762-4772), every pre-391 game with a capitalised alt lost its alt
' names outright.
'
' The Mansion (ASL 311) is the game that cares.  Its `define object
' <Man in Black Suit>' carries `alias <Man in a black suit>' and `alt <Man>'
' (mansion.asl:3373-3375) and the game is won with USE FIRE AX ON MAN; under the
' case-sensitive test both EXAMINE MAN and that winning command answered "You
' don't have any object by that name" and the game could not be finished.
'
' `x oak' is the control: matching an alt is not the same as matching a word
' inside a name, and at ASL 311 the partial pass must stay switched off.
define game <Altcase>
 asl-version <311>
 start <Lab>
end define

define room <Lab>
 define object <Man in Black Suit>
  alias <Man in a black suit>
  alt <Man>
  examine msg <There's nothing special about the man.>
  use <Fire ax> msg <The ax lops his arm off at the elbow.>
  use anything msg <You can't use that on the man.>
 end define

 define object <Fire ax>
  take
 end define

 define object <heavy oak door>
  examine msg <A heavy oak door.>
 end define
end define
