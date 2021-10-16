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

@dynamic data;
@dynamic imageDescription;
@dynamic interpolation;
@dynamic originalURL;
@dynamic metadata;

- (void)willSave {
    if (@available(macOS 10.11, *)) {
        if (!self.deleted)
            return;
        CSSearchableIndex *index = [CSSearchableIndex defaultSearchableIndex];
        NSString *identifier = self.objectID.URIRepresentation.path;
        if (!identifier)
            return;
        [index deleteSearchableItemsWithIdentifiers:@[identifier]
                                  completionHandler:^(NSError *blockerror){
            if (blockerror) {
                NSLog(@"Deleting searchable item for Image failed: %@", blockerror);
            }
        }];
    }
}

@end
