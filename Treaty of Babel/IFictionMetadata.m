//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import "IFictionMetadata.h"
#import "IFStory.h"

@implementation IFictionMetadata

- (instancetype)initWithData:(NSData *)data {
  self = [super init];
  if (self) {
    NSMutableArray *stories = [[NSMutableArray alloc] init];

    NSError *error;
    NSXMLDocument *xml =
        [[NSXMLDocument alloc] initWithData:data
                                    options:NSXMLDocumentTidyXML
                                      error:&error];
    NSEnumerator *enumerator =
        [[[xml rootElement] elementsForName:@"story"] objectEnumerator];
    for (NSXMLElement *child in enumerator) {
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

@end
