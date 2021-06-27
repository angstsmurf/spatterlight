//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import <Foundation/Foundation.h>

@class Metadata;

@interface IFDB : NSObject

+ (NSString *)pathFromCoverArtElement:(NSXMLElement *)element;

- (instancetype)initWithXMLElement:(NSXMLElement *)element andMetadata:(Metadata *)metadata;

@end
