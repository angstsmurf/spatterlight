//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import <Cocoa/Cocoa.h>

@class IFIdentification;
@class IFBibliographic;
@class IFDB;

@interface IFStory : NSObject

@property(readonly) IFIdentification *identification;
@property(readonly) IFBibliographic *bibliographic;
@property(readonly) IFDB *ifdb;

- (instancetype)initWithXMLElement:(NSXMLElement *)element andContext:(NSManagedObjectContext *)context;

@end