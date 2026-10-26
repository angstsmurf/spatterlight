' More size states than spare styles.  The title and the heaviest sizes get
' the spares; the lightest fall back to the nearest assigned style.
define game <Styles Overflow>
 asl-version <410>
 start <Gallery>
 startscript msg <|jc|s20|bOverflow|xb|s00|jl>
end define

define room <Gallery>
 look <|s06Six point, a short line.|s00 |s08Eight point text that runs on for a while longer.|s00 |s12Twelve.|s00 |s14Fourteen point, which is written out at some length here.|s00 |s16Sixteen point text, also fairly long to weigh it up.|s00 |s18Eighteen point text that is the longest of them all, by quite some way, honestly.|s00 |s22Twenty-two point text of medium length here.|s00>
end define
