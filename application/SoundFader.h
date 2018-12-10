//
//  SoundFader.h
//  Spatterlight
//
//  Created by Administrator on 2018-09-20.
//

#import <Foundation/Foundation.h>

#import "GlkEvent.h"
#import "GlkController.h"

#define SDL_MIX_MAXVOLUME 128
#define GLK_MAXVOLUME 65536

@interface SoundFader : NSObject
{
	NSInteger channel;
	NSInteger notify;
	NSInteger targetVolume;
	CGFloat currentVolume;
	CGFloat increment;
	NSTimer *timer;
	GlkController *glkctl;
}

@property BOOL kill;

- (instancetype) initWithSoundChannel: (NSInteger)achannel startVolume:(NSInteger)startVol targetVolume:(NSInteger)targetVol duration:(NSInteger)duration notify:(NSInteger)notify sender:(id)sender NS_DESIGNATED_INITIALIZER;
@end
