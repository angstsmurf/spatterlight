' Below ASL 2.82, `show <...>' is the picture statement -- the other half of
' finding 31; see picture.asl for the whole dispatch chain.
'
' The picture branch of the chain claims the word `show' below 2.82
' (V4Game.Part2.cs:5842) and sits above the branch that makes an object
' visible, whose own gate reads >= 2.81 (ibid. 5904).  So there is no version
' at which `show <object>' does both: here the lamp stays hidden and a picture
' called "lamp" is asked for -- with "the brass one" for its caption, which is
' how the fixture can tell that the tag went to the picture routine at all, the
' harness having no graphics.  A game this old that wants an object back says
' `showobject' or `showchar' instead (ibid. 5866, 5878).
'
' The companion fixture pictureshow282.asl is this file with the version raised
' by one, where `show' goes back to being an object statement.
define game <pictureshow>
    asl-version <281>
    start <Gallery>
    command <reveal> show <lamp; the brass one>
    command <report> msg <objects: #quest.formatobjects#>
end define

define room <Gallery>
    look <A gallery.>

    define object <lamp>
        prefix <a>
        hidden
    end define
end define
