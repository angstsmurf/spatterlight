# El asesino durmiente (Micky) — CandyVonBitter, 2016, ASL 550 (Quest 5.6).
# Spanish-language horror GAMEBOOK (Quest gamebook editor style, inlined
# GameBook Core): no parser, no verbs — each "command" is the object name of
# the destination page, exactly as authored (accented Spanish page names
# included). No published walkthrough exists — derived from game source.
#
# Ending: the richest of the two endings — Page1 -> Page2 (touch the empty
# pillow) -> Page5 (investigate the backyard, find the grave) -> Page6 ->
# Page7 -> "Aceptar bebidas" (accept the invitation; the protagonist
# "accepts" he is the killer) -> Page8 (FINAL: "Los sueños se hacen
# realidad.") -> the hidden epilogue page "Alguien en algún lado está
# anotando algo", which delivers the twist: a neighbour boy is the real
# sleeping-killer's puppeteer, and the protagonist was "subject 21". The
# alternate branch ("Negar bebidas" -> Page9) is shorter and has no epilogue.
#
# Note: the game NEVER calls finish — no finish() exists anywhere in the
# source, and the GameBook core's HandleCommand only understands page names
# (plus "undo"), so there is no quit-style command mapped to finish. The
# script ends parked on the terminal option-less epilogue page:
# state=Running steps=7 errors=0 is this game's genuine completed state.
Page2
Page5
Page6
Page7
Aceptar bebidas
Page8
Alguien en algún lado está anotando algo
