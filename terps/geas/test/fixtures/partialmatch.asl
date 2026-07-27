! Below ASL 391 Quest refuses a word taken from the middle of a name: the loose
! second pass in Disambiguate is gated on `ASLVersion >= 391' as well as on
! `abbreviations' (V4Game.cs:4724-4767), so "rose" never reaches `Red Rose'.
!
! Geas runs that pass below 391 too -- half the corpus predates 391 and those
! games are full of nouns Quest simply refuses -- but accepts it only when it
! names exactly one object.  So this fixture pins both halves at once:
!
!   * "rose" resolves, because `Red Rose' is the only object it can mean.
!   * "key" does not, because `Brass Key' and `Rusty Key' both match: an
!     ambiguous loose match is dropped rather than put to the player, since
!     Quest found nothing here and the answer has to stay "nothing".
!   * "stone" reaches the pebble, not `Standing Stone': the exact pass runs
!     first and a name that matches exactly is never put up against a partial.
!
! partialmatch400.asl is the same game above the gate, where "key" gets the
! disambiguation menu instead.
define game <partialmatch>
    asl-version <300>
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
