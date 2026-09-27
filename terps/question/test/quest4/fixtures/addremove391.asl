' addremove.asl one ASL version down.  Everything about `add`, `remove`, `open`
' and `close` is the same at 3.91 except the one line DoAddRemove gates on 4.10:
' `add` does not mark the parent "seen".  So here STOW puts the coin into a chest
' that has never been looked at, UpdateVisibilityInContainers finds the parent
' unseen, and the coin disappears -- an unopened-looking chest -- until LOOK AT
' CHEST supplies the missing "seen".  Under addremove.asl (4.10) the same STOW
' leaves the coin plainly visible.
'
' The command list, the objects and the .cmd are identical to addremove's, so the
' two .expected files diff to exactly that difference.

define game <Addremove391>
 asl-version <391>
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
