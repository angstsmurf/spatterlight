//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//


#import "IFIdentification.h"
#import "Metadata.h"
#import "Ifid.h"


@implementation IFIdentification

- (instancetype)initWithXMLElement:(NSXMLElement *)element andContext:(NSManagedObjectContext *)context {
  self = [super init];
  if (self) {
    NSMutableArray *ifids = [[NSMutableArray alloc] init];
    NSMutableArray *ifidObjs = [[NSMutableArray alloc] init];

      Ifid *ifidObj;
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
      if (ifids.count == 0) {
          NSLog(@"IFIdentification: no Ifids in document! Bailing!");
          return nil;
      }
    _ifids = ifids;
      for (NSString *ifid in ifids) {
          ifidObj = [self fetchIfid:ifid inContext:context];
          if (!ifidObj) {
              ifidObj = (Ifid *) [NSEntityDescription
                                        insertNewObjectForEntityForName:@"Ifid"
                                        inManagedObjectContext:context];
              ifidObj.ifidString = ifid;
          }
          [ifidObjs addObject:ifidObj];
      }
      for (Ifid *ifid in ifidObjs) {
          _metadata = ifid.metadata;
          if (_metadata)
              break;
      }

      if (!_metadata) {
          _metadata = (Metadata *) [NSEntityDescription
                                    insertNewObjectForEntityForName:@"Metadata"
                                    inManagedObjectContext:context];
      }
      if (!_metadata.format)
          _metadata.format = _format;
      _metadata.bafn = _bafn;
      [_metadata addIfid:[NSSet setWithArray:ifidObjs]];
  }
  return self;
}

- (Ifid *)fetchIfid:(NSString *)ifid inContext:(NSManagedObjectContext *)context {
    NSError *error = nil;
    NSArray *fetchedObjects;
    NSPredicate *predicate;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:@"Ifid" inManagedObjectContext:context];

    fetchRequest.entity = entity;

    predicate = [NSPredicate predicateWithFormat:@"ifidString like[c] %@",ifid];
    fetchRequest.predicate = predicate;

    fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"fetchMetadataForIFID: Problem! %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"Found more than one Ifid with ifidString %@",ifid);
    }
    else if (fetchedObjects.count == 0)
    {
        return nil;
    }

    return [fetchedObjects objectAtIndex:0];
}

@end
