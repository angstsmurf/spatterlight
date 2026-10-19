//
//  Blorb.h
//  Spatterlight
//
//  Based on code from Yazmin by David Schweinsberg
//  https://github.com/yazmin
//
//  Original code created by David Schweinsberg on 21/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class BlorbResource;

@interface Blorb : NSObject

@property(readonly) NSArray<BlorbResource *> *resources;
@property(readonly, nullable) NSData *zcodeData;
@property(readonly, nullable) NSData *pictureData;
@property(readonly, nullable) NSData *metaData;

// From the IFhd chunk. Only read for Z-code Blorbs (or Blorbs without an
// executable): Glulx identifies a game by the first 128 bytes of the story
// file instead, and that is left to Babel.
@property(readonly) NSUInteger releaseNumber;
@property(readonly) NSUInteger checkSum;
@property(readonly, nullable) NSString *serialNumber;

// Text chunks keyed by chunk ID: ANNO, AUTH, "(c) " and SNam.
@property(readonly) NSDictionary<NSString *, NSString *> *optionalChunks;

// The image returned by coverImageData, and whether it was picked from the
// pictures because the file has no Fspc chunk.
@property(readonly, nullable) NSData *coverImageData;
@property(readonly) BOOL fakeFrontispiece;

// ZCODE-release-serial[-checksum], or nil if there is no IFhd chunk.
@property(readonly, nullable) NSString *ifidFromIFhd;

+ (BOOL)isBlorbURL:(nullable NSURL *)url;

// Reads and parses the file at url. Returns nil if it can't be read, is too
// short, or the parser throws, so a malformed file only means "no Blorb".
+ (nullable instancetype)blorbAtURL:(nullable NSURL *)url;
// The cover image of the Blorb at url, or nil if url has no Blorb extension
// or blorbAtURL: fails.
+ (nullable NSData *)coverImageDataAtURL:(nullable NSURL *)url;
// Checks for a FORM IFRS header.
+ (BOOL)isBlorbData:(NSData *)data;

// The Treaty of Babel IFID for a Z-code game, as built from its IFhd or
// story file header fields. Shared with the Quetzal save code.
+ (NSString *)zcodeIFIDWithRelease:(NSUInteger)release
                            serial:(NSString *)serial
                          checksum:(NSUInteger)checksum;

// Returns nil for data too short to hold a FORM header and a chunk. Does not
// check that the FORM type is IFRS; use isBlorbData: for that.
- (nullable instancetype)initWithData:(nullable NSData *)data NS_DESIGNATED_INITIALIZER;
- (instancetype)init __attribute__((unavailable));

- (nullable NSData *)dataForResource:(BlorbResource *)resource;
- (NSArray<BlorbResource *> *)resourcesForUsage:(FourCharCode)usage;
- (nullable BlorbResource *)findResourceOfUsage:(FourCharCode)usage;

@end

NS_ASSUME_NONNULL_END
