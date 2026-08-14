! How a condition list's "and"/"or" joiners are folded.  eval_conds ports
! Quest's ExecuteConditions (LegacyGame V4Game.cs), which splits the list at
! its joiners and folds LEFT to right, each condition combined with the joiner
! that precedes it -- so "A and B or C" is "(A and B) or C", not the
! "A and (B or C)" the old right-recursive code produced.
!
! It also keeps one quirk of the original: at each split Quest looks for the
! next "and" anywhere ahead and only falls back to "or" when there is none, so
! an "or" standing before a later "and" is swallowed into the preceding
! condition's text and never evaluated -- "A or B and C" means "A and C".
! Games were written against that engine, so orand below is a no even though
! both textbook readings of the expression say yes.
define game <CondJoin>
 asl-version <311>
 start <Room>

 define variable <score>
  type numeric
  value <10>
 end define

 ! false and true or true: left fold -> (F and T) or T -> yes
 command <andor>   if (%score% = 99) and (%score% = 10) or (%score% = 10) then msg <andor: yes> else msg <andor: no>
 ! true and false or false: left fold -> (T and F) or F -> no
 command <andor2>  if (%score% = 10) and (%score% = 99) or (%score% = 99) then msg <andor2: yes> else msg <andor2: no>
 ! false or true and true: the "or true" is swallowed -> F and T -> no
 command <orand>   if (%score% = 99) or (%score% = 10) and (%score% = 10) then msg <orand: yes> else msg <orand: no>
 ! uniform joiners are unaffected by the grouping
 command <purand>  if (%score% = 10) and (%score% = 10) then msg <purand: yes> else msg <purand: no>
 command <purand2> if (%score% = 10) and (%score% = 99) then msg <purand2: yes> else msg <purand2: no>
 command <puror>   if (%score% = 99) or (%score% = 10) then msg <puror: yes> else msg <puror: no>
 command <puror2>  if (%score% = 99) or (%score% = 98) then msg <puror2: yes> else msg <puror2: no>
 ! three of a kind, to pin the fold across more than one joiner
 command <and3>    if (%score% = 10) and (%score% = 10) and (%score% = 99) then msg <and3: yes> else msg <and3: no>
 command <or3>     if (%score% = 99) and (%score% = 98) or (%score% = 97) or (%score% = 10) then msg <or3: yes> else msg <or3: no>
 ! a joiner word inside a <parameter> is not a joiner
 command <inparam> if is <and or; and or> then msg <inparam: yes> else msg <inparam: no>
end define

define room <Room>
 look <A room.>
end define
