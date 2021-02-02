//
//  IFictionMetadata.m
//  Yazmin
//
//  Created by David Schweinsberg on 22/11/07.
//  Copyright 2007 David Schweinsberg. All rights reserved.
//

#import "YazIFictionMetadata.h"
#import "YazIFIdentification.h"
#import "YazIFStory.h"

@implementation IFictionMetadata

- (instancetype)initWithData:(NSData *)data {
  self = [super init];
  if (self) {
    NSMutableArray<IFStory *> *stories = [[NSMutableArray alloc] init];

    NSError *error;
    NSXMLDocument *xml =
        [[NSXMLDocument alloc] initWithData:data
                                    options:NSXMLDocumentTidyXML
                                      error:&error];
    NSEnumerator *enumerator =
        [[[xml rootElement] elementsForName:@"story"] objectEnumerator];
    NSXMLElement *child;
    while ((child = [enumerator nextObject])) {
      IFStory *story = [[IFStory alloc] initWithXMLElement:child];
      [stories addObject:story];
    }
    _stories = stories;
  }
  return self;
}

- (instancetype)initWithStories:(NSArray<IFStory *> *)stories {
  self = [super init];
  if (self) {
    _stories = stories;
  }
  return self;
}

- (nullable IFStory *)storyWithIFID:(NSString *)ifid {
  for (IFStory *story in _stories) {
    if ([story.identification.ifids containsObject:ifid]) {
      return story;
    }
  }
  return nil;
}

- (NSString *)xmlString {
  NSMutableString *string = [NSMutableString string];
  [string appendString:@"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"];
  [string appendString:@"<ifindex version=\"1.0\" "
                       @"xmlns=\"http://babel.ifarchive.org/protocol/iFiction/"
                       @"\">\n"];
  for (IFStory *story in _stories)
    [string appendString:story.xmlString];
  [string appendString:@"</ifindex>\n"];
  return string;
}

@end
