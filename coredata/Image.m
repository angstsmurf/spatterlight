//
//  Image.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import "Image.h"
#import "Metadata.h"
#import <CoreSpotlight/CoreSpotlight.h>


@implementation Image

+ (NSFetchRequest<Image *> *)fetchRequest {
	return [NSFetchRequest fetchRequestWithEntityName:@"Image"];
}

@dynamic data;
@dynamic imageDescription;
@dynamic interpolation;
@dynamic originalURL;
@dynamic metadata;

- (void)willSave {
    if (!self.deleted)
        return;
    CSSearchableIndex *index = [CSSearchableIndex defaultSearchableIndex];
    NSString *identifier = self.objectID.URIRepresentation.absoluteString;
    if (!identifier)
        return;
    [index deleteSearchableItemsWithIdentifiers:@[identifier]
                              completionHandler:^(NSError *blockerror){
        if (blockerror) {
            NSLog(@"Deleting searchable item for Image failed: %@", blockerror);
        }
    }];
}

+ (void)deleteIfOrphan:(Image *)image {
    if (image && image.metadata.count == 0 && ![image.originalURL isEqualToString:@"Placeholder"]) {
        [image.managedObjectContext deleteObject:image];
    }
}

@end
