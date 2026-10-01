! Prose in a text block that starts with the word "define".  The block scan
! counted "Define your terms" as a block header, the text's "end define" closed
! only one of the two, and every block after it -- the room here -- was never
! read.  A lower-case "define" line likewise misaligned the blocks the
! preprocessor owes !addto lines to.
define game <TextDefine>
 asl-version <410>
 start <Lab>
end define
define text <intro>
Define your terms carefully.
define them twice.
end define
!addto game
 command <ping> msg <Pong.>
!end
define room <Lab>
 look <A plain lab.>
end define
