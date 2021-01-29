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

@interface SoundFile : NSObject

- (instancetype)initWithPath:(NSString *)path;
- (void)resolveBookmark;

@property (nullable) NSData *bookmark;
@property (nullable) NSURL *URL;

@end;


@interface SoundResource : NSObject

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length;

-(BOOL)load;

@property (nullable) NSData *data;
@property (nullable) SoundFile *soundFile;
@property NSString *filename;
@property NSUInteger offset;
@property NSUInteger length;
@property kBlorbSoundFormatType type;

@end


@interface AudioResourceHandler : NSObject

@property NSMutableDictionary <NSNumber *, SoundResource *> *resources;
@property NSMutableDictionary <NSNumber *, GlkSoundChannel *> *sound_channels;
@property NSMutableDictionary <NSString *, SoundFile *> *files;
@property (nullable) GlkSoundChannel *music_channel;
@property NSUInteger restored_music_channel_id;


- (void)initializeSound;

- (void)setSoundID:(NSInteger)snd filename:(nullable NSString *)filename length:(NSUInteger)length offset:(NSUInteger)offset;

-(NSInteger)load_sound_resource:(NSInteger)snd length:(NSUInteger *)len data:(char * _Nonnull * _Nonnull)buf;

- (void)stopAllAndCleanUp:(NSArray *)channels;
- (BOOL)soundIsLoaded:(NSInteger)soundId;

@end

NS_ASSUME_NONNULL_END
