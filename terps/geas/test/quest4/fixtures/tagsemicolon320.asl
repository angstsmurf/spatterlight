' A tag value keeps its semicolons until the version that made it a property.
'
' Quest reads `look', `prefix' and the rest into a field of the room or object
' record, and only then -- and only from the version that introduced it --
' pushes the same text through AddToObjectProperties, which splits a
' semicolon-separated list and so throws everything after the first item away
' (V4Game.cs:4011-4040).  An object's descriptive tags crossed over at 3.11
' (InitialiseObject, V4Game.Part2.cs:3271-3392) and every one of a room's at
' 3.50 (SetUpRoomData, ibid. 989-1200); below that the field is all there is,
' and the field has the whole value in it.  The container tags -- `open',
' `close', `add', `remove' -- were properties from the start, and print from
' the property, so they are cut at every version.
'
' `take' is the odd one out and is never cut: it writes both, and ExecTake
' prints the field (ibid. 5226-5228).  It is here at all three versions to
' hold that.
'
' geas rewrote every tag line into a property list as it read the file, so it
' truncated the lot.  Magic Sword (2.17) is the corpus game: its training room
' describes two exits in one `look', "...replicas of enemys; using weapons.|n
' And...", of which geas printed the first eight words and stopped.
'
' This is the version between the two gates: the object's `look' and `prefix'
' are cut, the room's are not.  tagsemicolon.asl is below both and
' tagsemicolon350.asl above both, on the same tags.
define game <tagsemicolon320>
    asl-version <320>
    start <Yard>
end define

define room <Yard>
    prefix <sunny; bright>
    look <alpha; beta>

    define object <rock>
        prefix <a grey; mossy>
        look <shiny; smooth>
        take <lifted; hefted>
    end define
end define
