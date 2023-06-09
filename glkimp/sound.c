#include <sys/syslimits.h>
#include <fcntl.h>
#include "glkimp.h"
#include "fileref.h"

#define GLK_MAXVOLUME 0x10000

static channel_t *gli_channellist = NULL;

extern char *gli_workdir;

static int loadsound(int sound)
{
    glui32 chunktype;
    FILE *file;
    char *buf;
    long pos = 0;
    long len;
    char filename[PATH_MAX];
    
    if (win_findsound(sound))
        return TRUE;
    
    if (!giblorb_is_resource_map())
    {
        int namelength = sound / 10 + 6;
        snprintf(filename, namelength + gli_parentdirlength, "%s/SND%d", gli_parentdir, sound);

        fprintf(stderr, "loadsound %s\n", filename);

        file = fopen(filename, "rb");
        if (!file)
            return FALSE;

        fseek(file, 0, 2);
        len = ftell(file);
        fseek(file, 0, 0);

        buf = malloc(len);
        if (!buf)
        {
            fclose(file);
            return FALSE;
        }

        fread(buf, len, 1, file);

        fclose(file);
        free(buf);
    }
    else
    {
        giblorb_get_resource(giblorb_ID_Snd, sound, &file, &pos, &len, &chunktype);
        if (!file)
        {
            gli_strict_warning("Failed to get resource from blorb!\n");
            return FALSE;
        }

        if (fcntl(fileno(file), F_GETPATH, filename) == -1)
        {
            gli_strict_warning("Failed to get file name from blorb!\n");
            return FALSE;
        }
    }
    
    win_loadsound(sound, filename, (int)pos, (int)len);

    return TRUE;
}

schanid_t glk_schannel_create(glui32 rock)
{
    return  glk_schannel_create_ext(rock, GLK_MAXVOLUME);
}


schanid_t glk_schannel_create_ext(glui32 rock, glui32 volume)
{
    channel_t *chan;
    
    if (!gli_enable_sound)
	return NULL;
    
    chan = malloc(sizeof(channel_t));
    
    if (!chan)
	return NULL;
    
    chan->rock = rock;
    chan->peer = win_newchan(volume);
    if (chan->peer == -1)
    {
        free(chan);
        return NULL;
    }

    chan->tag = generate_tag(); /* For serialization */

    chan->prev = NULL;
    chan->next = gli_channellist;
    gli_channellist = chan;
    if (chan->next) {
        chan->next->prev = chan;
    }
    
    if (gli_register_obj)
	chan->disprock = (*gli_register_obj)(chan, gidisp_Class_Schannel);
    
    return chan;
}

void glk_schannel_destroy(schanid_t chan)
{
    channel_t *prev, *next;
    
    if (!chan) {
	gli_strict_warning("schannel_destroy: invalid id.");
	return;
    }
    
    glk_schannel_stop(chan);
    
    if (gli_unregister_obj)
	(*gli_unregister_obj)(chan, gidisp_Class_Schannel, chan->disprock);
    
    win_delchan(chan->peer);
    
    prev = chan->prev;
    next = chan->next;
    chan->prev = NULL;
    chan->next = NULL;
    
    if (prev)
	prev->next = next;
    else
	gli_channellist = next;
    if (next)
	next->prev = prev;
    
    free(chan);
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rock)
{
    if (!chan) {
	chan = gli_channellist;
    } else {
	chan = chan->next;
    }
    
    if (chan) {
	if (rock)
	    *rock = chan->rock;
	return chan;
    }
    
    if (rock)
	*rock = 0;
    return NULL;
}

glui32 glk_schannel_get_rock(schanid_t chan)
{
    if (!chan) {
	gli_strict_warning("schannel_get_rock: invalid id.");
	return 0;
    }
    return chan->rock;
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
    /* don't do a thing */
}

glui32 glk_schannel_play(schanid_t chan, glui32 snd)
{
    return glk_schannel_play_ext(chan, snd, 1, 0);
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats,
		glui32 notify)
{
    if (!chan) {
	gli_strict_warning("schannel_play_ext: invalid id.");
	return 0;
    }
    
    if (!loadsound(snd)) {
        fprintf(stderr, "loadsound %d returned false\n", snd);
        return FALSE;
    }
    
    win_playsound(chan->peer, repeats, notify);
    return TRUE;
}

void glk_schannel_stop(schanid_t chan)
{
    if (!chan) {
	gli_strict_warning("schannel_stop: invalid id.");
	return;
    }
    win_stopsound(chan->peer);
}

void glk_schannel_pause(schanid_t chan)
{
    if (!chan)
    {
        gli_strict_warning("schannel_pause: invalid id.");
        return;
    }

    win_pause(chan->peer);

//    chan->paused = TRUE;
}

void glk_schannel_unpause(schanid_t chan)
{
    if (!chan)
    {
        gli_strict_warning("schannel_unpause: invalid id.");
        return;
    }

    win_unpause(chan->peer);

//    chan->paused = 0;
}


void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{

    glk_schannel_set_volume_ext(chan, vol, 0, 0);
}

void glk_schannel_set_volume_ext(schanid_t chan, glui32 vol,
                                 glui32 duration, glui32 notify)
{
    if (!chan)
    {
        gli_strict_warning("schannel_set_volume: invalid id.");
        return;
    }
    
    win_setvolume(chan->peer, vol, duration, notify);
}

glui32 glk_schannel_play_multi(schanid_t *chanarray, glui32 chancount,
                               glui32 *sndarray, glui32 soundcount, glui32 notify)
{
    glui32 i;
    glui32 successes = 0;

    for (i = 0; i < chancount; i++)
    {
        successes += glk_schannel_play_ext(chanarray[i], sndarray[i], 1, notify);
    }

    return successes;
}

void gli_replace_schan_list(schanid_t newlist) /* Only used by autorestore */
{
    schanid_t chan;

    if (!newlist)
    {
        gli_strict_warning("gli_replace_schannel_list: invalid ref");
        return;
    }

    /* At the time when this is called, the sound channel list should be empty */
    while (gli_channellist)
    {
        chan = gli_channellist;
        gli_channellist = gli_channellist->next;
        glk_schannel_destroy(chan);
    }

    gli_channellist = newlist;
}

/* For autorestore */
channel_t *gli_schan_for_tag(int tag)
{
    channel_t *chan = gli_channellist;
    while (chan)
    {
        if (chan->tag == tag)
            return chan;
        chan = chan->next;
    }
    return NULL;
}

void gli_stop_all_sound_channels(void)
{
    channel_t *chan;

    for (chan = glk_schannel_iterate(NULL, NULL); chan; chan = glk_schannel_iterate(chan, NULL))
    {
        glk_schannel_stop(chan);
    }
}
