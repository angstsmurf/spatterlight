! What a RESTORE (and UNDO) must and must not carry across.
!  - Numeric variables are doubles, but the save wrote them through int, so
!    n = 0.5 came back as 1.
!  - The undo history survived a RESTORE: each snapshot records a length of
!    the abandoned game's property log, so UNDO stitched the two games into a
!    state neither ever had (here: the abandoned "frac" kept, "paint" lost).
!  - A clone's definition alias outlived the clone itself across UNDO, so a
!    second clone of the same name answered with the first one's description.
!  - A save naming another game was accepted, dropping the player in rooms
!    this one does not define.
define game <SaveRestore>
 asl-version <410>
 start <Hall>
 command <frac> set numeric <n; 1 / 2>
 command <zero> set numeric <n; 0>
 command <shown> msg <n=%n%>
 command <paint> property <Box; color=red>
 command <colour> msg <Box is $objectproperty(Box; color)$>
 command <sword> clone <Sword; Thing>
 command <shield> clone <Shield; Thing>
end define
define variable <n>
 type numeric
 value <0>
end define
define room <Hall>
 look <The hall.>
 east <Attic>
 define object <Box>
  look <A box.>
 end define
 define object <Sword>
  look <A sharp sword.>
 end define
 define object <Shield>
  look <A round shield.>
 end define
end define
define room <Attic>
 look <The attic.>
 west <Hall>
end define
