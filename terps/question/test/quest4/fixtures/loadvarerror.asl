! A definition-level "#n#" naming a numeric variable is resolved at load time,
! and the lookup throws on the type mismatch.  Nothing caught it short of
! set_game, which abandoned startup and left the whole game silent.  It is now
! reported and replaced by nothing, and play goes on.
define game <LoadVarError>
 asl-version <410>
 start <Lab>
 define variable <n>
  type numeric
  value <3>
 end define
end define
define room <Lab>
 look <A plain lab.>
 define object <rock>
  look <A #n# rock.>
 end define
end define
