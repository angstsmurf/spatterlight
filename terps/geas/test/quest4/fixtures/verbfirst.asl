! A game-scope "verb" declaration is dispatched ahead of every built-in, not
! after it.  ExecCommand tries user commands, then ExecVerb, then library
! commands and library verbs, and only then reaches its own "speak to"/"take "/
! "look "/"examine "/"use "/movement handling (V4Game.Part2.cs:4205-4240), so a
! game that declares "verb <use>" owns USE outright: the built-in never sees it,
! the two-object "use X on Y" form goes with it (the verb matches "use " and
! Disambiguate is handed all the rest as one object name), and an object answers
! through "action <use>" or a "use" property instead.
!
! ExecVerb itself (V4Game.cs:2928-3070) resolves in that order too -- object
! action, then object property (printed), then the verb's own default script --
! and a declaration with no default at all is therefore a silent no-op for every
! object that does not override it.  Synonyms split on ";", and the action or
! property looked for on the object is whatever follows a ":" if the declaration
! has one, else the first name.
!
! geas used to check these declarations last, after the built-ins, so a game
! whose only winning move is a custom verb on an object could not be finished at
! all: Operation Rising Star's Card Reader carries "action <Use> playerwin" and
! USE CARD READER answered "You don't have that."  geas also ignored the ":"
! form, which both renamed the property and hid the synonyms behind it.
!
! Note that the properties here have to be written "properties <name=value>":
! Quest's object parser has no catch-all for a bare "name <value>" line, so an
! unrecognised tag is simply dropped, and "use <...>" in an object is use-info
! for USE X ON Y rather than a property (V4Game.Part2.cs:3406-3542).
define game <VerbFirst>
 asl-version <400>
 start <Room>

 verb <use> msg <You can't use that.>
 verb <poke;prod: pokeresult> msg <Poking that achieves nothing.>
 ! No default script: this makes EXAMINE silent, while the built-in X still works.
 verb <examine>
end define

define room <Room>
 look <A plain room.>

 ! A room command still beats the verb -- commands are tried first.
 command <use lever> msg <The lever clunks.>

 define object <widget>
  look <A widget.>
  take
  action <use> msg <You use the widget.>
 end define

 ! No action, but a property named after the verb: ExecVerb prints it.
 define object <rope>
  look <A rope.>
  properties <use=The rope is too short.>
 end define

 ! The room command above wins even though this property would have matched.
 define object <lever>
  look <A lever.>
  properties <use=The lever resists.>
 end define

 ! The property comes from after the ":", not from the verb name.
 define object <cushion>
  look <A cushion.>
  properties <pokeresult=The cushion wobbles.>
 end define

 define object <rock>
  look <A rock.>
  take
 end define
end define
