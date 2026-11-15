#import "Preferences.h"

/*
 * Zooming the current theme in and out (the View menu zoom commands).
 */

@interface Preferences (Zoom)

+ (void)zoomIn;
+ (void)zoomOut;
+ (void)zoomToActualSize;
+ (void)scale:(CGFloat)scalefactor;
+ (void)scaleStylesOfTheme:(Theme *)themeToScale by:(CGFloat)scalefactor;
- (void)updatePanelAfterZoom;

@end
