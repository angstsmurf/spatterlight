' pictureshow.asl with the ASL version raised by one: at 2.82 the picture
' branch of the dispatch chain stops claiming the word `show'
' (V4Game.Part2.cs:5842), so the tag falls through to the branch that makes an
' object visible (ibid. 5904) and the lamp appears.  Nothing is printed for it:
' whatever followed the ';' was a picture caption and is now part of the name
' being looked up, and no object is called that.
define game <pictureshow282>
    asl-version <282>
    start <Gallery>
    command <reveal> show <lamp>
    command <report> msg <objects: #quest.formatobjects#>
end define

define room <Gallery>
    look <A gallery.>

    define object <lamp>
        prefix <a>
        hidden
    end define
end define
