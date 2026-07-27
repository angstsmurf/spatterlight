' Quest 2.x's "name@room" way of naming an object.
'
' SetAvailability -- which is what hide/show/hideobject/showobject/hidechar/
' showchar all end up in -- treats its argument two different ways depending on
' the version (V4Game.Part2.cs:2994-3068).  At ASL 281 and up the whole string is
' compared to the object's name, and anything with an "@" in it just is not found.
' Below 281:
'
'     atPos = InStr(thingString, "@")
'     if atPos = 0 then
'         room = _currentRoom : name = thingString
'     else
'         name = Trim(Left(thingString, atPos - 1))
'         room = Trim(Right(thingString, Len(thingString) - atPos))
'     ...
'     for i = 1 to _numberObjs
'         if LCase(_objs[i].ContainerRoom) = LCase(room) and
'            LCase(_objs[i].ObjectName)   = LCase(name) then ...
'
' -- so the reference is (name, room), the room half is matched against where the
' object actually is, and naming a room the object is not in finds nothing and
' changes nothing.  It does not matter where the *player* is: "showlinda" below is
' typed in the study and reveals a character in the hall.
'
' 2.x games lean on this because "give" in 2.x adds to the item list rather than
' moving a world object, so the idiom for picking something up is a pair:
'
'     give <book>
'     hideobject <book@#quest.currentroom#>
'
' geas passed the whole "book@Study" string through as an object name, matched
' nothing, and hid nothing -- so in "Venus flytrap: Romantic Music" (musicvf1.cas,
' ASL 210) every object you took stayed lying in the room as well, and its two
' characters, both revealed with `showchar <X@room>`, never appeared at all.
'
' geas keys properties by object name alone, so what it does with the split is
' decide whether the reference resolves and then use the bare name.  The case with
' no "@" at all is knowingly left as it was: Quest below 281 presumes the current
' room there, where geas looks for the name wherever it is.

define game <Atroom>
 asl-version <210>
 start <Study>
 items <book>
 startitems <>
 command <get book> do <takebook>
 command <hide far book> do <hidefar>
 command <showlinda> do <showher>
end define

define room <Study>
 description <You are in the study.>
 place <the; Hall>

 define object <book>
  look <A slim volume.>
 end define
end define

define room <Hall>
 description <You are in the hall.>
 place <the; Study>

 define character <Linda>
  hidden
  gender <she>
  look <The lodger.>
 end define
end define

define procedure <takebook>
 give <book>
 hideobject <book@#quest.currentroom#>
 msg <You take the book.>
end define

define procedure <hidefar>
 hideobject <book@Hall>
 msg <Tried to hide the book in the hall.>
end define

define procedure <showher>
 showchar <Linda@Hall>
 msg <Tried to reveal Linda.>
end define
