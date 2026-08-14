! From ASL 3.91 on, Quest finishes building every parameter by evaluating any
! inline {expression} in it -- "msg <2 + 2 = {2+2}>" prints 4 -- with "{{" as the
! escape for a literal brace and "}}" collapsing to one on the way out
! (EvaluateInlineExpressions, V4Game.cs:2258-2320, called at the end of
! GetParameter, V4Game.cs:1902).  Games use it to do arithmetic on object
! properties, which "property <obj; name = value>" cannot otherwise do: YOU ARE A
! TIGER shrinks its whiskey with "property <jack_daniels; volume =
! { #jack_daniels:volume# - 10 }>" and its burrito the same way.
!
! geas stored the braces verbatim, so the property grew into
! "{ { 30 - 10 } - 10 }" and the "if is <#jack_daniels:volume#;0>" test that
! empties the bottle never fired.  See inlineexpr353.asl for the pre-3.91 side of
! the gate.
!
! Two details this also pins down:
!
!  * A brace with no operator in it is handed back as the element itself, trimmed
!    (ExpressionHandler splits on the operators and returns elements[1] when
!    there are none, V4Game.cs:3129-3240), so {  7  } prints 7 and not 7.0.
!  * "properties <volume = 30>" is the value 30, not " 30": Quest trims both sides
!    of the "=" (AddToObjectProperties, V4Game.cs:4021-4025).  geas trimmed the
!    name but not the value, which showed up as a stray space in front of every
!    printed property and, worse, inside the arithmetic above.
!
! The property is read back with no leading space -- that is the trim above --
! and the arithmetic in "shrink" empties the bottle in three goes instead of
! nesting braces forever.
!
! Where geas deliberately differs: Quest replaces the *whole* parameter with the
! string "<ERROR>" when an expression does not evaluate, and its arithmetic is not
! ours, so geas leaves a brace it cannot evaluate exactly as it found it rather
! than destroying the surrounding text.  That is the {not an expression} line.
define game <InlineExpr>
 asl-version <400>
 start <Room>

 command <shrink> property <bottle; volume = { #bottle:volume# - 10 }>
 command <sums> {
  msg <2 + 2 = {2+2}>
  msg <precedence: { 2 + 3 * 4 }>
  msg <parens: { (2 + 3) * 4 }>
  msg <negatives: { 3 - 10 }>
  msg <division: { 7 / 2 }>
  msg <no operator: {  7  }>
  msg <variable: { %counter% * 2 }>
  msg <literal brace: {{2+2}}>
  msg <left alone: {not an expression}>
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
  ! The scripted (action) form, as YOU ARE A TIGER uses it, so the description
  ! is rebuilt on every LOOK AT rather than frozen at load time -- see
  ! loadtimeprop.asl for the "look <...>" form and what it does instead.
  look msg <There is { #bottle:volume# } percent left, and '#bottle:volume#' has no stray space.>
 end define
end define
