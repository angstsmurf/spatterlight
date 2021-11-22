#include "tadshtml.h"

extern "C"
{
    
    char G_tads_oem_app_name[] = "CocoTADS";
    char G_tads_oem_display_mode[] = "technicolor";
    char G_tads_oem_author[] = "Maintained by Tor Andersson.\n";
    int G_tads_oem_copyright_prefix = 1;
    
    /*
     *   Simple implementation of os_get_sysinfo.  This can be used for any
     *   non-HTML version of the system, since all sysinfo codes currently
     *   pertain to HTML features.  Note that new sysinfo codes may be added
     *   in the future which may be relevant to non-html versions, so the
     *   sysinfo codes should be checked from time to time to ensure that new
     *   codes relevant to this system version are handled correctly here.  
     */
    int os_get_sysinfo(int code, void *param, long *result)
    {
	
        /* check the type of information they're requesting */
        switch(code)
        {
	    case SYSINFO_INTERP_CLASS:
		*result = SYSINFO_ICLASS_HTML;
		return TRUE;
		
	    case SYSINFO_HTML:
	    case SYSINFO_JPEG:
	    case SYSINFO_PNG:
	    case SYSINFO_PNG_TRANS:
	    case SYSINFO_PNG_ALPHA:
	    case SYSINFO_PREF_IMAGES:
		*result = TRUE;
		return TRUE;
		
	    case SYSINFO_WAV:
	    case SYSINFO_MIDI:
	    case SYSINFO_WAV_MIDI_OVL:
	    case SYSINFO_WAV_OVL:
	    case SYSINFO_MPEG:
	    case SYSINFO_MPEG1:
	    case SYSINFO_MPEG2:
	    case SYSINFO_MPEG3:
	    case SYSINFO_PREF_SOUNDS:
	    case SYSINFO_PREF_MUSIC:
	    case SYSINFO_PREF_LINKS:
	    case SYSINFO_LINKS_HTTP:
	    case SYSINFO_LINKS_FTP:
	    case SYSINFO_LINKS_NEWS:
	    case SYSINFO_LINKS_MAILTO:
	    case SYSINFO_LINKS_TELNET:
	    case SYSINFO_OGG:
	    case SYSINFO_MNG:
	    case SYSINFO_MNG_TRANS:
	    case SYSINFO_MNG_ALPHA:
	    case SYSINFO_BANNERS:
		/* 
		*   we don't support any of these features - set the result to 0
		 *   to indicate this 
		 */
		*result = 0;
		
		/* return true to indicate that we recognized the code */
		return TRUE;
		
	    default:
		/* not recognized */
		return FALSE;
        }
    }
    
}

os_timer_t os_get_time(void)
{
    return os_get_sys_clock_ms();
}

textchar_t *os_next_char(oshtml_charset_id_t id, const textchar_t *p)
{
    /* some utf-8 voodoo should go here */
    return (textchar_t*) (p + 1);
}

void os_dbg_sys_msg(char const *msg)
{
    fprintf(stderr, "%s", msg);
}

