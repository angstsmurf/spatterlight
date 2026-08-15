' An unrecognised "|" code is a literal bar.  Quest's TextFormatter tries the
' two-character codes, then the one-character ones, then |sNN, and when none of
' them matches it emits a "|" and carries on reading from the character right
' after the bar, so that character is printed as ordinary text
' (TextFormatter.cs:161-176).  geas fell off the end of its switch with the
' index already past the code character, and the loop's own i++ then skipped it,
' so a bar plus one character disappeared.
'
' The Lazy Gun Cult is the corpus case: |K| is its cult's sigil and |"..."| sets
' off a quoted page, and every one of them lost two characters a bar -- "a
' bronze |plaque| on the wall" came out as "a bronze laqueon the wall", the
' words run together because the second bar ate the space after it.
'
' Uranus opens with a stray |s9|, malformed because the size code wants two
' digits.  It wants two characters rather than two digits, though: Quest reads
' them with int.TryParse, whose default NumberStyles.Integer skips whitespace at
' either end, so "0 " and " 8" are sizes as well (finding 71).
' ChristmaKwanzakkah's title banner ends "|cr! |s0 |cb |xb" and a stricter
' reader printed the "|s0" as text on the game's first line.
'
' The bars that ARE codes are here as the control, including the two
' that never reach TextFormatter at all: Print handles |w and the clear-screen
' |c itself (V4Game.Part2.cs:6747-6779).  |cX -- a clear -- is deliberately not
' here: it agrees on the bar, and disagrees on what a cleared screen does to a
' captured transcript, which is the harness's business and not this finding's.
define game <barcodes>
 asl-version <410>
 start <Room>

 command <sigil>   msg <The letter |K| is etched on the head.>
 command <plaque>  msg <There is a bronze |plaque| on the wall beside you.>
 command <quoted>  msg <|"Tuesday IV Sept:>
 command <badsize> msg <|s9| and then some text.>
 command <goodsize> msg <|s16big|s10 back to normal.>
 command <spacesize> msg <|s0 and |s 8then|s10 some text.>
 command <badjust> msg <|jx not justified.>
 command <badx>    msg <|xz not a code, |xn neither in the middle.>
 command <trail>   msg <a bar at the very end |>
 command <colour>  msg <|crred|cb and |cggreen|cb back.>
 command <bold>    msg <|bbold|xb, |iitalic|xi, |uunder|xu.>
end define

define room <Room>
 look <A room.>
end define
