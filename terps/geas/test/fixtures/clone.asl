' `clone <src; newname[; room]>`.
'
' ExecClone (V4Game.cs:4352-4400) appends a byte copy of the source object's
' record to Quest's object array, overwrites the copy's ObjectName with the second
' field and its ContainerRoom with the third, and refreshes the object list.  Two
' consequences follow from it being a copy of the *record*, and both are checked
' below:
'
'   - the clone starts out with the source's properties and actions as they stand
'     at that moment, not as the story file wrote them, so cloning a source that
'     has been changed during play copies the change;
'
'   - the two are separate records from then on, so changing one afterwards does
'     nothing to the other.
'
' With only two fields there is no ContainerRoom to write, and Quest leaves the
' copied one in place -- which is the source's *room*, since ContainerRoom is
' always a room even for an object sitting inside a container.
'
' geas had `clone` as a no-op, which quietly broke Barbarian (Wonderjudge,
' ASL 410): every purchase from Turner's market stall is a `clone <Tomato;
' Tomato%c%; Market>` followed by a `give`, so the player handed over the coins
' and received nothing, and the cabbage that tames the large horse is the first
' link in the game's opening task.  A clone here is an alias in the GeasFile
' (which supplies the definition: the look text, the actions, the take flag) plus
' a fresh runtime record, and because the GeasFile is rebuilt from the story file
' on load, the aliases are re-registered after a restore from a trail of
' properties on a "!clones" pseudo-object.

define game <Clone>
 asl-version <410>
 start <Market>

 command <here> clone <Tomato; Tomato3>
 command <stock> clone <Tomato; Tomato2; Larder>
 command <mark #thing#> property <#thing#; ripe>
 command <unmark #thing#> property <#thing#; not ripe>
 command <check #thing#> {
  if property <#thing#; ripe> then msg <#thing# is ripe.>
  else msg <#thing# is green.>
 }
end define

define room <Market>
 look <The market.>
 out <Larder>

 define object <Tomato>
  look <A round red tomato.>
  take
 end define
end define

define room <Larder>
 look <The larder.>
 out <Market>
end define
