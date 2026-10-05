' "l" is "look" and "l <thing>" is "look <thing>" (ExecCommand,
' V4Game.Part2.cs:4426-4429, 4452-4455), typed or exec'd.
define game <labbrev>
	asl-version <400>
	start <room>
	command <peek #thing#> exec <l #thing#>
end define

define room <room>
	look <A room.>
	define object <rat>
		look <A rat.>
	end define
end define
