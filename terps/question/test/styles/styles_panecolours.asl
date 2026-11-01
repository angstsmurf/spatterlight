' A background and a foreground set in play reach the side pane as well as
' the page, as in Quest 4.1.5: the pane is painted in the palette as it is now
' on each redraw, and the main window gets its own colours back afterwards.
define game <Styles Pane Colours>
 asl-version <410>
 start <Room>
 background <black>
 foreground <white>
end define

define room <Room>
 look <A dark room with a lamp in it.>
 define object <lamp>
 end define
 command <paint> {
  background <blue>
  foreground <yellow>
  msg <The walls turn |crred|cb.>
 }
end define
