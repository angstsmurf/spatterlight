' A `type' line is applied where it stands, not before the object's own tags.
'
' Quest reads a define block one line at a time and applies each line as it
' comes to it (InitialiseObject, V4Game.Part2.cs:3378-3421).  A `type' line is
' a property source like any other: it pushes the whole type through the same
' AddToObjectProperties a `properties' line uses, and through AddObjectAction
' for the type's actions, and both of those overwrite an entry of the same name
' (V4Game.cs:4042-4069 and 3784-3816).  So the last writer wins, and there is no
' object-beats-type precedence at all -- a type named BELOW a tag beats it.
'
' geas used to resolve every inherited type first and then let the object's own
' lines override, whatever the source order.  For the usual type-first ordering
' that gives the same answer, which is why only one corpus game noticed:
' Operation: Sleepover puts a `prefix' above a clothing `type' eight times, and
' every garment kept an adjective Quest had thrown away -- "a black sweater" for
' Quest's "a sweater", "red panties" for "a pair of panties".
'
' Both objects here name the same type, on either side of the same three lines:
' a prefix, a plain assigned property, and an action.
define game <typeorder>
    asl-version <410>
    start <Shop>

    command <report> {
        msg <sweater prefix: #sweater:prefix#>
        msg <sweater colour: #sweater:colour#>
        msg <coat prefix: #coat:prefix#>
        msg <coat colour: #coat:colour#>
        doaction <sweater; rub>
        doaction <coat; rub>
    }
end define

define type <jacket>
    prefix = a
    colour = grey
    action <rub> msg <You rub the jacket type's script.>
end define

define room <Shop>
    look <A dim little shop.>

    ' Tags first, then the type: the type wins all three.
    define object <sweater>
        prefix <a black>
        properties <colour=black>
        action <rub> msg <You rub the sweater's own script.>
        type <jacket>
    end define

    ' The type first, then the tags: the tags win all three.
    define object <coat>
        type <jacket>
        prefix <a black>
        properties <colour=black>
        action <rub> msg <You rub the coat's own script.>
    end define
end define
