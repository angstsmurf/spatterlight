! A keyword whose script is missing because it is the last token on its line:
! "startscript", "action <obj; name>" and a variable's "onchange" all took
! substr(c2 + 1) with c2 == length(), and the std::out_of_range it threw is
! caught nowhere, so the interpreter aborted.
define game <BareKeyword>
 asl-version <410>
 start <Room>
 startscript
 command <act> {
   action <apple; eat>
   msg <acted>
 }
 command <change> {
   set string <v; b>
   msg <changed>
 }
end define
define variable <v>
 type string
 value <a>
 onchange
end define
define room <Room>
 look <A room.>
 define object <apple>
 end define
end define
