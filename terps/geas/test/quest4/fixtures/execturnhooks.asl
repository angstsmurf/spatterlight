! What `exec` does to the turn hooks.
!
! `exec` is not a shortcut into the dispatcher.  ExecuteExec, at
! V4Game.Part2.cs:2215-2255, builds a fresh context and hands its parameter to
! the whole of ExecCommand at 4127-4626, so an exec'd command runs its own
! beforeturn and its own afterturn on top of the ones the outer turn is already
! running.  One `exec` therefore ticks the turn hooks twice, and two `exec`s in
! the same command tick them three times.
!
! An exec'd command that matches nothing takes the same error path a typed one
! does, ibid. 4574, hooks and all -- it is not silently dropped.
!
! `exec <cmd; normal>` is ExecCommand's `runUserCommand` turned off, ibid.
! 2246-2251: the game's own `command <>` definitions are passed over and cmd
! gets the built-in handling.  The extra pass over the hooks happens either way.
!
! "Magic Sword" is the corpus game that needs this: its
! `command <use #obj# on #char#> exec <use #obj# on #char#; normal>` runs the
! afterturn twice on every attack, and the afterturn is what counts the
! Oggy-Waggi plant's regrowth timer down, so the plant sprouts a turn earlier
! than one afterturn a turn would have it.  "The Things That Go Bump In The
! Night" is built out of the same shape three times over -- its EXAMINE, its
! SHINE LIGHT ON and its SPRAY are all `exec`, and all three set-pieces they
! belong to are timed off the afterturn.
define game <ExecTurnHooks>
 asl-version <410>
 start <lab>
 beforeturn msg <  [game before]>
 afterturn msg <  [game after]>

 ! One exec: two passes over both hooks, and the prod happens inside the second.
 command <poke> exec <prod>
 command <prod> msg <You prod the switch.>

 ! Two execs in one command: three passes.
 command <poke twice> {
  exec <prod>
  exec <prod>
 }

 ! An exec'd command nothing matches still gets the error and the hooks.
 command <ghost> exec <flibbertigibbet>

 ! An empty command line is not one of those: ExecCommand returns on the spot,
 ! ibid. 4137-4141, before the hooks, the error and its closing blank line.  A
 ! line of nothing but spaces is not empty, though, and neither is one whose
 ! emptiness is on the near side of a `; normal' -- that half is trimmed first.
 command <nothing> exec <>
 command <spaces> exec <   >
 command <blank> exec < ; normal>

 ! A post-command parameter that is not `normal' is logged and nothing runs,
 ! ibid. 2251-2254: no turn, no hooks, not even the separator.
 command <bad> exec <prod; wibble>

 ! The game's own `take lamp` wins for a typed command and for a plain exec;
 ! `; normal' passes over it and lets the built-in take run.
 command <take lamp> msg <You know better than to touch that.>
 command <grab> exec <take lamp>
 command <grab hard> exec <take lamp; normal>
end define

define room <lab>
 look <A small laboratory.>
 beforeturn msg <  [lab before]>
 afterturn msg <  [lab after]>

 define object <lamp>
  look <A brass lamp.>
  take
 end define
end define
