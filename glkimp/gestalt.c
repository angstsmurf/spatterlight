#include "glkimp.h"

glui32 glk_gestalt(glui32 id, glui32 val)
{
    return glk_gestalt_ext(id, val, NULL, 0);
}

glui32 glk_gestalt_ext(glui32 id, glui32 val, glui32 *arr, glui32 arrlen)
{
    switch (id)
    {
	case gestalt_Version:
	    return 0x00000700;
	    
	case gestalt_LineInput:
	    if (val >= 32 && val < 127)
		return TRUE;
#if 0 // only unicode char input and output are implemented
	    else if (val >= 160 && val < 0x200000)
		return TRUE;
#endif
	    else
		return FALSE;
	    
	case gestalt_CharInput: 
	    if (val >= 32 && val < 127)
		return TRUE;
	    else if (val >= 160 && val < 0x200000)
		return TRUE;
	    else if (val == keycode_Return)
		return TRUE;
	    else
		return FALSE;
	    
	case gestalt_CharOutput: 
	    /* cheaply, we don't do any translation of printed
	    characters, so the output is always one character 
	    even if it's wrong. */
	    if (arr && arrlen >= 1)
		arr[0] = 1;
	    if (val >= 32 && val < 127)
		return gestalt_CharOutput_ExactPrint;
	    else if (val >= 160 && val < 0x200000)
		return gestalt_CharOutput_ExactPrint;		
	    else
		return gestalt_CharOutput_CannotPrint;
	    
	case gestalt_Timer: 
	    return TRUE;
	    
	case gestalt_MouseInput: 
	    return TRUE;
	    
	case gestalt_Graphics:
	case gestalt_GraphicsTransparency:
	    return gli_enable_graphics;
	case gestalt_DrawImage:
	    if (val == wintype_Graphics)
		return gli_enable_graphics;
	    if (val == wintype_TextBuffer)
		return gli_enable_graphics;
	    return FALSE;

	case gestalt_Unicode:
            return TRUE;
	    	    
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
	    
	default:
	    return 0;
    }
}
