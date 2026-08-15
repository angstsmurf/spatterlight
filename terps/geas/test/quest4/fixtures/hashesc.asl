' Two conversion characters in a row stand for a literal one: Quest's
' ConvertParameter has an explicit empty-name case (V4Game.cs:6704-6708) and
' runs it for each of #, %, ~ and $.  geas had that case in eval_string, which
' handles `msg` and other runtime parameters (geas-runner.cc:7895-7896,
' :7952-7953, :7970-7971), and not in static_eval, the load-time twin that
' resolves definition text, so the same escape survived a msg and was eaten in
' a room alias or an object `look`, where `##` became an empty string-variable
' name and `%%` an empty numeric one -- and a missing numeric variable reads as
' "-32768".  static_eval has it in both branches now (geasfile.cc:930-941 and
' :991-995).
'
' RiddleRun's five rooms are aliased `Room ##1` .. `Room ##5`; the `####` room
' here pins how many passes have run, since one pass leaves `##` and two leave
' nothing.  The alias gets one pass on either side.  The object `look` gets two
' on either side, which is why Quest answers it with `<ERROR>`: its first pass
' leaves a lone `#`, and an unpaired conversion character replaces the whole
' parameter.

define game <Hashesc>
 asl-version <400>
 start <Room 1>
 command <hash> msg <cmd: A ## B and 100%% sure>
end define

define room <Room 1>
 alias <Room ##1>
 east <Room 2>
 define object <sign>
  look <look: A ## B and 100%% sure>
 end define
end define

define room <Room 2>
 alias <Room ####2>
 west <Room 1>
end define
