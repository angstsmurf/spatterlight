//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import "IFictionMetadata.h"
#import "IFStory.h"

@implementation IFictionMetadata

- (instancetype)initWithData:(NSData *)data andContext:(NSManagedObjectContext *)context andQueue:(NSOperationQueue *)queue {
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
    NSXMLElement *child;
    while ((child = [enumerator nextObject])) {
        IFStory *story = [[IFStory alloc] initWithXMLElement:child andContext:context andQueue:queue];
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
