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


NSString * const kImagePlaceholderOriginalURL = @"Placeholder";
NSString * const kImagePlaceholderDescription = @"Inform 7 placeholder cover image: The worn spine of an old book, laying open on top of another open book. Caption: \"Interactive fiction by Inform. Photograph: Scot Campbell\".";

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
    if (self.deleted)
        [self removeFromSpotlightIndex];
}

+ (void)deleteIfOrphan:(Image *)image {
    if (image && image.metadata.count == 0 && ![image.originalURL isEqualToString:kImagePlaceholderOriginalURL]) {
        [image.managedObjectContext deleteObject:image];
    }
}

@end

@implementation NSManagedObject (SpotlightRemoval)

- (void)removeFromSpotlightIndex {
    NSString *identifier = self.objectID.URIRepresentation.absoluteString;
    if (!identifier)
        return;
    NSString *entityName = self.entity.name;
    [[CSSearchableIndex defaultSearchableIndex]
     deleteSearchableItemsWithIdentifiers:@[identifier]
     completionHandler:^(NSError *blockerror){
        if (blockerror) {
            NSLog(@"Deleting searchable item for %@ failed: %@", entityName, blockerror);
        }
    }];
}

@end
