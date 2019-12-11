//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import "IFIdentification.h"
#import "Metadata.h"

@implementation IFIdentification

- (instancetype)initWithXMLElement:(NSXMLElement *)element andContext:(NSManagedObjectContext *)context {
  self = [super init];
  if (self) {
    NSMutableArray *ifids = [[NSMutableArray alloc] init];
    _format = @"";

    NSEnumerator *enumChildren = [element.children objectEnumerator];
    NSXMLNode *node;
    while ((node = [enumChildren nextObject])) {
      if ([node.name compare:@"ifid"] == 0)
        [ifids addObject:node.stringValue];
      else if ([node.name compare:@"format"] == 0) {
        _format = node.stringValue;
      } else if ([node.name compare:@"bafn"] == 0)
        _bafn = node.stringValue;
    }
    _ifids = ifids;
      for (NSString *ifid in ifids) {
          _metadata = [self fetchMetadataForIFID:ifid inContext:context];
          if (_metadata) {
              break;
          }
      }

      if (!_metadata) {
          _metadata = (Metadata *) [NSEntityDescription
                                    insertNewObjectForEntityForName:@"Metadata"
                                    inManagedObjectContext:context];
          _metadata.ifid = [ifids objectAtIndex:0];
      }
      if (!_metadata.format)
          _metadata.format = _format;
      _metadata.bafn = _bafn;
  }
  return self;
}

- (Metadata *)fetchMetadataForIFID:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;
    NSPredicate *predicate;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:@"Metadata" inManagedObjectContext:context];

    fetchRequest.entity = entity;

    predicate = [NSPredicate predicateWithFormat:@"ifid like[c] %@",ifid];
    fetchRequest.predicate = predicate;

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"fetchMetadataForIFID: Problem! %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"Found more than one entry with metadata %@",ifid);
    }
    else if (fetchedObjects.count == 0)
    {
        return nil;
    }

    return [fetchedObjects objectAtIndex:0];
}

@end
