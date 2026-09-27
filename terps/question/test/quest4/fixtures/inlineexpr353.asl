! The other side of the gate in inlineexpr.asl: EvaluateInlineExpressions
! returns its argument untouched below ASL 3.91 (V4Game.cs:2262-2265), and the
! "}}" -> "}" pass at its end is inside the same function, so a pre-3.91 game
! sees every brace exactly as it typed it -- including a "property <obj; name =
! { ... }>" whose value stays the literal text.
!
! Same game as inlineexpr.asl, one line different: asl-version <350>.
define game <InlineExpr353>
 asl-version <350>
 start <Room>

 command <shrink> property <bottle; volume = { #bottle:volume# - 10 }>
 command <sums> {
  msg <2 + 2 = {2+2}>
  msg <no operator: {  7  }>
  msg <literal brace: {{2+2}}>
 }

 define variable <counter>
  type numeric
  value <21>
 end define
end define

define room <Room>
 look <A plain room.>

 define object <bottle>
  properties <volume = 30>
  look msg <There is { #bottle:volume# } percent left.>
 end define
end define
