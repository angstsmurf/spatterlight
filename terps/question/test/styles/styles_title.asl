' A centred bold 20pt title in the start script, then bold and 14pt text used
' separately in play.  The title is the heaviest state the game never
' shares with the natural table, so it takes Header; bold keeps Subheader and
' the 14pt state takes the next spare (BlockQuote).
define game <Styles Title>
 asl-version <410>
 start <Hall>
 startscript {
  msg <|jc|s20|bThe Great Title|xb|s00|jl>
  msg <An ordinary introduction in the game's own size.>
 }
end define

define room <Hall>
 look <A hall with |bbold words|xb in it, and |s14a larger sentence|s00 after.  Then |iitalic|xi and |uunderlined|xu text.>
end define
