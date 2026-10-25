' Six size states, all heavier than the only centred text, would take all six
' spares (Header, BlockQuote, Alert, User2, Note, User1), so nothing would
' carry centre.  The alignment post-pass gives User1 back to plain centre (the
' centred italic line weighs more than a quarter of the bold 14pt state it
' evicts, which falls back to Subheader), and the line lands there, dropping
' only the italic.
define game <Styles Aligned>
 asl-version <410>
 start <Court>
end define

define room <Court>
 look <|s14Fourteen point words here.|s00 |s16Sixteen point words here.|s00 |s18Eighteen point words go.|s00 |s20Twenty point words here.|s00 |s22Twenty-two point here.|s00 |b|s14Bold fourteen point.|xb|s00 |jc|iCentred italic.|xi|jl>
end define
