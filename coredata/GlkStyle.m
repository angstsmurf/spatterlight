//
//  GlkStyle.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-25.
//
//

#include "glk.h"

#import "GlkStyle.h"
#import "Theme.h"
#import "Compatibility.h"

@implementation GlkStyle

@dynamic attributeDict;
@dynamic baseline;
@dynamic bgcolor;
@dynamic color;
@dynamic columnWidth;
@dynamic font;
@dynamic kerning;
@dynamic leading;
@dynamic ligatures;
@dynamic lineHeight;
@dynamic name;
@dynamic paragraphStyle;
@dynamic pitch;
@dynamic shadow;
@dynamic size;
@dynamic strikethrough;
@dynamic strikethroughColor;
@dynamic underline;
@dynamic underlineColor;
@dynamic variant;
@dynamic bufAlert;
@dynamic bufBlock;
@dynamic bufEmph;
@dynamic bufferFont;
@dynamic bufHead;
@dynamic bufInput;
@dynamic bufNote;
@dynamic bufPre;
@dynamic bufSubH;
@dynamic bufUsr1;
@dynamic bufUsr2;
@dynamic gridAlert;
@dynamic gridBlock;
@dynamic gridEmph;
@dynamic gridFont;
@dynamic gridHead;
@dynamic gridInput;
@dynamic gridNote;
@dynamic gridPre;
@dynamic gridSubH;
@dynamic gridUsr1;
@dynamic gridUsr2;

- (GlkStyle *)clone {
	// Create new object in data store
	GlkStyle *cloned = [NSEntityDescription
                     insertNewObjectForEntityForName:@"GlkStyle"
                     inManagedObjectContext:self.managedObjectContext];

	// Loop through all attributes and assign them to the clone
	NSDictionary *attributes = [NSEntityDescription
                                entityForName:@"GlkStyle"
                                inManagedObjectContext:self.managedObjectContext].attributesByName;

	for (NSString *attr in attributes) {
        if ([attr isEqualToString:@"font"]) {
            cloned.font = [NSFont fontWithName:self.name size:self.size.doubleValue];
        } else {
            [cloned setValue:[[self valueForKey:attr] copy] forKey:attr];
        }
	}
	return cloned;
}

- (void)recalculateCell {

    if (!self.font) {
        self.font = [NSFont fontWithName:self.name size:self.size.doubleValue];
    }

    // This is the only way I have found to get the correct width at all sizes
    if (NSAppKitVersionNumber < NSAppKitVersionNumber10_8) {
        CGFloat width = [@"X" sizeWithAttributes:@{NSFontAttributeName :self.font}].width;
        self.columnWidth = [NSNumber numberWithDouble:width];
    } else
        self.columnWidth = @([self.font advancementForGlyph:(NSGlyph)'X'].width);

    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    self.lineHeight = [NSNumber numberWithDouble:[layoutManager defaultLineHeightForFont:self.font] + self.leading.doubleValue];
}

- (NSMutableDictionary *)attributesWithHints:(NSArray *)hints andGridWindow:(BOOL)gridWindow {

    /*
     * Get default attribute dictionary from the preferences
     */

    NSMutableDictionary *attributes = [self.attributeDict mutableCopy];

    NSFont *font = [attributes objectForKey:NSFontAttributeName];
    NSFontManager *fontmgr = [NSFontManager sharedFontManager];

    NSMutableParagraphStyle *para = [attributes
                                     objectForKey:NSParagraphStyleAttributeName];

    NSInteger value, i, r, b, g;
    NSColor *color;

    for (i = 0 ; i < stylehint_NUMHINTS ; i++ ){
        if ([[hints objectAtIndex:i] isNotEqualTo:[NSNull null]]) {

            value = ((NSNumber *)[hints objectAtIndex:i]).integerValue;

            switch (i) {

                    /*
                     * Change indentation and justification.
                     */

                case stylehint_Indentation:
                    para.headIndent = value * 3;
                    break;

                case stylehint_ParaIndentation:
                    para.firstLineHeadIndent = para.headIndent + value * 3;
                    break;

                case stylehint_Justification:
                    switch (value) {
                        case stylehint_just_LeftFlush:
                            para.alignment = NSLeftTextAlignment;
                            break;
                        case stylehint_just_LeftRight:
                            para.alignment = NSJustifiedTextAlignment;
                            break;
                        case stylehint_just_Centered:
                            para.alignment = NSCenterTextAlignment;
                            break;
                        case stylehint_just_RightFlush:
                            para.alignment = NSRightTextAlignment;
                            break;
                        default:
                            NSLog(@"Unimplemented justification style hint!");
                            break;
                    }
                    break;

                    /*
                     * Change font size and style.
                     */

                case stylehint_Size:
                    if (!gridWindow) {

                        float size = font.matrix[0] + value * 2;
                        font = [fontmgr convertFont:font toSize:size];
                    }
                    break;

                case stylehint_Weight:
                    font = [fontmgr convertFont:font toNotHaveTrait:NSBoldFontMask];
                    if (value == -1)
                        font = [fontmgr convertWeight:NO ofFont:font];
                    else if (value == 1)
                        font = [fontmgr convertWeight:YES ofFont:font];
                    break;

                case stylehint_Oblique:
                    /* buggy buggy buggy games, they don't read the glk spec */
                    if (value == 1)
                        font = [fontmgr convertFont:font toHaveTrait:NSItalicFontMask];
                    else
                        font = [fontmgr convertFont:font
                                     toNotHaveTrait:NSItalicFontMask];
                    break;

                case stylehint_Proportional:
                    if (value == 1)
                        font = [fontmgr convertFont:font
                                     toNotHaveTrait:NSFixedPitchFontMask];
                    else
                        font = [fontmgr convertFont:font
                                        toHaveTrait:NSFixedPitchFontMask];
                    break;

                    /*
                     * Change colors
                     */

                case stylehint_TextColor:

                    r = (value >> 16) & 0xff;
                    g = (value >> 8) & 0xff;
                    b = (value >> 0) & 0xff;
                    color = [NSColor colorWithCalibratedRed:r / 255.0
                                                      green:g / 255.0
                                                       blue:b / 255.0
                                                      alpha:1.0];

                    [attributes setObject:color forKey:NSForegroundColorAttributeName];
                    break;

                case stylehint_BackColor:

                    r = (value >> 16) & 0xff;
                    g = (value >> 8) & 0xff;
                    b = (value >> 0) & 0xff;
                    color = [NSColor colorWithCalibratedRed:r / 255.0
                                                      green:g / 255.0
                                                       blue:b / 255.0
                                                      alpha:1.0];

                    [attributes setObject:color forKey:NSBackgroundColorAttributeName];
                    break;


                case stylehint_ReverseColor:

                    if ([[hints objectAtIndex:stylehint_TextColor] isEqual:[NSNull null]] &&
                        [[hints objectAtIndex:stylehint_BackColor] isEqual:[NSNull null]]) {
                        [attributes setObject:self.bgcolor
                                       forKey:NSForegroundColorAttributeName];
                        [attributes setObject:self.color
                                       forKey:NSForegroundColorAttributeName];
                    }
                    break;
                    
                default:
                    
                    NSLog(@"Unhandled style hint");
                    break;
            }
        }
        
    }
    
    [attributes setObject: font forKey:NSFontAttributeName];
    
    return attributes;
}

@end
