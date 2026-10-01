! What a RESTORE and an UNDO must carry across.
!  - Numeric variables are doubles, but the save wrote them through int, so
!    n = 0.5 came back as 1.
!  - UNDO after a RESTORE has to go back to the game the restore abandoned.
!    The undo snapshots record lengths of the property log, which the
!    restore replaced, so UNDO used to stitch the two games together into a
!    state neither ever had (here: the abandoned "frac" kept, "paint" lost).
!  - A clone's definition alias outlived the clone itself across UNDO, so a
!    second clone of the same name answered with the first one's description.
!  - A save this game could not have written (here: one standing in a room
!    the game does not define) has to be turned away, leaving the game as it
!    was.
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
