#import "main.h"

@implementation GlkSoundChannel

- (id) initWithGlkController: (GlkController*)glkctl_ name: (int)name
{
    [super init];
    
    glkctl = glkctl_;
    
    sound = nil;
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
    if (![Preferences soundEnabled])
	return;
    
    [self stop];
    
    sound = [[NSSound alloc] initWithData: sample];
    if (!sound)
    {
	NSLog(@"could not create sound effect");
	return;
    }
    
    repeats = areps;
    notify = anot;
    
    [(id)sound setDelegate: self];
    [(id)sound play];
}

- (void) setVolume: (int)avol
{
    volume = avol;
    
    if (sound)
    {
    }
}

- (void) stop
{
    repeats = 0;
    notify = 0;

    if (sound)
    {
	[sound stop];
	[sound release];
	sound = nil;
    }
}

- (void) sound: (NSSound*)asound didFinishPlaying: (BOOL)finished
{
    if (finished)
    {
	if (repeats < 0)
	    [sound play];
	else if (repeats > 0)
	{
	    repeats --;
	    if (repeats > 0)
	    {
		[sound play];
	    }
	}
    }
}

@end
