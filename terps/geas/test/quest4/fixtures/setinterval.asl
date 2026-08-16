' `set interval' reaches the cycle a timer is already in -- finding 52.
'
' Quest counts a timer up and compares the running count against the interval
' as it stands on the tick (Tick, V4Game.Part2.cs:126-131):
'
'     _timers[i].TimerTicks += elapsedTime
'     if (_timers[i].TimerTicks >= _timers[i].TimerInterval) {
'         _timers[i].TimerTicks = 0
'         timerScripts.Add(_timers[i].TimerAction)
'     }
'
' geas used to count down and re-arm from the interval at the moment of firing,
' which is the same clock only while the interval holds still.  `set interval'
' writes nothing but the interval on either side, so the two parted as soon as
' it moved -- and they parted in two different ways, which is why this fixture
' has two timers.
'
' `banked' is the shortening case, driven from outside the timer.  It starts at
' interval 10; `fast' cuts it to 2 on the fifth turn, by which time 5 ticks are
' banked, and 5 is already past 2, so it fires on that very turn and every 2
' turns after.  A countdown seeded from the old 10 cannot do that at all: no
' shortening could take effect before the cycle in flight ran out.
'
' `stepping' is the lengthening case, driven from inside the timer's own action
' -- which is how every corpus game does it: Barbarian, Beam, King's Quest V,
' Michael's Game, The Pilgrim's Progress, Shipwrecked and ThunderClan all call
' `set interval' on themselves.  It starts at 2 and its action raises it to 4,
' so it fires on turns 2, 6, 10 and 14.  Counting down, the action ran after
' the re-arm, so the new interval took hold only from the cycle *after* next
' and the first gap was 2 rather than 4: the Pilgrim served each of its slough
' messages a whole cycle late and the rescue arrived four turns behind Quest's.
' (The interval is written as a literal on purpose -- `set interval
' <t; %var%>' is a divergence of its own, Quest not substituting there.)
'
' One tick per turn, through fixtures/setinterval.args:
'
'   1  z                 banked 1                   stepping 1
'   2  z                 banked 2                   stepping fires, 2 -> 4
'   3  z                 banked 3                   stepping 1
'   4  z                 banked 4                   stepping 2
'   5  fast   10 -> 2    banked fires (5 >= 2)      stepping 3
'   6  z                 banked 1                   stepping fires
'   7  z                 banked fires
'   ⋮                    and every 2 turns after    and every 4 turns after
define game <SetInterval>
 asl-version <410>
 start <Hall>
end define

define timer <banked>
 interval <10>
 action msg <[banked]>
 enabled
end define

define timer <stepping>
 interval <2>
 action {
  msg <[stepping]>
  set interval <stepping; 4>
 }
 enabled
end define

define room <Hall>
 look <A hall.>
 command <z> msg <[turn]>
 command <fast> {
  msg <[cutting banked to 2]>
  set interval <banked; 2>
 }
end define
