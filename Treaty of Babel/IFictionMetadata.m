//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import "IFictionMetadata.h"
#import "IFIdentification.h"
#import "IFStory.h"

@implementation IFictionMetadata

- (instancetype)initWithData:(NSData *)data andContext:(NSManagedObjectContext *)context{
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
        IFStory *story = [[IFStory alloc] initWithXMLElement:child andContext:context];
      [stories addObject:story];
    }
    _stories = stories;
  }
  return self;
}

@end
