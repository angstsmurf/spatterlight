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
    NSInteger stylenumber;
    NSInteger windowtype;
    NSInteger enabled[stylehint_NUMHINTS];
    NSInteger value[stylehint_NUMHINTS];
}

- initWithStyle: (NSInteger)stylenumber_
     windowType: (NSInteger)windowtype_
         enable: (NSInteger *)enablearray
          value: (NSInteger *)valuearray;
- (void) prefsDidChange;
- (BOOL) valueForHint: (NSInteger) hint value:(NSInteger *)val;

@property (readonly, copy) NSDictionary *attributes;

@end
