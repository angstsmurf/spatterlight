' `give' takes the first <...> on the line whatever word precedes it
' (GetParameter, V4Game.Part2.cs:5796-5799), so QDK's long-hand `give object
' <hairdo>' -- On Time's elf menu -- hands the object over like `give <hairdo>'.
define game <giveobject>
	asl-version <400>
	start <room>
	command <elf> {
		create object <hairdo>
		give object <hairdo>
		if got <hairdo> then msg <got it> else msg <not got>
	}
end define
define room <room>
end define
