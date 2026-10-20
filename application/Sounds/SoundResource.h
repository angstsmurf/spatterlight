//
//  SoundResource.h
//  Spatterlight
//
//  A sound a game has declared: where its bytes are (a file, with a bookmark
//  that survives the file being moved, plus an offset and length), its format,
//  and once read, the bytes themselves.
//

#import <Foundation/Foundation.h>

@class SoundHandler;

/* The Glk integer types, for the sound code; glk.h has the same. */
typedef uint32_t glui32;
typedef int32_t glsi32;

/* Archived as raw values: never reorder. */
typedef NS_ENUM(NSInteger, GlkSoundBlorbFormatType) {
    GlkSoundBlorbFormatNone,
    /* Any tracker module DUMB reads: MOD, S3M, XM or IT. */
    GlkSoundBlorbFormatMod,
    GlkSoundBlorbFormatOggVorbis,
    /* AIFF, which is an IFF FORM. */
    GlkSoundBlorbFormatFORM,
    /* Never produced (sniffing reports AIFF as FORM); kept so that the
       values after it keep their numbers. */
    GlkSoundBlorbFormatAIFF,
    GlkSoundBlorbFormatMP3,
    GlkSoundBlorbFormatWave,
    GlkSoundBlorbFormatMIDI,
};

NS_ASSUME_NONNULL_BEGIN

@interface SoundFile : NSObject

- (instancetype)initWithPath:(NSString *)path;
- (void)resolveBookmark;

@property (nullable) NSData *bookmark;
@property (nullable) NSURL *URL;
@property (weak) SoundHandler *handler;

@end


@interface SoundResource : NSObject

- (instancetype)initWithFilename:(NSString *)filename offset:(NSUInteger)offset length:(NSUInteger)length;

@property (NS_NONATOMIC_IOSONLY, readonly) BOOL load;

@property (copy, nullable) NSData *data;
@property (strong, nullable) SoundFile *soundFile;
@property NSString *filename;
@property NSUInteger offset;
@property NSUInteger length;
@property GlkSoundBlorbFormatType type;
+ (GlkSoundBlorbFormatType)detectSoundFormatFromData:(NSData*)data;

@end

NS_ASSUME_NONNULL_END
