//
//  ThemeArrayController.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-01-31.
//
//

#import "ThemeArrayController.h"
#import "Theme.h"

@implementation ThemeArrayController

- (id)newObject {
    Theme *newObj = [super newObject];
    [newObj copyAttributesFrom:self.selectedTheme];
    NSString *name = @"New theme";
    NSUInteger counter = 1;
    while ([self findThemeByName:name]) {
        name = [NSString stringWithFormat:@"New theme %ld", counter++];
    }
    newObj.name = name;
    newObj.defaultParent = self.selectedTheme;
    return newObj;
}

- (Theme *)findThemeByName:(NSString *)name {

    if (!name)
        return nil;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSArray *fetchedObjects;
    NSError *error;
    fetchRequest.entity = [NSEntityDescription entityForName:@"Theme" inManagedObjectContext:self.managedObjectContext];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"name like[c] %@", name];
    fetchRequest.includesPropertyValues = NO;
    fetchedObjects = [self.managedObjectContext executeFetchRequest:fetchRequest error:&error];

    if (fetchedObjects == nil || fetchedObjects.count == 0) {
        return nil;
    }

    return [fetchedObjects objectAtIndex:0];
}

- (Theme *)selectedTheme {
    NSArray *selectedThemes = [self selectedObjects];
    if (selectedThemes && selectedThemes.count)
        return [selectedThemes objectAtIndex:0];
    return nil;
}


@end
