//
//  ThemeArrayController.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-01-31.
//
//

#import "ThemeArrayController.h"
#import "Theme.h"
#import "Fetches.h"

@implementation ThemeArrayController

- (id)newObject {
    Theme *newObj = [super newObject];
    [newObj copyAttributesFrom:self.selectedTheme];
    newObj.name = [Theme unusedNameBasedOn:@"New theme" inContext:self.managedObjectContext];
    newObj.defaultParent = self.selectedTheme;
    return newObj;
}

- (Theme *)findThemeByName:(NSString *)name {
    return [Fetches findTheme:name inContext:self.managedObjectContext];
}

- (Theme *)selectedTheme {
    NSArray *selectedThemes = self.selectedObjects;
    if (selectedThemes && selectedThemes.count)
        return selectedThemes[0];
    return nil;
}


@end
