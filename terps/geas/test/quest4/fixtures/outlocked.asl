! A "locked" exit refuses to be used whatever else it carries.  RoomExit.Go
! tests GetIsLocked() first and only then looks for a script or a destination
! (RoomExit.cs:240-259), and the "locked " keyword is stripped by the same
! parser for every exit tag -- "out" and the eight compass directions alike
! (RoomExits.cs:143-147).  geas checked the lock only in its directional loop,
! and only for exits with no script, so:
!
!   * "out locked <...>" walked straight through -- Exits of The World's one
!     locked exit is the ship gate that keeps the player off Skarro, so the
!     whole game could be skipped;
!   * a locked exit with a script ran the script instead of refusing.
!
! The parameter of a *scripted* directional exit is its lock message, not a
! destination (RoomExits.cs:186-191) -- the Cellar door below would otherwise
! try to move the player to a room named after the refusal text.
!
! With no lockmessage at all the exit falls back to PlayerError.Locked, "The
! exit is locked." (V4Game.Part2.cs:452), which a game can override with
! "error <locked; ...>" -- Locked is error type "locked" (ibid. 1584-1586).
!
! ASL 4.10, because exit objects are all there is to lock: before 4.10 a room's
! exits are plain fields on the room, "locked" is not a keyword the loader knows
! and "unlock" has no object to reach.  This fixture was written at 400 by
! mistake and Quest refused nothing in it -- it answered each `unlock' with "[An
! internal error occurred]" and let the player walk out through the ship gate,
! and the `out' line advertised "You can go out to Yard The guard shakes his
! head." because the whole parameter went into Out.Text (see roominfov2exits and
! finding 74).  geas still locks pre-4.10 exits; no corpus game declares one.
define game <OutLocked>
 asl-version <410>
 start <Cell>
 error <locked; The door does not budge.>
end define

define room <Cell>
 look <A cell.  Doors lead out, north, south and down.>
 out locked <Yard; The guard shakes his head.>
 north locked <Corridor>
 south locked <You hear voices behind the door.> msg <You slip into the vestry.>
 down locked msg <You climb down into the cellar.>
 command <bribe> unlock <Cell; out>
 command <shove> unlock <Cell; south>
 command <lever> unlock <Cell; down>
end define

define room <Yard>
 look <The prison yard.>
 out <Cell>
end define

define room <Corridor>
 look <A corridor.>
 south <Cell>
end define
