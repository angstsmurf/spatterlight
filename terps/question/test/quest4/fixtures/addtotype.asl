! A library's `!addto type <name>' section takes any type name.
!
! Quest's library loader knows three !addto forms, "game", "synonyms" and
! "type <name>" (V4Game.cs:1497-1660).  The type form is not limited to
! <default> and <defaultroom>: the name is lowercased and matched against the
! game's own "define type <name>" line, the section's lines are spliced in
! before that block's "end define", and when the game has no such block one is
! created.  Question used to refuse every other type name with "Bad addto" and
! drop the section, so a library could extend the built-in types but not its
! own.
!
! The lamp's type is the game's (Gadget, spelt in another case than the
! library's section); the box's type exists only through the library.
!include <addtotype_lib.asl>
define game <AddtoType>
 asl-version <410>
 start <Room>
 command <probe #thing#> {
  if property <#thing#; glow> then msg <#thing# glows.> else msg <#thing# does not glow.>
  if property <#thing#; hum> then msg <#thing# hums.> else msg <#thing# does not hum.>
  if property <#thing#; metal> then msg <#thing# is metal.> else msg <#thing# is not metal.>
 }
end define

define type <gadget>
 properties <metal>
end define

define room <Room>
 look <A room.>
 define object <lamp>
  look <A brass lamp.>
  type <gadget>
 end define
 define object <box>
  look <A plain box.>
  type <widget>
 end define
end define
