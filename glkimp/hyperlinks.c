#include "glkimp.h"

#ifdef GLK_MODULE_HYPERLINKS

void glk_set_hyperlink(glui32 linkval)
{
    gli_set_hyperlink(gli_currentstr, linkval);
}

void glk_set_hyperlink_stream(strid_t str, glui32 linkval)
{
    if (!str)
    {
        gli_strict_warning("set_hyperlink_stream: invalid ref");
        return;
    }
    gli_set_hyperlink(str, linkval);

}

void glk_request_hyperlink_event(winid_t win)
{
    if (!win)
    {
        gli_strict_warning("request_hyperlink_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
        case wintype_Graphics:
            win->hyper_request = TRUE;
            break;
        default:
            /* do nothing */
            break;
    }
}

void glk_cancel_hyperlink_event(winid_t win)
{
    if (!win)
    {
        gli_strict_warning("cancel_hyperlink_event: invalid ref");
        return;
    }

    switch (win->type)
    {
        case wintype_TextBuffer:
        case wintype_TextGrid:
        case wintype_Graphics:
            win->hyper_request = FALSE;
            break;
        default:
            /* do nothing */
            break;
    }
}

#endif /* GLK_MODULE_HYPERLINKS */

