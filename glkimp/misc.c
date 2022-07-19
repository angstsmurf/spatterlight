#include "glkimp.h"
#include "fileref.h"

static unsigned char char_tolower_table[256];
static unsigned char char_toupper_table[256];

glui32 tagcounter = 0;

gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass) = NULL;
void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock) = NULL;
gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, char *typecode) = NULL;
void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, gidispatch_rock_t objrock) = NULL;

long (*gli_locate_arr)(void *array, glui32 len, char *typecode, gidispatch_rock_t objrock, int *elemsizeref);
gidispatch_rock_t (*gli_restore_arr)(long bufkey, glui32 len, char *typecode, void **arrayref);

void gli_initialize_misc()
{
    int ix;
    int res;

    /* Initialize the to-uppercase and to-lowercase tables. These should
     *not* be localized to a platform-native character set! They are
     intended to work on Latin-1 data, and the code below correctly
     sets up the tables for that character set. */

    for (ix=0; ix<256; ix++) {
        char_toupper_table[ix] = ix;
        char_tolower_table[ix] = ix;
    }
    for (ix=0; ix<256; ix++) {
        if (ix >= 'A' && ix <= 'Z') {
            res = ix + ('a' - 'A');
        }
        else if (ix >= 0xC0 && ix <= 0xDE && ix != 0xD7) {
            res = ix + 0x20;
        }
        else {
            res = 0;
        }
        if (res) {
            char_tolower_table[ix] = res;
            char_toupper_table[res] = ix;
        }
    }

}

unsigned char glk_char_to_lower(unsigned char ch)
{
    return char_tolower_table[ch];
}

unsigned char glk_char_to_upper(unsigned char ch)
{
    return char_toupper_table[ch];
}

void glk_tick()
{
}

void glk_exit()
{
    if (gli_program_info != NULL)
        free(gli_program_info);
    if (gli_game_path != NULL)
        free(gli_game_path);
    if (gli_story_name != NULL)
        free(gli_story_name);
    if (gli_story_title != NULL)
        free(gli_story_title);
    if (gli_program_name != NULL)
        free(gli_program_name);
    if (gli_workdir != NULL)
        free(gli_workdir);
    if (autosavedir != NULL)
        free(autosavedir);
    if (gli_parentdir != NULL)
        free(gli_parentdir);

    win_flush();

    gli_stop_all_sound_channels();
    gli_close_all_file_streams();

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

void gidispatch_set_autorestore_registry(long (*locatearr)(void *array, glui32 len, char *typecode,
                                                                  gidispatch_rock_t objrock, int *elemsizeref),
                                                gidispatch_rock_t (*restorearr)(long bufkey, glui32 len,
                                                                                char *typecode, void **arrayref))
{
    gli_locate_arr = locatearr;
    gli_restore_arr = restorearr;
}


glui32 generate_tag(void)
{
    tagcounter++;
    return tagcounter;
}

/* Some dirty fixes for Gargoyle compatibility */
char *gli_program_name = NULL;
char *gli_program_info = NULL;
char *gli_story_name = NULL;
char *gli_story_title = NULL;

void malloc_and_set_string(char **variable, const char *string)
{
    size_t length = strlen(string);

    if (*variable != NULL)
        free(*variable);

    *variable = malloc(length + 1);
    strncpy(*variable, string, length);
    (*variable)[length] = 0;
}

void garglk_set_program_name(const char *name)
{
    malloc_and_set_string(&gli_program_name, name);
}

void garglk_set_program_info(const char *info)
{
    malloc_and_set_string(&gli_program_info, info);
}

void garglk_set_story_name(const char *name)
{
    malloc_and_set_string(&gli_story_name, name);
}

void garglk_set_story_title(const char *title)
{
    malloc_and_set_string(&gli_story_title, title);
    wintitle();
}

void spatterlight_set_game_path(const char *path)
{
    malloc_and_set_string(&gli_game_path, path);
}
