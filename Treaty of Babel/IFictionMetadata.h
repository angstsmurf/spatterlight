//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Cocoa/Cocoa.h>

@class IFStory;

@interface IFictionMetadata : NSObject

@property(readonly) NSArray *stories;

- (instancetype)initWithData:(NSData *)data;

@end
