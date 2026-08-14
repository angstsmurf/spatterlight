! "out" is the one exit Quest keeps in two halves.  The room parser fills
! Out.Text from the parameter and Out.Script from whatever follows the '>'
! (V4Game.Part2.cs:999-1001), and GoDirection prefers the script whenever there
! is one (6302-6313).  "create exit out <src; dest>" assigns Out.Text only
! (V4Game.cs:5485), so a room whose "out" line carries a script keeps printing
! the script after the exit has been created, while a room with no "out" line at
! all gains a working one.
!
! geas read the room block and nothing else for "out", so a created out exit was
! invisible: Riddle Run, whose five doors are all "create exit out <Room n;
! Room n+1>", could not be played past its first riddle.
define game <OutExit>
 asl-version <400>
 start <Hall>
end define

define room <Hall>
 look <A hall.>
 out <Porch>
end define

define room <Porch>
 look <A porch.>
 out <Hall> msg <The door has swollen shut.>
 north <Garden>
 command <pry> create exit out <Porch; Cellar>
end define

define room <Garden>
 look <A garden.>
 south <Porch>
 command <dig> create exit out <Garden; Cellar>
end define

define room <Cellar>
 look <A cellar.>
end define
