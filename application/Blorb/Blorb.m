//
//  Blorb.m
//  Yazmin
//
//  Created by David Schweinsberg on 21/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import "Blorb.h"
#import "BlorbResource.h"
#include "iff.h"

@interface Blorb () {
  NSData *data;
  NSData *metaData;
  NSInteger frontispiece;
}

- (nullable BlorbResource *)findResourceOfUsage:(unsigned int)usage;

@end

@implementation Blorb

+ (BOOL)isBlorbURL:(nonnull NSURL *)url {
  NSString *pathExtension = url.pathExtension.lowercaseString;
  if ([pathExtension isEqualToString:@"zblorb"] ||
      [pathExtension isEqualToString:@"blorb"] ||
      [pathExtension isEqualToString:@"blb"] ||
      [pathExtension isEqualToString:@"gblorb"] ||
      [pathExtension isEqualToString:@"zlb"])
    return YES;
  else
    return NO;
}

+ (BOOL)isBlorbData:(nonnull NSData *)data {
  const void *ptr = data.bytes;
  if (isForm(ptr, 'I', 'F', 'R', 'S'))
    return YES;
  else
    return NO;
}

- (instancetype)initWithData:(NSData *)aData {
  self = [super init];
  if (self) {
    if (aData.length < 13)
      return nil;
    data = aData;
    metaData = nil;
    frontispiece = 0;
    NSMutableArray<BlorbResource *> *resources = [[NSMutableArray alloc] init];

    NSArray *optionalChunksIDs = @[@"ANNO", @"AUTH", @"(c) ", @"SNam"];
    _optionalChunks = [NSMutableDictionary new];

    const unsigned char *ptr = data.bytes;
    ptr += 12;
    unsigned int chunkID;
    chunkIDAndLength(ptr, &chunkID);
    if (chunkID == IFFID('R', 'I', 'd', 'x')) {
      ptr += 8;
      unsigned int count = unpackLong(ptr);
      ptr += 4;
      while (count--) {
        unsigned int usage = unpackLong(ptr);
        unsigned int number = unpackLong(ptr + 4);
        unsigned int start = unpackLong(ptr + 8);
        ptr += 12;
        [resources addObject:[[BlorbResource alloc] initWithUsage:usage
                                                           number:number
                                                            start:start]];
        resources.lastObject.chunkType = [self stringFromChunkWithPtr:data.bytes + start];
      }

      // Look through the rest of the file
      while (ptr < (const unsigned char *)data.bytes + data.length) {
        unsigned int len = chunkIDAndLength(ptr, &chunkID);
        NSString *chunkString = [self stringFromChunkWithPtr:ptr];
        if (chunkID == IFFID('I', 'F', 'm', 'd')) {
          // Treaty of Babel Metadata
          NSRange range =
          NSMakeRange((NSUInteger)(ptr - (const unsigned char *)data.bytes + 8), len);
          metaData = [data subdataWithRange:range];
          //          NSLog(@"Found metadata. len:%d", len);
        }

        else if (chunkID == IFFID('F', 's', 'p', 'c')) {
          // Frontispiece - Cover picture index
          frontispiece  = unpackLong(ptr + 8);
          //          NSLog(@"Found frontispiece chunk with a value of %ld", frontispiece);
        }

        else if ([optionalChunksIDs indexOfObject:chunkString] != NSNotFound) {
          NSRange range =
          NSMakeRange((NSUInteger)(ptr - (const unsigned char *)data.bytes + 8), len);

          NSStringEncoding encoding = NSASCIIStringEncoding;
          if ([chunkString isEqualToString:@"Snam"])
            encoding = NSUTF16BigEndianStringEncoding;

          _optionalChunks[chunkString] = [[NSString alloc] initWithData:[data subdataWithRange:range] encoding:encoding];
          //          NSLog(@"Found %@ chunk with value \"%@\"", chunkString, _optionalChunks[chunkString]);
        }
        else if (chunkID == IFFID('I', 'F', 'h', 'd')) {
          // Game Identifier Chunk

          // These values only make sense in a ZCode game
          // Glulx uses the first 128 bytes of th file,
          // but we don't handle that here (yet)
          BlorbResource *resource = [self findResourceOfUsage:ExecutableResource];
          if (!resource || [resource.chunkType isEqualToString:@"ZCOD"]) {
            _releaseNumber  = unpackShort(ptr + 8);
            NSData *serialData = [NSData dataWithBytes:ptr + 10 length:6];
            _serialNumber = [[NSString alloc] initWithData:serialData encoding:NSASCIIStringEncoding];
            _checkSum = unpackShort(ptr + 16);
          }
        }

        else if (chunkID == IFFID('R', 'D', 'e', 's')) {
          NSLog(@"Found Resource Description Chunk!");
          ptr += 8;
          count = unpackLong(ptr);
          ptr += 4;
          while (count--) {
            unsigned int usage = unpackLong(ptr);
            unsigned int number = unpackLong(ptr + 4);
            unsigned int length = unpackLong(ptr + 8);
            NSRange range =
            NSMakeRange((NSUInteger)(ptr + 12 - (const unsigned char *)data.bytes), length);
            NSString *description = [[NSString alloc] initWithData:[data subdataWithRange:range] encoding:NSUTF8StringEncoding];
            ptr += 12 + length;
            for (BlorbResource *resource in _resources) {
              BOOL found = NO;
              if (resource.usage == usage && resource.number == number) {
                resource.descriptiontext = description;
                found = YES;
                break;
              }
              if (!found)
                NSLog(@"Error! Found no resource with usage %d and number %d", usage, number);
            }
          }
        }

        ptr += paddedLength(len) + 8;
      }
    }
    _resources = resources;
  }
  return self;
}

- (NSString *)stringFromChunkWithPtr:(const unsigned char *)ptr {
  NSRange range =
  NSMakeRange((NSUInteger)(ptr - (const unsigned char *)data.bytes), 4);
  NSData *subData = [data subdataWithRange:range];
  return [[NSString alloc] initWithData:subData encoding:NSASCIIStringEncoding];
}

- (NSData *)dataForResource:(BlorbResource *)resource {
  const unsigned char *ptr = data.bytes;
  ptr += resource.start;
  unsigned int chunkID;
  unsigned int len = chunkIDAndLength(ptr, &chunkID);
  NSRange range = NSMakeRange(resource.start + 8, len);
  return [data subdataWithRange:range];
}

- (NSArray<BlorbResource *> *)resourcesForUsage:(unsigned int)usage {
  NSMutableArray<BlorbResource *> *array = [NSMutableArray array];
  for (NSUInteger i = 0; i < _resources.count; ++i) {
    BlorbResource *resource = _resources[i];
    if (resource.usage == usage)
      [array addObject:resource];
  }
  return array;
}

- (BlorbResource *)findResourceOfUsage:(unsigned int)usage {
  for (NSUInteger i = 0; i < _resources.count; ++i) {
    BlorbResource *resource = _resources[i];
    if ([resource usage] == usage)
      return resource;
  }
  return nil;
}

- (NSData *)zcodeData {
  // Look up the executable chunk
  BlorbResource *resource = [self findResourceOfUsage:ExecutableResource];
  if (resource) {
    const unsigned char *ptr = data.bytes;
    ptr += [resource start];
    unsigned int chunkID;
    unsigned int len = chunkIDAndLength(ptr, &chunkID);
    if (chunkID == IFFID('Z', 'C', 'O', 'D')) {
      NSRange range = NSMakeRange([resource start] + 8, len);
      return [data subdataWithRange:range];
    }
  }
  return nil;
}

- (NSData *)pictureData {
  // Look up the picture chunk
  BlorbResource *resource = [self findResourceOfUsage:PictureResource];
  if (resource) {
    const unsigned char *ptr = data.bytes;
    ptr += [resource start];
    unsigned int chunkID;
    unsigned int len = chunkIDAndLength(ptr, &chunkID);
    NSRange range = NSMakeRange([resource start] + 8, len);
    return [data subdataWithRange:range];
  }
  return nil;
}

- (NSData *)metaData {
  return metaData;
}

- (NSData *)coverImageData {
  NSArray<BlorbResource *> *images = [self resourcesForUsage:PictureResource];

  if (!frontispiece && images.count) {
    frontispiece = images.firstObject.number;
    NSLog(@"Providing a fake frontispiece index of %ld", frontispiece);
  }

  for (BlorbResource *res in images)
    if (res.number == frontispiece) {
      return [self dataForResource:res];
    }
  return nil;
}

- (NSString *)ifidFromIFhd {
  NSString *zcodeifid = nil;
  if (!_serialNumber && _releaseNumber == 0 && _checkSum == 0)
    return nil;

  NSInteger date = _serialNumber.intValue;
  
  NSString *serialString = [_serialNumber stringByReplacingOccurrencesOfString:@"[^0-Z]" withString:@"-" options:NSRegularExpressionSearch range:NSMakeRange(0, 6)];

  if ((date < 700000 || date > 900000) && date != 0 && date != 999999) {
    zcodeifid = [NSString stringWithFormat:@"ZCODE-%ld-%@-%04lX", _releaseNumber, serialString, (unsigned long)_checkSum];
  } else {
    zcodeifid = [NSString stringWithFormat:@"ZCODE-%ld-%@", _releaseNumber, serialString];
  }
  return zcodeifid;
}

//(NSUInteger)zChecksumFromData:(NSData *)data {
////      The checksum is the unsigned sum mod 65536 of the bytes in the
////      story file from 0x0040 (first byte after header) to the end.
//
//   NSInteger zversion = ((const unsigned char *)(data.bytes))[0];
//
//   NSUInteger file_length = word(data, 0x1a) * (zversion <= 3 ? 2UL : zversion <= 5 ? 4UL : 8UL);
//   if(file_length > data.length) {
//       NSLog(@"story's reported size (%lu) greater than file size (%lu)", file_length, data.length);
//       file_length = data.length;
//   }
//
//   if(file_length < 100) {
//       NSLog(@"story's reported size too small (%lu)", file_length);
//       file_length = data.length;
//   }
//
//   NSUInteger checksum = 0;
//   for (NSUInteger i = 0x40; i < file_length; i++) {
//       checksum += (NSUInteger)((const unsigned char *)(data.bytes))[i];
//   }
//
//   checksum = checksum % 65536;
//   return checksum;
//}

@end
