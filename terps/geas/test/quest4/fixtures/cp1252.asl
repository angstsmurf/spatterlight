' Windows-1252, which is what a Quest 4 file is written in.
'
' Quest is a VB6 program and reads its files through Chr(), which maps a byte
' through the thread's ANSI code page.  On the Windows these games were written
' on that is always 1252, so an author who typed a pound sign got one byte,
' 0xA3, and QDK's own typography -- curly quotes, an em dash, an ellipsis, a
' trademark -- lives in 0x80-0x9F, the range Latin-1 leaves as control codes.
' geas used to hand those bytes straight to Glk, which reads them as Latin-1:
' everything above 0x7F came out as mojibake and the QDK range as nothing at
' all.  read_geas_file now transcodes the whole file to UTF-8 once, after the
' .cas decoder and after !include has pulled every other file in.
'
' The player's input has to make the same trip or it would stop matching: a
' non-Unicode Glk line request hands back Latin-1 bytes, and a command script
' is whatever the author saved it as.  run_command and the `enter' statement
' both normalise, which is why TAKE PANUELO (typed with the tilde) still finds
' the object here.
'
' This file itself is stored in Windows-1252 -- that is the point of it -- so
' an editor that opens it as UTF-8 will show the same mojibake the bug used to
' print.  The .expected transcript beside it is UTF-8.
define game <cp1252>
    asl-version <410>
    start <Sala>
    game version <1.0>
    game author <Ángel Muñoz>

    command <symbols> {
        msg <QDK range: ‘single’ “double” — dash … ellipsis ™ trade † dagger>
        msg <Latin-1 range: © copyright £ pound ½ half × times ÷ divide>
    }
end define

define room <Sala>
    look <Una sala pequeña. El sol entra por la ventana, y hace calor aquí.>
    alias <Sala del Museo>

    define object <pañuelo>
        look <Un pañuelo de seda, bordado con la letra “M”.>
        take
        prefix <un>
    end define

    define object <cañón>
        look <Un cañón de bronce, © 1487.>
    end define
end define
