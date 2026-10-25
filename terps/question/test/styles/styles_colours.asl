' The game's own colours: a dark `define game' palette turns the colour mode
' on from the first window (the header rule), so the Normal hints carry it
' and every span is named in the game's foreground over its background, a
' |c colour in that colour.  COLOURS OFF goes back to the theme and COLOURS
' ON to the game's palette again, each with the windows rebuilt.
define game <Styles Colours>
 asl-version <410>
 start <Room>
 background <black>
 foreground <white>
 startscript msg <White on black, then |cyyellow|cb and back.>
end define

define room <Room>
 look <A dark room, described in white.>
end define
