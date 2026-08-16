! An `action <name>` with nothing after its closing bracket is thrown away.
! AddToObjectActions takes the first closing bracket in the action data and,
! if it is the last character, logs "No script given for '<name>' action data"
! and returns (V4Game.cs:3934-3939) -- the loader has already trimmed the line
! (V4Game.cs:1335), so that test and "no script" are the same test.  geas
! registered the action anyway, the action lookup found it, and the verb printed
! nothing at all.
!
! What the empty action shadows is the object's like-named `properties
! <verb = ...>` text, which is what ExecVerb falls back on.  *Sutekh Is Hiding
! In Your Priory* is the corpus case: a `watch` property, an empty
! `action <watch>`, and a `switch off` action that rewrites the property -- so
! under geas the whole rewrite mechanism was invisible.
!
! The wardrobe is the control: a `wear` action with a real script still wins
! over the property, on both engines.  The runtime `action <obj; name>`
! statement is deliberately not covered here -- ExecuteAction has the opposite
! rule and stores the empty script (V4Game.cs:5063-5071).
define game <emptyact>
 asl-version <410>
 start <Lounge>
 verb <watch> msg <You can't watch that.>
 verb <switch off> msg <Nothing to switch off.>
 verb <wear> msg <You can't wear that.>
end define

define room <Lounge>
 define object <television>
  look <A vintage viewer.>
  properties <watch = A woman in a white dress paces about a priory garden.>
  action <watch>
  action <switch off> {
   property <television; watch = A single dot, fading.>
   msg <You switch off the vintage viewer.>
  }
 end define

 define object <wardrobe>
  look <A wardrobe.>
  properties <wear = You are wearing the wardrobe. Somehow.>
  action <wear> msg <The wardrobe will not go on over your coat.>
 end define
end define
