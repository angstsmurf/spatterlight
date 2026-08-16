' The `picture' family -- finding 31 in the oracle's FINDINGS.md.
'
' Quest 4 had two picture routines and one if/else-if chain to choose between
' them (V4Game.Part2.cs:5836-5858).  ShowPicture opened a separate picture
' window: it split an optional "; <caption>" off the filename and then an
' optional "@<width>x<height>" off what was left, and used the caption as the
' window's title bar -- "If a caption is specified, that caption is used for
' the window title, rather than the default 'Picture'" (Quest 4.1.5's own
' quest.chm, script-script.htm).  ShowPictureInTextAsync drew the image among
' the game text and split nothing at all.
'
' The chain, in order (spelled without angle brackets on purpose: Quest checks
' every line of the file, comments included, for a '<' with no '>' after it,
' and refuses to load the game if it finds one -- finding 34):
'
'   picture close                        nothing
'   picture popup FILE   3.90 and up     ShowPicture
'   picture FILE         2.82 to 3.89    ShowPicture
'   show FILE            below 2.82      ShowPicture  (see pictureshow.asl)
'   picture FILE         3.90 and up     ShowPictureInText
'   animate persist FILE                 ShowPicture
'   animate FILE                         ShowPicture
'
' So at 4.10, below, `picture <map.jpg; A map>' names a file called
' "map.jpg; A map" -- the plain form stopped splitting when `popup' arrived to
' carry the old syntax.  geas used to split '@' for every form and never split
' ';' for any, so a captioned tag reached the host with the caption still glued
' to the filename and no image could load: six corpus games write captions on
' twenty tags between them, among them Burglary, Defenders of Gondor and King's
' Quest V.
'
' Neither QuestViva nor geas has a picture window to title, both drawing the
' image among the text, so the caption is printed with the image instead
' (ibid. 3686) -- which is what this fixture can see, the harness having no
' graphics at all.  A tag whose caption is empty prints nothing.
define game <picture>
    asl-version <410>
    start <Gallery>
    command <plain> {
        picture <map.jpg>
        picture <sized.jpg@640x480>
        picture <glued.jpg; not a caption at 4.10>
    }
    command <popup> {
        picture popup <map.jpg; The Shire>
        picture popup <sized.jpg@640x480; A map of the Shire>
        picture popup <bare.jpg@640x480>
        picture popup <semis.jpg; one; two>
        picture popup <empty.jpg;>
    }
    command <animation> {
        animate <spin.gif; Spinning>
        animate persist <burn.gif; Burning>
    }
    command <shut> {
        picture close
        PICTURE CLOSE
    }
end define

define room <Gallery>
    look <A gallery.>
end define
