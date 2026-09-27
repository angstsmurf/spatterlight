' Two entries in Quest's error-name switch are version-gated
' (SetUpUserDefinedPlayerErrors, V4Game.Part2.cs:1466-1517):
'
'  * "defaultake" sets BadTake at ASL 2.80 and below, and only becomes the
'    DefaultTake message above it -- so in an old game the line is the refusal,
'    not the success;
'  * "badexamine" is recognised from ASL 3.10 only.  Below it the case falls
'    through with currentError still 0, so the line redefines badcommand.
'
' See errornames.asl for the same switch at 4.10.

define game <Errornames280>
 asl-version <280>
 start <Hall>
 error <defaultake; The rock is far too heavy.>
 error <badexamine; Below 3.10 this line redefines badcommand.>
end define

define room <Hall>
 look <The hall.>
 define object <rock>
  prefix <a>
 end define
 define object <coin>
  prefix <a>
  take
 end define
end define
