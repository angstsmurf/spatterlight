! "for <var; from; to [; step]>" -- the end bound is inclusive, the step may be
! negative, and a malformed "for" (too few arguments) must be reported and
! skipped rather than read past the end of its argument list.
define game <Loops>
 asl-version <311>
 start <Room>

 command <up> msg <up: $countup$>
 command <down> msg <down: $countdown$>
 command <single> msg <single: $single$>
 command <step2> msg <step2: $bytwo$>
 command <badfor> {
  msg <badfor: before>
  do <shortfor>
  msg <badfor: survived>
 }
end define

define room <Room>
 look <A room.>
end define

define function <countup>
 setstring <out;>
 for <i; 1; 3; 1> {
  setstring <out;#out#%i%,>
 }
 return <#out#>
end define

define function <countdown>
 setstring <out;>
 for <i; 3; 1; -1> {
  setstring <out;#out#%i%,>
 }
 return <#out#>
end define

! from == to: runs exactly once
define function <single>
 setstring <out;>
 for <i; 2; 2; 1> {
  setstring <out;#out#%i%,>
 }
 return <#out#>
end define

! a step that overshoots the (inclusive) end bound
define function <bytwo>
 setstring <out;>
 for <i; 1; 6; 2> {
  setstring <out;#out#%i%,>
 }
 return <#out#>
end define

define procedure <shortfor>
 for <k; 5> {
  msg <BODY-RAN>
 }
end define
