! "speak to <character>" resolves the same way Quest's ExecSpeak does: look for
! a `speak` *action* first, then for a `speak` *property*, and only then print
! the default "<gender> says nothing."
!
! The property step used to be missing, so the ordinary
!
!     speak <He jests with you.>
!
! form -- which the loader stores as a property, not an action -- printed
! "He says nothing." instead.  That silently muted the dialogue of nearly every
! character in every corpus game (all of Red Sauce Monday's cast bar one, and
! Marlon/Woodhouse/the fruit-machine woman in Sir Loin).  It looks like authored
! silence, which is why it survived so long: no game crashes, no walkthrough
! fails, the text just never appears.
!
! Also pinned here: the action form still wins over the property form when an
! object defines both, `talk to` is a synonym for `speak to`, and the default
! message takes its pronoun from the object's gender.
define game <Speak>
 asl-version <410>
 start <Room>
end define

define room <Room>
 look <A room.>

 ! property form -- the one that used to be lost
 define object <james>
  speak <He jests with you.>
  gender <he>
 end define

 ! action form (`speak msg <...>` is stored as an action)
 define object <tom>
  speak msg <He replies, "Hello!">
  gender <he>
 end define

 ! both: the action must win
 define object <amy>
  speak msg <The action ran.>
  properties <speak = The property ran.>
  gender <she>
 end define

 ! neither: default message, gendered
 define object <selena>
  look <Nothing to say.>
  gender <she>
 end define

end define
