#include "glkimp.h"

glui32 glk_gestalt(glui32 id, glui32 val)
{
    return glk_gestalt_ext(id, val, NULL, 0);
}

glui32 glk_gestalt_ext(glui32 id, glui32 val, glui32 *arr,
                       glui32 arrlen)
{
    switch (id)
    {
        case gestalt_Version:
            return 0x00000703;
            
        case gestalt_LineInput:
            if (val >= 32 && val < 0x10ffff)
                return TRUE;
            else
                return FALSE;
            
        case gestalt_CharInput:
            if (val >= 32 && val < 0x10ffff)
                return TRUE;
            else if (val == keycode_Return)
                return TRUE;
            else
                return FALSE;
            
        case gestalt_CharOutput:
            if (val >= 32 && val < 0x10ffff)
            {
                if (arr && arrlen >= 1)
                    arr[0] = 1;
                return gestalt_CharOutput_ExactPrint;
            }
            else
            {
                /* cheaply, we don't do any translation of printed
                 characters, so the output is always one character
                 even if it's wrong. */
                if (arr && arrlen >= 1)
                    arr[0] = 1;
                return gestalt_CharOutput_CannotPrint;
            }
            
        case gestalt_MouseInput:
            if (val == wintype_TextGrid)
                return TRUE;
            if (val == wintype_Graphics)
                return TRUE;
            return FALSE;
            
        case gestalt_Timer:
            return TRUE;
            
        case gestalt_Graphics:
        case gestalt_GraphicsTransparency:
            return gli_enable_graphics;
        case gestalt_DrawImage:
            if (val == wintype_TextBuffer)
                return gli_enable_graphics;
            if (val == wintype_Graphics)
                return gli_enable_graphics;
            return FALSE;

            
#if 0 /* NSSound */
        case gestalt_Sound:
            return gli_enable_sound;
        case gestalt_SoundVolume:
            return FALSE;
        case gestalt_SoundMusic:
            return FALSE;
        case gestalt_SoundNotify:
            return FALSE;
#else /* FMOD */
        case gestalt_Sound:
            return gli_enable_sound;
        case gestalt_SoundVolume:
            return gli_enable_sound;
        case gestalt_SoundMusic:
            return gli_enable_sound;
        case gestalt_SoundNotify: 
            return gli_enable_sound;
#endif
            
        case gestalt_Sound2:
            return FALSE;
            
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
            
        default:
            return 0;
            
    }
}
