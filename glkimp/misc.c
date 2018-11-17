#include "glkimp.h"

gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass) = NULL;
void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock) = NULL;
gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, char *typecode) = NULL;
void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, gidispatch_rock_t objrock) = NULL;

unsigned char glk_char_to_lower(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z')
	return ch + ('a' - 'A');
    if (ch >= 0xC0 && ch <= 0xDE && ch != 0xD7)
	return ch + 0x20;
    return ch;
}

unsigned char glk_char_to_upper(unsigned char ch)
{
    if (ch >= 'a' && ch <= 'z')
	return ch - ('a' - 'A');
    if (ch >= 0xe0 && ch <= 0xFE && ch != 0xF7)
	return ch - 0x20;
    return ch;
}

void glk_tick()
{
}

void glk_exit()
{
    win_flush();
    close(0);
    close(1);
    exit(0);
}

void glk_set_interrupt_handler(void (*func)(void))
{
    /* This cheap library doesn't understand interrupts. */
}

void gidispatch_set_object_registry(gidispatch_rock_t (*regi)(void *obj, glui32 objclass), 
				    void (*unregi)(void *obj, glui32 objclass, gidispatch_rock_t objrock))
{
    window_t *win;
    stream_t *str;
    fileref_t *fref;
    channel_t *chan;
    
    gli_register_obj = regi;
    gli_unregister_obj = unregi;
    
    if (gli_register_obj)
    {
	/* It's now necessary to go through all existing objects, and register them. */
	for (win = glk_window_iterate(NULL, NULL); win; win = glk_window_iterate(win, NULL))
	    win->disprock = (*gli_register_obj)(win, gidisp_Class_Window);
	for (str = glk_stream_iterate(NULL, NULL); str; str = glk_stream_iterate(str, NULL))
	    str->disprock = (*gli_register_obj)(str, gidisp_Class_Stream);
	for (fref = glk_fileref_iterate(NULL, NULL); fref; fref = glk_fileref_iterate(fref, NULL))
	    fref->disprock = (*gli_register_obj)(fref, gidisp_Class_Fileref);
	for (chan = glk_schannel_iterate(NULL, NULL); chan; chan = glk_schannel_iterate(chan, NULL))
	    chan->disprock = (*gli_register_obj)(chan, gidisp_Class_Schannel);
    }
}

void gidispatch_set_retained_registry(gidispatch_rock_t (*regi)(void *array, glui32 len, char *typecode), 
				      void (*unregi)(void *array, glui32 len, char *typecode, gidispatch_rock_t objrock))
{
    gli_register_arr = regi;
    gli_unregister_arr = unregi;
}

gidispatch_rock_t gidispatch_get_objrock(void *obj, glui32 objclass)
{
    switch (objclass)
    {
	case gidisp_Class_Window:
	    return ((window_t *)obj)->disprock;
	case gidisp_Class_Stream:
	    return ((stream_t *)obj)->disprock;
	case gidisp_Class_Fileref:
	    return ((fileref_t *)obj)->disprock;
	case gidisp_Class_Schannel:
            return ((channel_t *)obj)->disprock;
	default:
	{
	    gidispatch_rock_t dummy;
	    dummy.num = 0;
	    return dummy;
	}
    }
}

/* Some dirty fixes for Gargoyle compatibility */
char gli_program_name[256] = "Unknown";
char gli_program_info[256] = "";
char gli_story_name[256] = "";
char gli_story_title[256] = "";

void garglk_set_program_name(const char *name)
{
    strncpy(gli_program_name, name, sizeof gli_program_name);
    gli_program_name[sizeof gli_program_name-1] = 0;
    //wintitle();
}

void garglk_set_program_info(const char *info)
{
    strncpy(gli_program_info, info, sizeof gli_program_info);
    gli_program_info[sizeof gli_program_info-1] = 0;
}

void garglk_set_story_name(const char *name)
{
    strncpy(gli_story_name, name, sizeof gli_story_name);
    gli_story_name[sizeof gli_story_name-1] = 0;
    //wintitle();
}

void garglk_set_story_title(const char *title)
{
    strncpy(gli_story_title, title, sizeof gli_story_title);
    gli_story_title[sizeof gli_story_title-1] = 0;
    wintitle();
}
