//
//  AudioResourceHandler.h
//  Spatterlight
//
//  Created by Administrator on 2021-01-24.
//

#import <Foundation/Foundation.h>

@class GlkSoundChannel;

typedef enum kBlorbSoundFormatType : NSInteger {
    NONE,
    giblorb_ID_MOD,
    giblorb_ID_OGG,
    giblorb_ID_FORM,
    giblorb_ID_AIFF,
    giblorb_ID_MP3,
    giblorb_ID_WAVE,
    giblorb_ID_MIDI
} kBlorbSoundFormatType;

#define FREE 1
#define BUSY 2

NS_ASSUME_NONNULL_BEGIN

@interface SoundResource : NSObject

@property (nullable) NSData *data;
@property size_t length;
@property (nullable) NSURL *filename;
@property NSInteger offset;
@property BOOL loadedflag;
@property kBlorbSoundFormatType type;

@end


@interface AudioResourceHandler : NSObject

@property NSMutableDictionary <NSNumber *, SoundResource *> *my_resources;
@property NSMutableDictionary <NSNumber *, GlkSoundChannel *> *sound_channels;
@property (nullable) GlkSoundChannel *music_channel;
@property NSUInteger restored_music_channel_id;


- (void)initializeSound;

- (void)setSoundResource:(NSInteger)snd type:(kBlorbSoundFormatType)type data:(NSData *)data length:(NSUInteger)length filename:(nullable NSString *)filename offset:(NSInteger)offset;

-(NSInteger)load_sound_resource:(NSInteger)snd length:(NSUInteger *)len data:(char * _Nonnull * _Nonnull)buf;

- (void)stopAllAndCleanUp:(NSArray *)channels;

- (BOOL)soundIsLoaded:(NSInteger)soundId;

@end

NS_ASSUME_NONNULL_END
