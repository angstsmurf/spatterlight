' A `define function' block wins over a built-in of the same name (DoFunction,
' V4Game.cs:6760-6772): this `round' truncates where Question's built-in rounds.
' Also pinned: a `return' inside a multi-line brace block reaches the caller
' (ExecuteDo's !intproc case, ASL >= 3.92), and an inline {expression} in a
' $function(...)$ argument is evaluated before the call (V4Game.cs:6716).
' Real Quest prints the same seven values.
define game <userfunc>
	asl-version <410>
	start <room>
	command <t1> msg <A[$round(78.26; 1)$]>
	command <t2> msg <B[$round (78.26; 1)$]>
	command <t3> {
		set numeric <n; 59.04>
		msg <C[$round (%n%; 1)$] [$round(173.6;0)$] [$round(5;1)$]>
		msg <D[$b(1)$] [$d(7.5)$] [$e(7.5)$]>
	}
end define
define function <round>
	set numeric <round.number; $parameter(1)$>
	set numeric <round.dp; $parameter(2)$>
	set numeric <round.decimalpos; $instr(%round.number%; .)$>
	if ( %round.decimalpos% = 0 ) then {
		return <%round.number%>
		} else {
		set string <round.beforedp; $left(%round.number%; {%round.decimalpos%-1})$>
		set string <round.afterdp; $mid(%round.number%; {%round.decimalpos%+1})$>
		set string <round.afterdp; $left(#round.afterdp#; %round.dp%)$>
		return <#round.beforedp#.#round.afterdp#>
		}
end define
define function <b>
	if ( 1 = 0 ) then {
		return <yes>
		} else {
		return <no>
		}
end define
define function <d>
	set string <s; $left(7.5; {2-1})$>
	return <#s#>
end define
define function <e>
	set numeric <n; $parameter(1)$>
	set numeric <k; 2>
	set string <s; $mid(%n%; {%k%+1})$>
	set string <s; $left(#s#; 1)$>
	return <x#s#>
end define
define room <room>
end define
