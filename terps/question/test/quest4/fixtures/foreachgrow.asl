! Loops whose body grows or overflows what they are walking.
!  - "for each object" ranged over state.objs while the body's clone pushed onto
!    it; once the vector reallocated the loop read freed memory (ASan:
!    heap-use-after-free).  Objects added during the loop are not visited.
!  - cloning to a name that differs from the source only in case appended to
!    the property list being copied, while it was being copied.
!  - "for <i; a; b>" with b == INT_MAX stepped past it, wrapped round to
!    INT_MIN and ran four billion times.
define game <ForEachGrow>
 asl-version <410>
 start <Room>
 define variable <n>
  type numeric
  value <0>
 end define
 command <all> for each object in game {
   inc <n>
   clone <apple; apple%n%; Room>
   msg <#quest.thing#>
 }
 command <here> for each object in <Room> {
   inc <n>
   clone <apple; pear%n%; Room>
   msg <#quest.thing#>
 }
 command <case> {
   property <apple; a=1>
   property <apple; b=2>
   property <apple; c=3>
   clone <apple; Apple>
   msg <cloned>
 }
 command <top> {
   for <i; 2147483645; 2147483647> msg <%i%>
   for <i; -2147483646; -2147483648; -1> msg <%i%>
   msg <counted>
 }
end define
define room <Room>
 look <A room.>
 define object <apple>
  properties <x=1; y=2; z=3>
 end define
 define object <b>
 end define
 define object <c>
 end define
 define object <d>
 end define
 define object <e>
 end define
end define
