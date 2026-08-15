' From ASL 3.91 a take is refused outright when the object sits somewhere the
' player cannot reach into.  ExecTake asks PlayerCanAccessObject before it does
' anything else and, on a no, prints badtake with the reason glued on
' (V4Game.Part2.cs:5176-5186, 7727-7795): the trailing full stop is dropped,
' " - inside closed " and the container's display name take its place.  geas
' had no gate at all, and Pyramid of Terror let the player reach into a shut
' goth and a shut cabinet of concoctions.
'
' The walk goes up the parent chain, and the test at each step is Quest's own
' pair -- surface, or opened.  Not "closed", and pointedly not "transparent":
' you can see into a glass jar and still not get your hand in.  Both boxes here
' are transparent, which is what makes their contents referrable at all, and
' neither is reachable.
'
' The pearl is the recursion: its own jar is open, so the walk carries on to
' the jar's box and refuses there.  The name in the refusal is the container's
' alias when it has one, which is why both refusals say "strongbox".
'
' The candle is the control.  A successful take out of an open container is
' deliberately not here: geas performs the implied remove that real Quest does
' and QuestViva does not (see the note in the take handler), so such a line
' would be measuring that difference instead of this one.  The two `x` lines
' still differ from Quest by the "It contains ..." line geas does not print
' (finding 12); everything else here is word for word.
'
' There is no sub-3.91 companion, and not for want of trying: below that
' version Quest stops sweeping container visibility altogether
' (UpdateVisibilityInContainers, V4Game.Part2.cs:7675-7678) while geas keeps
' sweeping, so the same game answers "I can't see that anywhere" here and lists
' every contained object there.  Nothing in such a transcript would be about
' the gate.
define game <TakeAccess>
 asl-version <410>
 start <Study>
end define

define room <Study>
 look <A study.>

 define object <box>
  alias <strongbox>
  look <A squat box.>
  container
  properties <transparent>
 end define

 define object <jar>
  look <A glass jar.>
  container
  properties <transparent; opened>
  parent <box>
 end define

 define object <pearl>
  look <A pearl.>
  parent <jar>
  take
 end define

 define object <coin>
  look <A coin.>
  parent <box>
  take
 end define

 define object <candle>
  look <A candle.>
  take
 end define
end define
