' Game-file keywords are case-insensitive: Quest reads nearly every one with
' BeginsWith, which lowercases both sides (V4Game.cs:1094-1102).  This fixture
' spells a spread of them in mixed case -- error/startscript in the game block,
' flag on, set string, repeat until, for each ... in, create exit NORTH, alt,
' drop everywhere, onchange, and timer names (timeron/set interval/timerstate
' compare LCase'd, V4Game.Part2.cs:565, 6120, V4Game.cs:7087).  Every one of
' these was matched case-sensitively in geas until the // SENSITIVE? audit;
' see the CI notes at each site in geas-runner.cc.
'
' One deliberately case-SENSITIVE spot is pinned too: the error *name* inside
' an error line is switched on raw (V4Game.Part2.cs:1394), so the <Badcommand>
' override must NOT fire and the <badcommand> one must.
define game <KeywordCase>
 asl-version <410>
 start <Start>
 Game Version <7>
 Error <Badcommand; This override must not fire.>
 Error <badcommand; Que?>
 StartScript {
  Flag On <started>
  if flag <started> then msg <flag on: OK>
  Set String <greeting; hello>
  msg <set string: #greeting#>
  Repeat Until flag <done> {
   flag on <done>
   msg <repeat until: OK>
  }
  For Each object In <Start> msg <for each: #quest.thing#>
  Create Exit NORTH <Start; Cellar>
  set numeric <counter; 1>
  timeron <TICKER>
  msg <timerstate: $timerstate(TICKER)$>
 }
end define

define room <Start>
 define object <rock>
  Alt <boulder>
  examine msg <Just a rock.>
  take <Taken.>
  Drop Everywhere <It bounces.>
 end define
end define

define room <Cellar>
 look <A damp cellar.>
end define

define timer <ticker>
 interval <5>
end define

define variable <counter>
 type numeric
 OnChange msg <onchange: counter changed>
end define
