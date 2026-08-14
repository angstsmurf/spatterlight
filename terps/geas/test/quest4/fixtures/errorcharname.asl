' quest.error.charactername: filled with the give-recipient's object name on
' the ItemUnwanted refusal path, and nowhere else -- Quest sets it only in
' ExecGive (V4Game.Part2.cs:4788, and 4835 on the ASL2 side), alongside
' quest.error.gender (the recipient's) and quest.error.article (the item's).
' geas left it unset until the // SENSITIVE? audit follow-up, so a game whose
' custom itemunwanted message names the refuser printed a blank.
define game <CharacterName>
 asl-version <410>
 start <Bar>
 error <itemunwanted; #quest.error.charactername# waves #quest.error.article# away.>
 startitems <coin>
end define

define room <Bar>
 define object <barman>
  speak <The barman grunts.>
  gender <he>
 end define
 define object <coin>
  article <the coin>
 end define
end define
