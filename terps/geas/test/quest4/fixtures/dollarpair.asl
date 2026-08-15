! A doubled dollar is a literal dollar, exactly as ## and %% are.
! ConvertParameter takes the text between a pair of conversion characters and,
! when it is empty, appends the character itself (V4Game.cs:6705-6708).  geas
! had the case for #, % and ~ but not for the dollar: eval_string evaluated the
! empty name as a user function, so the pair vanished and its price tag lost
! its currency.
!
! Pure Chaos is the corpus case, and it hits both paths -- a room `look` line,
! which GetParameter resolves once at load, and a `msg` in a command, which is
! resolved every time it runs.  The load-time evaluator can only ever do the
! escape half of the dollar pass: the rest of it calls user functions, and at
! load time there is no game to call them in.
!
! The last command is the control: a real function call still works.
define game <dollarpair>
 asl-version <410>
 start <Vault>

 command <price> msg <You search through the $$1000000 and find a key.>
 command <both>  msg <Marked $$5, sold for $$7 -- a $$2 profit.>
 command <call>  msg <The teller says: $teller$>
end define

define function <teller>
  return <that will be $$40, please>
end define

define room <Vault>
 look <It's about $$1000000. The hoard is stacked to the ceiling.>
end define
