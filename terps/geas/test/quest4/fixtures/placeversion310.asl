' A `place' is named by its destination room's alias only from ASL 3.11 up.
' Finding 53 -- and finding 23, which is the same thing found from the other
' end -- in the oracle's FINDINGS.md.  The no-script half of the same condition
' is placealias.asl's business; this pair is the version half.
'
' Both halves are one condition in Quest, and it guards the listing and the name
' the player has to type alike:
'
'     if ((ASLVersion >= 311) & string.IsNullOrEmpty(Places[i].Script))
'         shownPlaceName = _rooms[GetRoomID(Places[i].PlaceName, ctx)].RoomAlias
'     else
'         shownPlaceName = Places[i].PlaceName
'
' -- GetGoToExits, V4Game.Part2.cs:7790-7818, and PlaceExist, ibid. 6568-6594.
' geas had the script half and not the version half, so it aliased everything.
'
' The version half is not cosmetic.  An author who aliases a room and then
' writes `place <the room's real name>' has, below 3.11, written a place the
' player must type by the real name -- and geas advertising the alias made GO TO
' reject its own listing.  "Space" (ASL 2.10) is the case that matters: its
' <Science Lab> is a room its author aliased "Sciece Lab", and geas walked the
' replay into a room Quest never opens.  The Broken Mirror (3.10) is the same
' story with <THE ENGLISH> and "The english pub".
'
' This is placeversion.asl with the version line changed and nothing else: 3.10
' is below the cutoff, so <Cellar> is listed and entered as "Cellar" here.
' The room the player is *in* is unaffected at either version: an alias always
' names that one, which is why every arrival below still says "Basement".
define game <placeversion310>
    asl-version <310>
    start <Hall>
end define

define room <Hall>
    look <A hall.>

    ' Plain: aliased from 3.11 up.
    place <Cellar>

    ' Scripted: named by the tag at every version.
    place <Attic> {
        msg <You haul yourself up.>
        goto <Attic>
    }
end define

define room <Cellar>
    alias <Basement>
    look <Damp.>
    place <Hall>
end define

define room <Attic>
    alias <Loft>
    look <Dusty.>
    place <Hall>
end define
