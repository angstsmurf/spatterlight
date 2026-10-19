#import "Preferences.h"

/*
 * The "Keep games organised" controls on the Global pane.
 */

@interface Preferences (LibraryOrganiser)

- (void)updateLibraryOrganiserControls;
- (void)presentLibraryChooserFromURL:(NSURL *)startURL;
- (void)goToDefaultLibraryLocation:(NSButton *)sender;

@end
