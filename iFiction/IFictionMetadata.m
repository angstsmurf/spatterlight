//
//  IFictionMetadata.m
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

#import "DebugLog.h"

#import "IFictionMetadata.h"
#import "IFStory.h"

@implementation IFictionMetadata

+ (NSXMLDocument *)XMLDocumentFromData:(NSData *)data {
    if (!data.length)
        return nil;
    NSError *error = nil;
    NSXMLDocument *xml = [[NSXMLDocument alloc] initWithData:data
                                                     options:NSXMLDocumentTidyXML
                                                       error:&error];
    if (!xml)
        NSLog(@"IFictionMetadata: could not parse XML: %@", error);
    return xml;
}

- (instancetype)initWithData:(NSData *)data {
    self = [super init];
    if (self) {
        NSMutableArray<IFStory *> *stories = [[NSMutableArray alloc] init];
        NSXMLDocument *xml = [IFictionMetadata XMLDocumentFromData:data];
        for (NSXMLElement *child in [xml.rootElement elementsForName:@"story"]) {
            IFStory *story = [[IFStory alloc] initWithXMLElement:child];
            if (story)
                [stories addObject:story];
        }
        if (stories.count == 0)
            return nil;
        _stories = stories;
    }
    return self;
}

- (IFStory *)firstStory {
    if (_stories.count > 1)
        NSLog(@"Found more than one story in iFiction data (%ld). Only using the first one.", _stories.count);
    return _stories.firstObject;
}

@end
