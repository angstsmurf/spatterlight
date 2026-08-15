' A place can be locked, and its lock message is the second field.
'
' AddExitFromTag strips the direction word first and the `locked' keyword after
' it, and `place ' is one of the direction words it strips (RoomExits.cs:
' 120-149), so `place locked <Vault; The vault is sealed.>' is an ordinary place
' exit to Vault that starts locked and refuses with the author's sentence.  With
' no second field the refusal is the game's own `locked' error message.
'
' Locking gates the crossing and nothing else: RoomExit.Go tests it and then
' either prints or goes (ibid. 240-259), while the exits line lists every exit
' the room has, locked or not.  So the lobby below advertises all three of its
' places from the first turn.
'
' geas read the token after `place', found the bare word `locked' where it
' wanted a <...>, and dropped the whole line: the exit was neither listed nor
' reachable and the author's message was unreachable with it.  Skate Ur Ass Off
' is the corpus game, with one such line in its Skate Lobby.
'
' `lock' and `unlock' reach a place by its destination room's name -- that is
' the key FindExit looks it up under (V4Game.Part2.cs:7877-7913) -- which is
' what the `k' and `u' commands here are for.  All of this is 4.10 machinery:
' below that a room's places are plain records with no lock in them, and the
' loader walks straight past the keyword into the older `prefix; dest' reading
' of the parameter.
define game <placelocked>
    asl-version <410>
    start <Lobby>

    command <k> lock <Lobby; Shop>
    command <u> unlock <Lobby; Vault>
end define

define room <Lobby>
    look <A bare lobby.>
    place <Shop>
    place locked <Vault; The vault is sealed.>
    place locked <Cellar>
end define

define room <Shop>
    look <Rows of empty shelves.>
end define

define room <Vault>
    look <Stacks of bullion.>
end define

define room <Cellar>
    look <Cold and dark.>
end define
