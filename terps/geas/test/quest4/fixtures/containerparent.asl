' Containment is a *property* in Quest, not just a link.
'
' DoAddRemove (V4Game.cs:2113-2133) records "X is inside Y" by writing Y's
' definition name into X's own "parent" property, and clears it again with
' "not parent".  That is observable from ASL: games read it back as
' #child:parent#, and two puzzles in Barbarian (Wonderjudge, ASL 410) are built
' on nothing else.  The crypt pedestal will only trade the Eye of the East for
' the sack you offer it while
'
'     is <#Pile of bones:parent#; Sack>
'
' holds, and springs a lethal trap otherwise; and the hedge-maze well winch is
' assembled in four stages, each of which re-parents the handle to the next well
' object and is gated on where the handle currently is.  geas kept containment
' only in ObjectRecord::parent, which no property lookup can see, so every one of
' those checks read back empty and both puzzles were unsolvable.
'
' Three code paths write the property, and they are all exercised below: the
' `add` statement, the `remove` statement, and the PUT X IN Y verb's default
' containment, which is the same DoAddRemove call reached from the parser.  Note
' that `add` writes the container's *definition* name, not its alias.
'
' TAKE never writes the property itself, and the bones stay in the sack when
' the player tries to take them out of it -- which is why the pedestal goes on
' accepting the offering.  Barbarian needs that leg of it too: its bones come
' out of the sack and go back in over the course of the game, and the pedestal
' has to keep recognising them.
'
' What stops the take here is the *inventory* rule rather than the container's:
' the sack is being carried, so from 3.91 on everything in it is in the
' inventory scope too, and from 4.10 a take of something already there answers
' alreadytaken before any script runs (ExecTake, V4Game.Part2.cs:5143-5153).
' Drop the sack first and the other mechanism takes over: a text/default take of
' a parented object announces the implied step, runs the REMOVE command on the
' player's behalf and gives up if the item is still parented afterwards
' (ibid. 5178-5204).  See takefromcontainer for that one on its own.
'
' Quest's MoveThing -- the `move <obj; room>` statement -- leaves the property
' alone too, so `stash` relocates the bones without disturbing what they claim to
' be inside of.
'
' The marble is there for the fourth writer of the property, which is the file
' loader.  A definition-level `parent <crate>` line does not merely nest the
' object; Quest rewrites it as the property "parent=crate" as it reads the
' definition (AddToObjectProperties, V4Game.Part2.cs:3538-3540), so an object
' that begins the game inside a container reads back the same way as one that was
' put there by `add`.  geas kept that line only in ObjectRecord::parent.
'
' The crate is also the other half of the `remove` story, and that half is
' faithful rather than broken.  `remove` clears the parent property and does
' nothing else: it does not restore the object's availability, and the sweep it
' then runs skips any object whose parent property is empty (the
' `if (!string.IsNullOrEmpty (parent))` guard in UpdateVisibilityInContainers,
' V4Game.Part2.cs:7650-7708).  So something removed from a container that was
' never opened and never seen stays invisible, and only an explicit `show` brings
' it back.

define game <Containerparent>
 asl-version <410>
 start <Vault>

 command <show> msg <parent of bones is [#bones:parent#]>
 command <openit> open <chest>
 command <opensack> open <sack>
 command <stow> add <bones; chest>
 command <unstow> remove <bones>
 command <stash> move <bones; Vault>
 command <marbleparent> msg <parent of marble is [#marble:parent#]>
 command <freemarble> remove <marble>
 command <revealmarble> show <marble>
end define

define room <Vault>
 look <The vault.>

 define object <chest>
  container
  look <A sturdy chest.>
 end define

 define object <sack>
  container
  take
  look <A cloth sack.>
 end define

 define object <bones>
  take
  look <A pile of bones.>
 end define

 define object <pedestal>
  look <A stone pedestal.>
  use <sack> {
   if is <#bones:parent#; sack> then msg <The pedestal accepts the offering.>
   else msg <The pedestal springs a trap!>
  }
 end define

 define object <crate>
  container
  look <A nailed-shut crate.>
 end define

 define object <marble>
  parent <crate>
  take
 end define
end define
