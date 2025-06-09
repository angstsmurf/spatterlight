//
//  IFBibliographic.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Cocoa/Cocoa.h>

@class Metadata;

@interface IFBibliographic : NSObject

- (instancetype _Nullable)initWithXMLElement:(NSXMLElement * _Nonnull)element;
- (void)addInfoToMetadata:(Metadata * _Nonnull)metadata;

@property(nullable) NSString *title;
@property(nullable) NSString *author;
@property(nullable) NSString *language;
@property(nullable) NSString *languageAsWord;
@property(nullable) NSString *headline;
@property(nullable) NSString *firstPublished;
@property(nullable) NSDate *firstPublishedDate;
@property(nullable) NSString *genre;
@property(nullable) NSString *group;
@property(nullable) NSString *series;
@property(nullable) NSString *seriesnumber;
@property(nullable) NSString *forgiveness;
@property(nullable) NSNumber *forgivenessNumeric;
@property(nullable) NSString *blurb;

@end
