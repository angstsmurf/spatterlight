//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import "IFBibliographic.h"
#import "Metadata.h"
#import "NSString+Categories.h"

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

- (instancetype)initWithXMLElement:(NSXMLElement *)element andMetadata:(Metadata *)metadata;{
    self = [super init];
    if (self) {
        // This dictionary is repeated in LibController.m, which will be cumbersome
        // when/if we add more languages
        NSDictionary *language = @{
                                   @"en" : @"English",
                                   @"fr" : @"French",
                                   @"es" : @"Spanish",
                                   @"ru" : @"Russian",
                                   @"se" : @"Swedish",
                                   @"de" : @"German",
                                   @"zh" : @"Chinese"
                                   };

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
        NSString *keyVal, *sub;
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
                        sub = [keyVal substringWithRange:NSMakeRange(0, 2)];
                        sub = sub.lowercaseString;
                        if (language[sub])
                            metadata.languageAsWord = language[sub];
                        else
                            metadata.languageAsWord = keyVal;
                        metadata.language = sub;
                    } else {
                        [metadata setValue:keyVal forKey:key];
                        if ([key isEqualToString:@"forgiveness"])
                            metadata.forgivenessNumeric = forgiveness[keyVal];
                        if ([key isEqualToString:@"title"])
                            metadata.title = [metadata.title stringByDecodingXMLEntities];

                    }

                } else if ([node.name compare:@"description"] == 0) {
                    metadata.blurb =
                    [self renderDescriptionElement:(NSXMLElement *)node];
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
