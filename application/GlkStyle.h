/*
 * Class for encapsulating both style hints and
 * the resulting NSDictionary for NSAttributedString.
 *
 * We also put our own style-number into the dictionary,
 * so that the window can go through strings and update
 * the strings with new attributes when the preferences
 * change.
 */

@interface GlkStyle : NSObject
{
    NSMutableDictionary *dict;
    int stylenumber;
    int windowtype;
    int enabled[stylehint_NUMHINTS];
    int value[stylehint_NUMHINTS];
}

- initWithStyle: (int)stylenumber_
     windowType: (int)windowtype_
	 enable: (int*)enablearray
	  value: (int*)valuearray;
- (void) prefsDidChange;
- (NSDictionary*) attributes;

@end
