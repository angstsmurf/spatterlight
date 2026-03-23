#import "GlkController.h"

@interface GlkController (MenuItems)

- (IBAction)showGameInfo:(id)sender;
- (IBAction)revealGameInFinder:(id)sender;
- (IBAction)deleteGame:(id)sender;
- (IBAction)like:(id)sender;
- (IBAction)dislike:(id)sender;
- (IBAction)openIfdb:(id)sender;
- (IBAction)download:(id)sender;
- (IBAction)applyTheme:(id)sender;
- (void)journeyPartyAction:(id)sender;
- (void)journeyMemberVerbAction:(id)sender;
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem;

@end
