! A pronoun stands for the last object only while that object is still in
! scope and is the pronoun's kind: "him" for an object whose article is him,
! "her" likewise, and it/them/this/that/these/those for everything else
! (Disambiguate, V4Game.cs:4653-4695, 4773-4791).  Anything else is answered
! with BadPronoun, "I don't know what 'it' you are referring to.", once, and
! with nothing after it -- not the take, use, give or drop refusal the command
! would otherwise have worded.  From 4.10 a movement command forgets the
! referent as well.  "they" is not a pronoun.
!
! Question used to hand any of its pronouns the last object whatever it was
! and wherever it had got to, and with no last object looked for a thing
! called "it" and answered "I can't see that here."
define game <badpronoun>
 asl-version <410>
 start <r>
end define
define room <r>
 look <A room.>
 north <r2>
 define object <rock>
  look <A rock.>
  take
 end define
 define object <box>
  look <A box.>
 end define
 define object <Bob>
  look <A man.>
  article <him>
  gender <he>
 end define
 define object <Ann>
  look <A woman.>
  article <her>
  gender <she>
 end define
end define
define room <r2>
 look <Another room.>
 south <r>
end define
