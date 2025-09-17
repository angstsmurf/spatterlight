//
//  IFBibliographic.m
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import "IFBibliographic.h"
#import "Metadata.h"
#import "Game.h"
#import "NSString+Categories.h"

typedef NS_ENUM(NSInteger, kForgiveness) {
    FORGIVENESS_NONE,
    FORGIVENESS_CRUEL,
    FORGIVENESS_NASTY,
    FORGIVENESS_TOUGH,
    FORGIVENESS_POLITE,
    FORGIVENESS_MERCIFUL
};

@interface IFBibliographic ()

- (NSString *)renderDescriptionElement:(NSXMLElement *)element;

@end

@implementation IFBibliographic

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
    self = [super init];
    if (self) {
        NSLocale *englishUSLocale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];

        NSDictionary *forgiveness = @{
            @"" : @(FORGIVENESS_NONE),
            @"Cruel" : @(FORGIVENESS_CRUEL),
            @"Nasty" : @(FORGIVENESS_NASTY),
            @"Tough" : @(FORGIVENESS_TOUGH),
            @"Polite" : @(FORGIVENESS_POLITE),
            @"Merciful" : @(FORGIVENESS_MERCIFUL)
        };

        NSDateFormatter *dateFormatter;

        for (NSXMLNode *node in element.children) {
            NSString *keyVal = node.stringValue;
            NSString *key = node.name;

            if ([key isEqualToString:@"firstpublished"] || [key isEqualToString:@"year"]) {
                if (dateFormatter == nil)
                    dateFormatter = [[NSDateFormatter alloc] init];
                if (keyVal.length == 4)
                    dateFormatter.dateFormat = @"yyyy";
                else if (keyVal.length == 10)
                    dateFormatter.dateFormat = @"yyyy-MM-dd";
                else NSLog(@"Illegal date format!");
                _firstPublishedDate = [dateFormatter dateFromString:keyVal];
                dateFormatter.dateFormat = @"yyyy";
                _firstPublished = [dateFormatter stringFromDate:_firstPublishedDate];
            } else if ([key isEqualToString:@"language"]) {
                // In IFDB xml data, "language" is usually a language code such as "en"
                // but may also be a locale code such as "en-US" or a descriptive string
                // like "English, French (en, fr)." We try to deal with all of them here.
                NSString *languageCode = keyVal;
                if (languageCode.length > 1) {
                    if (languageCode.length > 3) {
                        // If it is longer than three characters, we use the first two
                        // as language code. This seems to cover all known cases.
                        languageCode = [languageCode substringToIndex:2];
                    }
                    NSString *language = [englishUSLocale displayNameForKey:NSLocaleLanguageCode value:languageCode];
                    if (language) {
                        _languageAsWord = language;
                    } else {
                        // Otherwise we use the full string for both language and languageAsWord.
                        _languageAsWord = keyVal;
                        languageCode = keyVal;
                    }
                }
                _language = languageCode;
            } else if ([key isEqualToString:@"forgiveness"]) {
                _forgivenessNumeric = forgiveness[keyVal];
            } else if ([key isEqualToString:@"title"]) {
                NSString *title = keyVal.stringByDecodingXMLEntities;
                if (title.length)
                    _title = title;
            } else if ([key isEqualToString:@"headline"]) {
                _headline = [keyVal stringByReplacingOccurrencesOfString:@"[em dash]" withString:@"-"];
            } else if ([key isEqualToString:@"author"]) {
                _author = keyVal;
            } else if ([key isEqualToString:@"genre"]) {
                _genre = keyVal;
            } else if ([key isEqualToString:@"group"]) {
                _group = keyVal;
            } else if ([key isEqualToString:@"series"]) {
                _series = keyVal;
            } else if ([key isEqualToString:@"seriesnumber"]) {
                _seriesnumber = keyVal;
            } else if ([key isEqualToString:@"description"] || [key isEqualToString:@"teaser"]) {
                _blurb =
                [self renderDescriptionElement:(NSXMLElement *)node];
                _blurb =
                [_blurb stringByDecodingXMLEntities];
                _blurb = [_blurb stringByReplacingOccurrencesOfString:@"<br/>" withString:@"\n\n"];

                // When importing Return to Ditch Day, Babel returns a text with \\n instead of line breaks.
                _blurb =
                [_blurb stringByReplacingOccurrencesOfString:@"\\n" withString:@"\n"];
            } else {
                NSLog(@"Unhandled node name:%@ value: %@", key, keyVal);
            }
        }
    }
    return self;
}

- (instancetype)init {
    self = [super init];
    if (self) {
    }
    return self;
}

- (NSString *)renderDescriptionElement:(NSXMLElement *)element {
    NSMutableString *string = [NSMutableString string];
    NSUInteger count = 0;
    for (NSXMLNode *node in element.children) {
        if (node.kind == NSXMLTextKind) {
            if (count > 0)
                [string appendString:@"\n\n"];
            [string appendString:node.stringValue];
            ++count;
        }
    }
    return string;
}

- (void)addInfoToMetadata:(Metadata *)metadata {
    if (_title.length)
        metadata.title = _title;
    if (_author.length)
        metadata.author = _author;
    if (_language.length)
        metadata.language = _language;
    if (_languageAsWord.length)
        metadata.languageAsWord = _languageAsWord;
    if (_headline.length)
        metadata.headline = _headline;
    if (_blurb.length)
        metadata.blurb = _blurb;
    if (_firstPublished.length)
        metadata.headline = _firstPublished;
    if (_firstPublishedDate)
        metadata.firstpublishedDate = _firstPublishedDate;
    if (_genre.length)
        metadata.genre = _genre;
    if (_group.length)
        metadata.group = _group;
    if (_series.length)
        metadata.series = _series;
    if (_seriesnumber.length)
        metadata.seriesnumber = _seriesnumber;
    if (_forgiveness.length)
        metadata.forgiveness = _forgiveness;
    if (_forgivenessNumeric)
        metadata.forgivenessNumeric = _forgivenessNumeric;
}


@end
