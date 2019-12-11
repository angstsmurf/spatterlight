//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Cocoa/Cocoa.h>

@class Metadata;

@interface IFBibliographic : NSObject

- (instancetype)initWithXMLElement:(NSXMLElement *)element andMetadata:(Metadata *)metadata;

@end