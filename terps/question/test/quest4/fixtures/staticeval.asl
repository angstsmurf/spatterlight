! Property values are "statically evaluated" as they are read: %numeric% and
! #string# variables are substituted, as is #object:property#.  The scan must
! start at the sigil (not one character past it) and must measure the pieces of
! an #obj:prop# reference correctly.
define game <StaticEval>
 asl-version <311>
 start <Room>

 define variable <score>
  type numeric
  value <42>
 end define

 define variable <hero>
  type string
  value <Rincewind>
 end define

 command <plaque> msg <plaque: $objectproperty(plaque;engraved)$>
 command <sign> msg <sign: $objectproperty(sign;copy)$>
 command <both> msg <both: $objectproperty(both;text)$>
end define

define room <Room>
 look <A room.>

 define object <urn>
  properties <label=Ming>
 end define

 define object <plaque>
  properties <engraved=%score% points for #hero#>
 end define

 define object <sign>
  properties <copy=The urn reads #urn:label#.>
 end define

 define object <both>
  properties <text=#hero# scored %score% by the #urn:label# urn>
 end define

end define
