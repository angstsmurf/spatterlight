! Conditions.  The preprocessor rewrites a parenthesised comparison such as
! "if (%score% <= 5)" into the canonical "if is <%score%;lt=;5>", so this pins
! down both halves of that contract: every operator, string and numeric, plus
! the "and"/"or" joiners.
define game <Compare>
 asl-version <311>
 start <Room>

 define variable <score>
  type numeric
  value <10>
 end define

 define variable <name>
  type string
  value <Bob>
 end define

 command <le> if (%score% <= 5) then msg <le: yes> else msg <le: no>
 command <leq> if (%score% <= 10) then msg <leq: yes> else msg <leq: no>
 command <ge> if (%score% >= 20) then msg <ge: yes> else msg <ge: no>
 command <geq> if (%score% >= 10) then msg <geq: yes> else msg <geq: no>
 command <lt> if (%score% < 20) then msg <lt: yes> else msg <lt: no>
 command <gt> if (%score% > 20) then msg <gt: yes> else msg <gt: no>
 command <ne> if (#name# != Bob) then msg <ne: yes> else msg <ne: no>
 command <nediff> if (#name# != Alice) then msg <nediff: yes> else msg <nediff: no>
 command <ltgt> if (#name# <> Alice) then msg <ltgt: yes> else msg <ltgt: no>
 command <eq> if (#name# = Bob) then msg <eq: yes> else msg <eq: no>
 command <isequal> if is <#name#;Bob> then msg <isequal: yes> else msg <isequal: no>
 command <both> if (%score% > 5) and (%score% < 20) then msg <both: yes> else msg <both: no>
 command <either> if (%score% < 5) or (%score% = 10) then msg <either: yes> else msg <either: no>
end define

define room <Room>
 look <A room.>
end define
