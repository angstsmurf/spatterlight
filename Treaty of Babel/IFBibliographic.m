//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import "IFBibliographic.h"
#import "Metadata.h"
#import "NSString+Categories.h"
#import "main.h"

enum  {
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

- (instancetype)initWithXMLElement:(NSXMLElement *)element andMetadata:(Metadata *)metadata {
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

        NSEnumerator *enumChildren = [element.children objectEnumerator];
        NSXMLNode *node;
        NSString *keyVal;
        NSArray *allKeys = [[[metadata entity] attributesByName] allKeys];
        while ((node = [enumChildren nextObject])) {
            keyVal = node.stringValue;
            for (NSString *key in allKeys) {
    
                if ([node.name compare:key] == 0) {
                    if ([key isEqualToString:@"firstpublished"])
                    {
                        if (dateFormatter == nil)
                            dateFormatter = [[NSDateFormatter alloc] init];
                        if (keyVal.length == 4)
                            dateFormatter.dateFormat = @"yyyy";
                        else if (keyVal.length == 10)
                            dateFormatter.dateFormat = @"yyyy-MM-dd";
                        else NSLog(@"Illegal date format!");

                        metadata.firstpublishedDate = [dateFormatter dateFromString:keyVal];
                        dateFormatter.dateFormat = @"yyyy";
                        metadata.firstpublished = [dateFormatter stringFromDate:metadata.firstpublishedDate];

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
                                metadata.languageAsWord = language;
                            } else {
                                // Otherwise we use the full string for both language and languageAsWord.
                                metadata.languageAsWord = keyVal;
                                languageCode = keyVal;
                            }
                        }
                        metadata.language = languageCode;
                    } else {
                        [metadata setValue:keyVal forKey:key];
                        if ([key isEqualToString:@"forgiveness"])
                            metadata.forgivenessNumeric = forgiveness[keyVal];
                        if ([key isEqualToString:@"title"]) {
                            NSString *title = [metadata.title stringByDecodingXMLEntities];
                            if (title && title.length)
                                metadata.title = title;
                        }
                    }

                } else if ([node.name compare:@"description"] == 0) {
                    metadata.blurb =
                        [self renderDescriptionElement:(NSXMLElement *)node];
                    // When importing Return to Ditch Day, Babel returns a text with \\n instead of line breaks.
                    metadata.blurb =
                        [metadata.blurb stringByReplacingOccurrencesOfString:@"\\n" withString:@"\n"];

                }
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
  NSEnumerator *enumChildren = [element.children objectEnumerator];
  NSXMLNode *node;
  NSUInteger count = 0;
  while ((node = [enumChildren nextObject])) {
    if (node.kind == NSXMLTextKind) {
      if (count > 0)
        [string appendString:@"\n\n"];
      [string appendString:node.stringValue];
      ++count;
    }
  }
  return string;
}

@end
