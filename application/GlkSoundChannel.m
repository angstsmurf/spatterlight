#import "main.h"

#include <fmod.h>
#include <fmod_errors.h>

static FMOD_SYSTEM *fmsys = NULL;

static int initsound(void)
{
    FMOD_RESULT result;
    
    if (fmsys)
	return YES;
    
    NSLog(@"snd: +initsound()");
    
    result = FMOD_System_Create(&fmsys);
    if (result != FMOD_OK)
    {
	NSLog(@"FMOD: %s", FMOD_ErrorString(result));
	fmsys = NULL;
	return NO;
    }
    
    result = FMOD_System_Init(fmsys, 32, FMOD_INIT_NORMAL, NULL);
    if (result != FMOD_OK)
    {
	NSLog(@"FMOD: %s", FMOD_ErrorString(result));
	fmsys = NULL;
	return NO;
    }
    
    return YES;
}

@implementation GlkSoundChannel

- (id) initWithGlkController: (GlkController*)glkctl_ name: (int)name
{
    [super init];
    
    glkctl = glkctl_;
    
    channel = NULL;
    volume = 0x10000;
    repeats = 0;
    notify = 0;
    
    return self;
}

- (void) dealloc
{
    [self stop];
    [super dealloc];
}

- (void) play: (NSData*)sample repeats: (int)areps notify: (int)anot
{
    FMOD_CREATESOUNDEXINFO info;
    FMOD_RESULT result;
    
    if (!initsound())
	return;
    
    [self stop];
    
    repeats = areps;
    notify = anot;
    
    memset(&info, 0, sizeof(info));
    info.cbsize = sizeof(info);
    info.length = [sample length];
    
    result = FMOD_System_CreateSound(fmsys, [sample bytes],
				     FMOD_SOFTWARE | FMOD_2D /* | FMOD_CREATESTREAM */ | FMOD_OPENMEMORY,
				     &info,
				     (FMOD_SOUND**)&sound);
    if (result != FMOD_OK) {
	NSLog(@"FMOD: %s", FMOD_ErrorString(result)); return;
    }
    
    result = FMOD_Sound_SetLoopCount(sound, repeats - 1);
    if (result != FMOD_OK) {
	NSLog(@"FMOD: %s", FMOD_ErrorString(result)); return;
    }
    
    result = FMOD_System_PlaySound(fmsys, FMOD_CHANNEL_FREE, sound, 1, (FMOD_CHANNEL**)&channel);
    if (result != FMOD_OK) {
	NSLog(@"FMOD: %s", FMOD_ErrorString(result)); return;
    }

    result = FMOD_Channel_SetVolume(channel, (float)volume / 0x10000);
    if (result != FMOD_OK) {
	NSLog(@"FMOD: %s", FMOD_ErrorString(result)); return;
    }
    
    result = FMOD_Channel_SetPaused(channel, 0);
    if (result != FMOD_OK) {
	NSLog(@"FMOD: %s", FMOD_ErrorString(result)); return;
    }    
}

- (void) setVolume: (int)avol
{
    FMOD_RESULT result;
    
    volume = avol;
    
    if (channel)
    {
	if (!initsound())
	    return;

	result = FMOD_Channel_SetVolume(channel, (float)volume / 0x10000);
	if (result != FMOD_OK) {
	    NSLog(@"FMOD: %s", FMOD_ErrorString(result)); return;
	}
    }
}

- (void) stop
{
    FMOD_RESULT result;

    if (channel)
    {
	if (!initsound())
	    return;

	result = FMOD_Channel_Stop(channel);
	if (result != FMOD_OK) {
	    NSLog(@"FMOD: %s", FMOD_ErrorString(result)); return;
	}
	
	channel = NULL;
    }

    if (sound)
    {
	result = FMOD_Sound_Release(sound);
	if (result != FMOD_OK) {
	    NSLog(@"FMOD: %s", FMOD_ErrorString(result)); return;
	}
	
	sound = NULL;
    }

    repeats = 0;
    notify = 0;
}

@end
