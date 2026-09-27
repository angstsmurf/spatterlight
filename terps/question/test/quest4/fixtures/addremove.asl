' Quest's four container *statements*, as opposed to the like-named verbs:
'
'   add <child; parent>     put an object into a container
'   remove <child>          take it back out again
'   open <container>        set the open state
'   close <container>       clear it
'
' All four are ASL >= 3.91 (ExecuteScript, V4Game.Part2.cs:5900-5915) and all
' four are silent: no message, no action or property lookup, no "is it even a
' container?" check.  That is the whole difference from OPEN CHEST typed by the
' player, which goes through ExecOpenClose and prints something.
'
' The visibility rule they all feed is UpdateVisibilityInContainers
' (V4Game.Part2.cs:7650): a container's contents are available only if the
' container is a surface, or is (open or transparent) AND *seen*.  So `open
' <chest>` on its own reveals nothing while the chest is still unseen -- which is
' the state a fresh game starts in -- and it takes a LOOK AT CHEST to finish the
' job.  From ASL 4.10 on, `add` also marks the parent seen, because otherwise you
' could put something in a container and then fail to LOOK AT it (Quest's own
' comment, DoAddRemove, V4Game.cs:2113); addremove391.asl is this same game one
' version down, where that does not happen.
'
' `remove` clears the "parent" property and leaves ContainerRoom alone, so the
' object ends up loose in whatever room the container was in -- here the pebble,
' which was never anywhere but inside the chest, becomes an ordinary Vault
' object.
'
' A malformed statement is logged and ignored, not fatal: `add <coin>` names no
' parent and `add <feather; chest>` names no object, and the room is unchanged
' after both.
'
' The corpus case is Londe Perplex (Kirk Gough, ASL 410), whose Sacred Guardians
' hand over the Discus with
'
'     add <Discus; Magic Box>  ...  open <Magic Box>  ...  give <Discus>
'
' Without the statements the Discus stayed hidden inside its declared parent for
' the whole game and the win was unreachable.

define game <Addremove>
 asl-version <410>
 start <Vault>
end define

define room <Vault>
 look <The vault.>

 define object <chest>
  container
  look <A sturdy chest.>
 end define

 define object <pebble>
  parent <chest>
 end define

 define object <coin>
 end define

 command <openit> open <chest>
 command <closeit> close <chest>
 command <stow> add <coin; chest>
 command <unstow> remove <pebble>
 command <noparent> add <coin>
 command <nosuchchild> add <feather; chest>
end define
