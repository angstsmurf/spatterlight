//
//  IFStory.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import <Cocoa/Cocoa.h>

@class IFIdentification, IFBibliographic, IFCoverDescription, IFDB, Metadata;

@interface IFStory : NSObject

@property(readonly) IFIdentification *identification;
@property(readonly) IFBibliographic *bibliographic;
@property(readonly) IFCoverDescription *coverDescription;
@property(readonly) IFDB *ifdb;

- (instancetype)initWithXMLElement:(NSXMLElement *)element;
- (void)addInfoToMetadata:(Metadata *)metadata;

@end
