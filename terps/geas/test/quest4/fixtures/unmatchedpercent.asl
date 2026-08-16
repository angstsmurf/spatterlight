! An unmatched conversion character (#, %, ~ or $) is not fatal.
!
! Each pass of Quest's ConvertParameter looks for its character in pairs; when
! one has no partner it reports "Line parameter <...> has missing %" to the log
! as a WarningError -- which the player never sees -- and returns the literal
! string "<ERROR>" as the value of the *whole* parameter (V4Game.cs:6697-6701,
! reached from GetParameter at 1858-1902).  Play then carries on as normal.
!
! geas used to throw out of GeasFile::static_eval instead.  The exception was
! caught in set_game, which abandoned the rest of startup, so the game printed
! nothing at all -- no title, no intro, no room description -- and every command
! afterwards was a no-op.  "The Statue of Riddles" (2009) has one room described
! as "100% empty" and was completely unplayable because of that single percent
! sign.  The runtime path (eval_string) merely truncated the text at the stray
! character, which is also wrong: Quest discards the whole parameter.
!
! Note that "%%" and "##" are *empty* variable names, not unmatched characters,
! and Quest emits a single literal % or # for them (6705-6708).
!
! ConvertParameter is one body called once per conversion character, so all four
! behave alike -- $ included, which is why the dollar commands below read the
! same as the percent ones.  geas had left the $ arm passing the rest of the
! string through instead of discarding the parameter (finding 34).
!
! One divergence remains, and it is the reason this file has two rooms.  Quest
! converts a room's `look` text once at load (SetUpRoomData, V4Game.Part2.cs:
! 1170-1172) and stores the result, so a description with a stray % is stored --
! and printed -- as just "<ERROR>".  We resolve definition-level text at load
! too, but write the result back as a `properties <look=...>` line, and
! next_token ends a <...> token at the first '>', so "<ERROR>" cannot survive
! the round trip.  The descriptions below therefore keep their raw text (with
! %pct% left unsubstituted, since the failed pass discards its partial work),
! while the `msg` commands -- the runtime path, and the one games really use to
! assemble text -- do report "<ERROR>" exactly as Quest does.
define game <UnmatchedPercent>
 asl-version <410>
 start <hall>
 command <pct> msg <A stray % percent.>
 command <hash> msg <A stray # hash.>
 command <empty> msg <Doubled %% and ## are literal.>
 command <both> msg <Stray % and paired #var#.>
 command <paired> msg <Paired %num% and #var#.>
 command <dollar> msg <A stray $ dollar.>
 command <dollarempty> msg <A doubled $$ is literal.>
end define

! The room description is the case that used to silence the entire game.
define room <hall>
 look <Yep, 100% empty.>
 north <plain>
end define

! Reached to prove the game is still running after the bad description.
define room <plain>
 look <An open plain, %pct%% of it grass.>
 south <hall>
end define

define variable <num>
 type numeric
 value <7>
end define

define variable <pct>
 type numeric
 value <93>
end define

define variable <var>
 type string
 value <a string>
end define
