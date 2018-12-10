#import "main.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

#import "SoundFader.h"

@implementation SoundFader

- (instancetype) initWithSoundChannel: (NSInteger)achannel startVolume:(NSInteger)startVol targetVolume:(NSInteger)targetVol duration:(NSInteger)duration notify:(NSInteger)anotify sender:(id)sender
{
	self = [super init];

	if (self)
	{
		glkctl = sender;
		notify = anotify;
		channel = achannel;
		targetVolume = targetVol;
		currentVolume = startVol >= SDL_MIX_MAXVOLUME ? GLK_MAXVOLUME : expf(logf((float)startVol/SDL_MIX_MAXVOLUME)/logf(4)) * GLK_MAXVOLUME;
		// This ridiculous calculation seems to be necessary to convert the SDL volume back to the correct Glk volume.

//		NSLog(@"Start volume is %ld. Recalculated to the Glk volume scale with a max of 0x10000 (%d), this is %f", (long)startVol, 0x10000, currentVolume);

		increment = (targetVol - currentVolume) * 300 / duration;
		timer = [NSTimer scheduledTimerWithTimeInterval: (0.01 * 300 / duration) target: self selector: @selector(incrementFader:) userInfo: nil repeats: YES];

		NSLog(@"Started a fader timer firing every centisecond with a delta of %f, changing the volume from %f to %ld in %ld milliseconds, firing a total of %ld times.", increment, currentVolume, (long)targetVolume, (long)duration, (long)(duration)/10);

		self.kill = NO;
	}

	return self;
}

- (void) incrementFader: (NSTimer *)timer
{
	GlkEvent *gev;

	NSInteger realVolumeOld = currentVolume >= GLK_MAXVOLUME ? SDL_MIX_MAXVOLUME : round(pow(((double) currentVolume) / GLK_MAXVOLUME, log(4)) * SDL_MIX_MAXVOLUME);

//	NSLog(@"On the Sdl audio scale, the previous volume (%f) was %ld.", currentVolume, realVolumeOld);

	currentVolume = currentVolume + increment;
	if (currentVolume < 0)
		currentVolume = 0;

//	NSLog(@"Incremented volume with %f to %f", increment, currentVolume);

	NSInteger realVolumeNew = currentVolume >= GLK_MAXVOLUME ? SDL_MIX_MAXVOLUME : round(pow(((double) currentVolume) / GLK_MAXVOLUME, log(4)) * SDL_MIX_MAXVOLUME);

//	NSLog(@"On the Sdl audio scale, the new volume (%f) is %ld.", currentVolume, (long)realVolumeNew);

	if (realVolumeOld != realVolumeNew)
	{
		if ((increment > 0 && realVolumeNew > realVolumeOld) || (increment < 0 && realVolumeNew < realVolumeOld)) // Not sure this is necessary
		{
		gev = [[GlkEvent alloc] initFadeEvent:channel andVolume:currentVolume];
		[glkctl queueEvent: gev];
		}
	}

	if ((increment > 0 && currentVolume >= targetVolume) || (increment < 0 && currentVolume <= targetVolume))
	{
		NSLog(@"That concludes the fade.");

		gev = [[GlkEvent alloc] initFadeEvent:channel andVolume:targetVolume];
		[glkctl queueEvent: gev];

		[timer invalidate];
		self.kill = YES;

		if (notify)
		{
			gev = [[GlkEvent alloc] initVolumeNotify:notify];
			[glkctl queueEvent: gev];
		}
	}
}

@end
