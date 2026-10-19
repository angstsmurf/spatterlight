//
//  IFBibliographic.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

//! Returns the language code for the contents of an iFiction <language>
//! element, and its English name in languageAsWord when there is one.
NSString *IFLanguageCodeFromString(NSString *raw, NSString * _Nullable * _Nullable languageAsWord);

@interface IFBibliographic : NSObject

- (instancetype)initWithXMLElement:(NSXMLElement *)element;

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

NS_ASSUME_NONNULL_END
