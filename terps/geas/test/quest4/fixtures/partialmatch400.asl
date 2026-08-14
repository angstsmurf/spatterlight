! The other side of the gate that partialmatch.asl pins.  Same game, above 391,
! where the loose pass is Quest's own behaviour and an ambiguous match is put to
! the player as a disambiguation menu -- so "key" asks which key is meant, and
! the script answers with a number.  Everything else resolves identically.

define game <partialmatch400>
    asl-version <400>
    start <Hall>
end define

define room <Hall>
    define object <Red Rose>
        look <A single red rose.>
    end define

    define object <Brass Key>
        look <A brass key.>
    end define

    define object <Rusty Key>
        look <A rusty key.>
    end define

    define object <Standing Stone>
        look <A tall standing stone.>
    end define

    define object <stone>
        look <A pebble.>
    end define
end define
