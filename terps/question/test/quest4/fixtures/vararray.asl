! Array subscripts on variables: "arr[3]" and "arr[n]", for both string and
! numeric variables.  All five accessors (set_svar/get_svar, set_ivar/get_ivar,
! get_dvar) share one parser, split_var_index.
!
! The bad-subscript commands are the point of the fixture: a subscript that is
! neither a literal nor a defined variable used to resolve to get_ivar's -32767
! "no such variable" sentinel, which as a size_t asked the record to resize to
! ~1.8e19 elements -- an uncaught std::length_error that aborted the whole
! interpreter mid-turn.  They print nothing (the diagnostic goes to
! debug_print, which the harness swallows); what they guard is that the game is
! still running afterwards, which the commands after them show.
define game <VarArray>
 asl-version <311>
 start <Room>

 define variable <num>
  type numeric
  value <0>
 end define

 define variable <str>
  type string
  value <zero>
 end define

 define variable <idx>
  type numeric
  value <2>
 end define

 command <setnum>   set numeric <num[3]; 42>
 command <getnum>   msg <num3 is %num[3]%>
 command <setvia>   set numeric <num[idx]; 17>
 command <getvia>   msg <num2 is %num[idx]%, num0 is %num%>
 command <setstr>   set string <str[1]; one>
 command <getstr>   msg <str1 is #str[1]#, str0 is #str#>

 command <badnum>   set numeric <num[nosuchvar]; 7>
 command <badstr>   set string <str[nosuchvar]; boom>
 command <negnum>   set numeric <num[-1]; 7>
 command <readbad>  msg <bad read is %num[nosuchvar]%>
 command <unclosed> set numeric <num[3; 7>

 command <alive>    msg <still running>
end define

define room <Room>
 look <A room.>
end define
