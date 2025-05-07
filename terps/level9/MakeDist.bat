@echo off

"%ProgramFiles(x86)%\Zip\zip" -j \Temp\Level9_Dos16.zip COPYING level9.txt DOS/level9.exe

"%ProgramFiles(x86)%\Zip\zip" -j \Temp\Level9_Dos32.zip COPYING level9.txt
"%ProgramFiles(x86)%\Zip\zip" -j \Temp\Level9_Dos32.zip DOS32/level9.exe DOS32/allegro.cfg DOS32/keyboard.dat
"%ProgramFiles(x86)%\Zip\zip" -j \Temp\Level9_Dos32.zip \DosApps\DJGPP\bin\cwsdpmi.exe

"%ProgramFiles(x86)%\Zip\zip" -j \Temp\Level9_Win32.zip COPYING Win/Release/Level9.exe Win/Release/Level9.chm

"%ProgramFiles(x86)%\Zip\zip" \Temp\Level9_Source.zip COPYING *.c *.h *.bat *.txt -x todo.txt
"%ProgramFiles(x86)%\Zip\zip" \Temp\Level9_Source.zip Amiga/*
"%ProgramFiles(x86)%\Zip\zip" \Temp\Level9_Source.zip DOS/* -x DOS/*.exe
"%ProgramFiles(x86)%\Zip\zip" \Temp\Level9_Source.zip DOS32/* -x DOS32/*.exe
"%ProgramFiles(x86)%\Zip\zip" \Temp\Level9_Source.zip Glk/*
"%ProgramFiles(x86)%\Zip\zip" \Temp\Level9_Source.zip Gtk/*
"%ProgramFiles(x86)%\Zip\zip" \Temp\Level9_Source.zip Unix/*
"%ProgramFiles(x86)%\Zip\zip" \Temp\Level9_Source.zip Win/* Win/Help/* Win/Classlib/*

