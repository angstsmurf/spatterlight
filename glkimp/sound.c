#include "glkimp.h"

static channel_t *gli_channellist = NULL;

extern char gli_workdir[];

static int loadsound(int sound)
{
    glui32 chunktype;
    FILE *file;
    char *buf;
    long pos;
    long len;
    
    if (win_findsound(sound))
	return TRUE;
    
    if (!giblorb_is_resource_map())
    {
	char filename[1024];
	
	sprintf(filename, "%s/SND%d", gli_workdir, sound);
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
    }
    
    else
    {
	giblorb_get_resource(giblorb_ID_Snd, sound, &file, &pos, &len, &chunktype);
	if (!file)
	    return FALSE;
	
	buf = malloc(len);
	if (!buf)
	    return FALSE;
	
	fseek(file, pos, 0);
	fread(buf, len, 1, file);
    }
    
    win_loadsound(sound, buf, len);
    
    free(buf);
     
    return TRUE;
}

schanid_t glk_schannel_create(glui32 rock)
{
    channel_t *chan;
    
    if (!gli_enable_sound)
	return NULL;
    
    chan = malloc(sizeof(channel_t));
    
    if (!chan)
	return NULL;
    
    chan->rock = rock;
    chan->peer = win_newchan();
    if (chan->peer == -1)
    {
	free(chan);
	return NULL;
    }
    
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
    
    if (!loadsound(snd))
	return FALSE;
    
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

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
    if (!chan) {
	gli_strict_warning("schannel_set_volume: invalid id.");
	return;
    }
    win_setvolume(chan->peer, vol);
}

