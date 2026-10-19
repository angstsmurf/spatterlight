//
//  Blorb.m
//  Spatterlight
//
//  Based on code from Yazmin by David Schweinsberg
//  https://github.com/yazmin
//
//  Original code created by David Schweinsberg on 21/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import "Blorb.h"
#import "BlorbResource.h"
#import "FileSignature.h"
#include "iff.h"
#import <BlorbFramework/BlorbFramework-Swift.h>

// FORM header: 'FORM', length, 'IFRS'.
static const NSUInteger kFormHeaderLength = 12;
static const NSUInteger kChunkHeaderLength = 8;

// Four-character codes of the chunks we read.
enum : FourCharCode {
  kRIdx = 'RIdx',
  kIFmd = 'IFmd',
  kFspc = 'Fspc',
  kIFhd = 'IFhd',
  kRDes = 'RDes',
  kZCOD = 'ZCOD',
};

// Text chunks copied into optionalChunks, with their encodings.
static NSDictionary<NSNumber *, NSNumber *> *OptionalChunkEncodings(void) {
  static NSDictionary<NSNumber *, NSNumber *> *encodings;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    encodings = @{
      @((FourCharCode)'ANNO') : @(NSASCIIStringEncoding),
      @((FourCharCode)'AUTH') : @(NSASCIIStringEncoding),
      @((FourCharCode)'(c) ') : @(NSASCIIStringEncoding),
      // Story name, written as UTF-16BE by Inform and the Infocom Blorbs.
      @((FourCharCode)'SNam') : @(NSUTF16BigEndianStringEncoding),
    };
  });
  return encodings;
}

static NSString *FourCCString(FourCharCode code) {
  char bytes[4] = { (char)(code >> 24), (char)(code >> 16), (char)(code >> 8), (char)code };
  return [[NSString alloc] initWithBytes:bytes length:4 encoding:NSASCIIStringEncoding];
}

// The pictures that Narcolepsy, Dracula, Pytho's Mask and Mysterious
// Adventures would get as a fake frontispiece are really boring, so we skip
// them. Calls FileSignature directly rather than the -[NSData sha256String]
// category, so that Blorb does not depend on app-level categories that may
// not be compiled into every target using the framework. FileSignature.m is
// compiled into BlorbFramework itself.
static BOOL IsBoringCoverImage(NSData *imageData) {
  static NSSet<NSString *> *boring;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    boring = [NSSet setWithArray:@[
      // Dracula
      @"838B2519FABA7D04463A0F5FAAB2403D84F302D32402F6CFE8ED817AC7F5A37E",
      // Narcolepsy
      @"C198379C894FB4B9EC686EABC0AAF368101A7B4FAFB50ED8304967E66138A6D2",
      // Pytho's Mask
      @"B47C56D78D86908C4C590549B4848C4FFACDBE039830EF2EE9947F565D739CF8",
      // Mysterious Adventures
      @"657B68A30153E51B42436E256869ECB916B16973583ED68B63FA22C78B434AAA",
    ]];
  });
  return [boring containsObject:[FileSignature sha256StringFromData:imageData]];
}

@interface Blorb () {
  NSData *_data;
  NSUInteger _frontispiece;
  BOOL _hasFrontispiece;
  BOOL _coverImageResolved;
}
@end

@implementation Blorb

@synthesize coverImageData = _coverImageData;
@synthesize fakeFrontispiece = _fakeFrontispiece;

+ (BOOL)isBlorbURL:(nullable NSURL *)url {
  static NSSet<NSString *> *extensions;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    extensions = [NSSet setWithArray:@[@"zblorb", @"blorb", @"blb", @"gblorb", @"zlb"]];
  });
  NSString *pathExtension = url.pathExtension.lowercaseString;
  return pathExtension && [extensions containsObject:pathExtension];
}

+ (nullable instancetype)blorbAtURL:(nullable NSURL *)url {
  if (!url)
    return nil;
  @try {
    NSData *data = [NSData dataWithContentsOfURL:url
                                         options:NSDataReadingMappedIfSafe
                                           error:NULL];
    return [[self alloc] initWithData:data];
  } @catch (NSException *exception) {
    NSLog(@"Blorb: exception parsing %@: %@", url.path, exception);
    return nil;
  }
}

+ (nullable NSData *)coverImageDataAtURL:(nullable NSURL *)url {
  if (![self isBlorbURL:url])
    return nil;
  Blorb *blorb = [self blorbAtURL:url];
  @try {
    return blorb.coverImageData;
  } @catch (NSException *exception) {
    NSLog(@"Blorb: exception reading the cover of %@: %@", url.path, exception);
    return nil;
  }
}

+ (BOOL)isBlorbData:(nonnull NSData *)data {
  return isForm(data.bytes, data.length, 'I', 'F', 'R', 'S');
}

- (nullable instancetype)initWithData:(NSData *)aData {
  self = [super init];
  if (self) {
    if (aData.length < kFormHeaderLength)
      return nil;
    _data = aData;
    _optionalChunks = @{};
    _resources = @[];

    uint32_t chunkID, len;
    if (!chunkHeaderAt(_data.bytes, _data.length, kFormHeaderLength, &chunkID, &len) ||
        chunkID != kRIdx)
      return self;

    NSMutableArray<BlorbResource *> *resources = [NSMutableArray array];
    NSUInteger offset = [self parseResourceIndexAt:kFormHeaderLength + kChunkHeaderLength
                                        intoArray:resources];
    _resources = resources;
    [self parseChunksFromOffset:offset];
  }
  return self;
}

#pragma mark - Parsing

// Reads the RIdx count and entries starting at offset (just past the chunk
// header). Returns the offset after the last entry read, where the parser
// carries on looking for other chunks.
- (NSUInteger)parseResourceIndexAt:(NSUInteger)offset
                         intoArray:(NSMutableArray<BlorbResource *> *)resources {
  const unsigned char *base = _data.bytes;
  NSUInteger length = _data.length;
  if (length - offset < 4)
    return length;
  uint32_t count = unpackLong(base + offset);
  offset += 4;
  // Each index entry is 12 bytes; stop if the file is truncated.
  for (; count && length - offset >= 12; count--, offset += 12) {
    FourCharCode usage = unpackLong(base + offset);
    uint32_t number = unpackLong(base + offset + 4);
    uint32_t start = unpackLong(base + offset + 8);
    BlorbResource *resource = [[BlorbResource alloc] initWithUsage:usage
                                                            number:number
                                                             start:start];
    uint32_t type, unused;
    if (chunkHeaderAt(base, length, start, &type, &unused))
      resource.chunkType = FourCCString(type);
    [resources addObject:resource];
  }
  return offset;
}

// Walks the chunks that follow the resource index.
- (void)parseChunksFromOffset:(NSUInteger)offset {
  NSDictionary<NSNumber *, NSNumber *> *optionalEncodings = OptionalChunkEncodings();
  NSMutableDictionary<NSString *, NSString *> *optionalChunks = [NSMutableDictionary dictionary];

  uint32_t chunkID, len;
  // A chunk header is 8 bytes, so stop as soon as there is not a full header
  // left to read.
  while (chunkHeaderAt(_data.bytes, _data.length, offset, &chunkID, &len)) {
    NSUInteger payloadOffset = offset + kChunkHeaderLength;
    NSData *payload = [self safeSubdataFromOffset:payloadOffset length:len];

    if (chunkID == kIFmd) {
      // Treaty of Babel Metadata
      _metaData = payload;
    } else if (chunkID == kFspc) {
      // Frontispiece - Cover picture index
      if (_data.length - payloadOffset >= 4) {
        _frontispiece = unpackLong((const unsigned char *)_data.bytes + payloadOffset);
        _hasFrontispiece = YES;
      }
    } else if (optionalEncodings[@(chunkID)] != nil) {
      NSStringEncoding encoding = optionalEncodings[@(chunkID)].unsignedIntegerValue;
      NSString *text = payload ? [[NSString alloc] initWithData:payload encoding:encoding] : nil;
      if (text)
        optionalChunks[FourCCString(chunkID)] = text;
    } else if (chunkID == kIFhd) {
      [self parseIFhd:payload];
    } else if (chunkID == kRDes) {
      [self parseResourceDescriptionsAt:payloadOffset];
    }

    offset = payloadOffset + paddedLength(len);
  }
  _optionalChunks = optionalChunks;
}

// Game Identifier Chunk. These values only make sense in a Z-code game; see
// the comment on releaseNumber in Blorb.h.
- (void)parseIFhd:(nullable NSData *)payload {
  BlorbResource *resource = [self findResourceOfUsage:ExecutableResource];
  if (resource && ![resource.chunkType isEqualToString:FourCCString(kZCOD)])
    return;
  IFhdInfo info;
  if (!parseIFhd(payload.bytes, payload.length, &info))
    return;
  _releaseNumber = info.release;
  _serialNumber = [[NSString alloc] initWithBytes:info.serial length:6 encoding:NSASCIIStringEncoding];
  _checkSum = info.checksum;
}

// Resource Description Chunk: a count, then (usage, number, length, text)
// entries.
- (void)parseResourceDescriptionsAt:(NSUInteger)offset {
  const unsigned char *base = _data.bytes;
  NSUInteger length = _data.length;
  if (length - offset < 4)
    return;
  uint32_t count = unpackLong(base + offset);
  offset += 4;

  NSMutableDictionary<NSString *, BlorbResource *> *byKey = [NSMutableDictionary dictionary];
  for (BlorbResource *resource in _resources)
    byKey[[NSString stringWithFormat:@"%u.%u", resource.usage, resource.number]] = resource;

  // Each description entry is a 12-byte header plus its text.
  for (; count && length - offset >= 12; count--) {
    FourCharCode usage = unpackLong(base + offset);
    uint32_t number = unpackLong(base + offset + 4);
    uint32_t textLength = unpackLong(base + offset + 8);
    NSData *descData = [self safeSubdataFromOffset:offset + 12 length:textLength];
    if (!descData)
      break;
    offset += 12 + textLength;
    byKey[[NSString stringWithFormat:@"%u.%u", usage, number]].descriptiontext =
        [[NSString alloc] initWithData:descData encoding:NSUTF8StringEncoding];
  }
}

// Returns a subrange of the backing data, or nil if the range would fall
// outside the file. Used to protect against corrupt or truncated Blorbs whose
// index entries and chunk lengths point past the end of the data.
- (nullable NSData *)safeSubdataFromOffset:(NSUInteger)offset length:(NSUInteger)len {
  if (offset > _data.length || len > _data.length - offset)
    return nil;
  return [_data subdataWithRange:NSMakeRange(offset, len)];
}

// The payload of the chunk starting at offset, or nil if the chunk does not
// fit in the file. If expectedID is non-zero, the chunk must also have that ID.
- (nullable NSData *)payloadOfChunkAtOffset:(NSUInteger)offset expectedID:(FourCharCode)expectedID {
  uint32_t chunkID, len;
  if (!chunkHeaderAt(_data.bytes, _data.length, offset, &chunkID, &len))
    return nil;
  if (expectedID && chunkID != expectedID)
    return nil;
  return [self safeSubdataFromOffset:offset + kChunkHeaderLength length:len];
}

#pragma mark - Resources

- (nullable NSData *)dataForResource:(BlorbResource *)resource {
  return [self payloadOfChunkAtOffset:resource.start expectedID:0];
}

- (NSArray<BlorbResource *> *)resourcesForUsage:(FourCharCode)usage {
  NSMutableArray<BlorbResource *> *array = [NSMutableArray array];
  for (BlorbResource *resource in _resources) {
    if (resource.usage == usage)
      [array addObject:resource];
  }
  return array;
}

- (nullable BlorbResource *)findResourceOfUsage:(FourCharCode)usage {
  for (BlorbResource *resource in _resources) {
    if (resource.usage == usage)
      return resource;
  }
  return nil;
}

- (nullable NSData *)zcodeData {
  BlorbResource *resource = [self findResourceOfUsage:ExecutableResource];
  return resource ? [self payloadOfChunkAtOffset:resource.start expectedID:kZCOD] : nil;
}

- (nullable NSData *)pictureData {
  BlorbResource *resource = [self findResourceOfUsage:PictureResource];
  return resource ? [self dataForResource:resource] : nil;
}

#pragma mark - Cover image

// The Fspc picture if there is one. Otherwise (or if Fspc names a picture
// that is missing or unreadable) the first picture in index order that is
// readable and not on the boring list, flagged as fake. Worked out once, so
// repeated calls return the same answer.
- (void)resolveCoverImage {
  if (_coverImageResolved)
    return;
  _coverImageResolved = YES;

  NSArray<BlorbResource *> *images = [self resourcesForUsage:PictureResource];
  if (_hasFrontispiece) {
    for (BlorbResource *res in images) {
      if (res.number == _frontispiece) {
        _coverImageData = [self dataForResource:res];
        if (_coverImageData)
          return;
        break;
      }
    }
  }

  for (BlorbResource *res in images) {
    NSData *imageData = [self dataForResource:res];
    if (imageData && !IsBoringCoverImage(imageData)) {
      _coverImageData = imageData;
      _fakeFrontispiece = YES;
      return;
    }
  }
}

- (nullable NSData *)coverImageData {
  [self resolveCoverImage];
  return _coverImageData;
}

- (BOOL)fakeFrontispiece {
  [self resolveCoverImage];
  return _fakeFrontispiece;
}

#pragma mark - IFID

+ (NSString *)zcodeIFIDWithRelease:(NSUInteger)release
                            serial:(NSString *)serial
                          checksum:(NSUInteger)checksum {
  NSString *serialString = serial;
  if (serialString.length >= 6)
    serialString = [serialString stringByReplacingOccurrencesOfString:@"[^0-Z]" withString:@"-" options:NSRegularExpressionSearch range:NSMakeRange(0, 6)];

  NSInteger date = serialString.intValue;
  if ((date < 700000 || date > 900000) && date != 0 && date != 999999)
    return [NSString stringWithFormat:@"ZCODE-%ld-%@-%04lX", release, serialString, (unsigned long)checksum];
  return [NSString stringWithFormat:@"ZCODE-%ld-%@", release, serialString];
}

- (nullable NSString *)ifidFromIFhd {
  if (!_serialNumber && _releaseNumber == 0 && _checkSum == 0)
    return nil;
  return [Blorb zcodeIFIDWithRelease:_releaseNumber serial:_serialNumber ?: @"" checksum:_checkSum];
}

@end
