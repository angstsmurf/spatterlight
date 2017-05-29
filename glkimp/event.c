#include "glkimp.h"

event_t *gli_curevent = NULL; 

void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2)
{
    if (gli_curevent)
    {
        gli_curevent->type = type;
        gli_curevent->win = win;
        gli_curevent->val1 = val1;
        gli_curevent->val2 = val2;
    }
}

void glk_request_timer_events(glui32 millisecs)
{
    win_timer(millisecs);
}

void gli_select(event_t *event, int block)
{

    gli_curevent = event;
    
    gli_event_clearevent(gli_curevent);
    
    win_select(gli_curevent, block);
    
    gli_curevent = NULL;
}

void glk_select(event_t *event)
{
    gli_select(event, 1);
}

void glk_select_poll(event_t *event)
{
    gli_select(event, 0);
}
