#include "glkimp.h"

glui32 glk_gestalt(glui32 id, glui32 val)
{
    return glk_gestalt_ext(id, val, NULL, 0);
}

glui32 glk_gestalt_ext(glui32 id, glui32 val, glui32 *arr,
                       glui32 arrlen)
{
    switch (id) {

        case gestalt_Version:
            /* This implements Glk spec version 0.7.5. */
            return 0x00000705;

        case gestalt_LineInput:
            if (val >= 32 && val < 127)
                return TRUE;
            else
                return FALSE;

        case gestalt_CharInput:
            if (val >= 32 && val < 127)
                return TRUE;
            else if (val == keycode_Return)
                return TRUE;
            else {
                /* If we're doing UTF-8 input, we can input any Unicode
                 character. Except control characters. */
                if (gli_utf8input) {
                    if (val >= 160 && val < 0x200000)
                        return TRUE;
                }
                /* If not, we can't accept anything non-ASCII */
                return FALSE;
            }

        case gestalt_CharOutput:
            if (val >= 32 && val < 127) {
                if (arr && arrlen >= 1)
                    arr[0] = 1;
                return gestalt_CharOutput_ExactPrint;
            }
            else {
                /* cheaply, we don't do any translation of printed
                 characters, so the output is always one character
                 even if it's wrong. */
                if (arr && arrlen >= 1)
                    arr[0] = 1;
                /* If we're doing UTF-8 output, we can print any Unicode
                 character. Except control characters. */
                if (gli_utf8output) {
                    if (val >= 160 && val < 0x200000)
                        return gestalt_CharOutput_ExactPrint;
                }
                return gestalt_CharOutput_CannotPrint;
            }
            
        case gestalt_MouseInput:
            return TRUE;
            
        case gestalt_Timer:
            return TRUE;
            
        case gestalt_Graphics:
        case gestalt_GraphicsTransparency:
        case gestalt_GraphicsCharInput:
            return gli_enable_graphics;
            
        case gestalt_DrawImage:
            if (val == wintype_Graphics)
                return gli_enable_graphics;
            if (val == wintype_TextBuffer)
                return gli_enable_graphics;
            return FALSE;
            
        case gestalt_Sound:
        case gestalt_SoundVolume:
        case gestalt_SoundNotify:
        case gestalt_SoundMusic:
        case gestalt_Sound2:
            return gli_enable_sound;
            
        case gestalt_Unicode:
            return TRUE;
        case gestalt_UnicodeNorm:
            return TRUE;

        case gestalt_Hyperlinks:
            return TRUE;
        case gestalt_HyperlinkInput:
            return TRUE;

        case gestalt_LineInputEcho:
            return TRUE;
        case gestalt_LineTerminators:
            return TRUE;
        case gestalt_LineTerminatorKey:
            return gli_window_check_terminator(val);

        case gestalt_DateTime:
            return TRUE;

        case gestalt_ResourceStream:
            return TRUE;

        default:
            return 0;
    }
}
