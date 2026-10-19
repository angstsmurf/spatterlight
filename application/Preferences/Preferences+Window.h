#import "Preferences.h"

/*
 * The window: preview show/hide and window height, toolbar panel switching,
 * and window state restoration.
 */

@interface Preferences (Window)

- (void)notePreferencesChanged:(NSNotification *)notify;
- (void)resizeWindowToHeight:(CGFloat)height animate:(BOOL)animate;
- (CGFloat)calculateWindowHeightWithPreview;
- (NSToolbarItem *)toolbarItemWithImage:(NSString *)imageName label:(NSString *)label tooltip:(NSString *)tooltip identifier:(NSToolbarItemIdentifier)identifier;
- (void)switchToPanel:(NSToolbarItem *)item;
- (void)switchToPanel:(NSToolbarItem *)item resizePreview:(BOOL)resizePreview;

@end
