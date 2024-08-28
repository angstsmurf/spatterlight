//
//  GlkStyle.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-29.
//
//

#include "glk.h"

#import "GlkStyle.h"
#import "Theme.h"
#import "Constants.h"
#import "NSColor+integer.h"


@implementation GlkStyle

@dynamic attributeDict;
@dynamic autogenerated;
@dynamic bufAlert;
@dynamic bufBlock;
@dynamic bufEmph;
@dynamic bufferNormal;
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
@dynamic gridNormal;
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
    NSMutableDictionary *dict = [self.attributeDict mutableCopy];

    NSParagraphStyle *paragraphStyle = [dict[NSParagraphStyleAttributeName] copy];
    dict[NSParagraphStyleAttributeName] = paragraphStyle;

    cloned.attributeDict = dict;
    cloned.autogenerated = self.autogenerated;
	return cloned;
}

- (NSSize)cellSize {

    if (!self.attributeDict)
        [self createDefaultAttributeDictionary];

    NSSize size = [@"W" sizeWithAttributes:self.attributeDict];

    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];

    // By measuring line height in this way we include
    // line spacing and baseline offset

    NSTextStorage *textstorage = [[NSTextStorage alloc] initWithString:@"ÅWQyjgq\nÅWQyjgq" attributes:self.attributeDict];

    [textstorage addLayoutManager:layoutManager];

    NSTextContainer *container = [[NSTextContainer alloc]
                 initWithContainerSize:NSMakeSize(MAXFLOAT, MAXFLOAT)];

    [layoutManager addTextContainer:container];

    [layoutManager ensureLayoutForTextContainer:container];
    NSRect frag = [layoutManager lineFragmentUsedRectForGlyphAtIndex:0 effectiveRange:nil];
    size.height = frag.size.height;
    return size;
}

-(void)createDefaultAttributeDictionary {
    
    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    NSFont *font;
    
    if ([self testGridStyle]) {
        NSLog(@"GlkStyle createDefaultAttributeDictionary: This is a grid style. Setting font to Source Code Pro 11");
        font = [NSFont fontWithName:@"Source Code Pro" size:11];

    } else {
        NSLog(@"GlkStyle createDefaultAttributeDictionary: This is a buffer style. Setting font to Helvetica 13");
        font = [NSFont fontWithName:@"Helvetica" size:13];
    }

    dict[NSFontAttributeName] = font;

    NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];
    para.paragraphStyle = [NSParagraphStyle defaultParagraphStyle];

    dict[NSParagraphStyleAttributeName] = para;
    dict[NSForegroundColorAttributeName] = [NSColor blackColor];

//    dict[NSCursorAttributeName] = [NSCursor arrowCursor];

    dict[@"GlkStyle"] = @(self.index);

    self.attributeDict = dict;
}

- (BOOL)testGridStyle {

    if (self.bufferNormal)
        return NO;

    return (self.gridNormal || self.gridAlert || self.gridBlock || self.gridEmph || self.gridPre || self.gridHead || self.gridInput || self.gridNote || self.gridSubH || self.gridUsr1 || self.gridUsr2);
}


- (NSMutableDictionary *)attributesWithHints:(NSArray *)hints {

    /*
     * Get default attribute dictionary from the preferences
     */

    if (!self.attributeDict) {
        NSLog(@"attributesWithHints: No attributeDict, so generating a new one");
        [self createDefaultAttributeDictionary];
    }

    NSMutableDictionary *attributes = [self.attributeDict mutableCopy];

    NSFont *font = attributes[NSFontAttributeName];
    NSFontManager *fontmgr = [NSFontManager sharedFontManager];

    NSMutableParagraphStyle *para = [attributes[NSParagraphStyleAttributeName] mutableCopy];

    NSInteger value;
    NSUInteger i;
    NSFontTraitMask mask;
    BOOL isBufferStyle = ![self testGridStyle];

    for (i = 0 ; i < stylehint_NUMHINTS ; i++ ){
        if ([hints[i] isNotEqualTo:[NSNull null]]) {

            value = ((NSNumber *)hints[i]).integerValue;

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
                            para.alignment = NSTextAlignmentLeft;
                            break;
                        case stylehint_just_LeftRight:
                            para.alignment = NSTextAlignmentJustified;
                            break;
                        case stylehint_just_Centered:
                            para.alignment = NSTextAlignmentCenter;
                            break;
                        case stylehint_just_RightFlush:
                            para.alignment = NSTextAlignmentRight;
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
                    if (isBufferStyle) {
                        CGFloat size = font.matrix[0] + value * 2;
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
                    else {
                        font = [fontmgr convertFont:font
                                        toHaveTrait:NSFixedPitchFontMask];

                        // This usually does not work, so we hack it to use the preformatted style font
                        if (isBufferStyle && ([fontmgr traitsOfFont:font] & NSFixedPitchFontMask) != NSFixedPitchFontMask) {
                            Theme *theme = [self findTheme];
                            if (theme) {
                                NSFont *fixedWidth = theme.bufPre.font;
                                mask = [fontmgr traitsOfFont:font];
                                if (mask & NSItalicFontMask)
                                    fixedWidth = [fontmgr convertFont:fixedWidth toHaveTrait:NSItalicFontMask];
                                if (mask & NSBoldFontMask)
                                    fixedWidth = [fontmgr convertFont:fixedWidth toHaveTrait:NSBoldFontMask];
                                font = fixedWidth;
                            }
                        }
                    }
                    break;

                    /*
                     * Change colors
                     */

                case stylehint_TextColor:
                    attributes[NSForegroundColorAttributeName] = [NSColor colorFromInteger:value];
                    break;

                case stylehint_BackColor:
                    attributes[NSBackgroundColorAttributeName] = [NSColor colorFromInteger:value];
                    break;


                case stylehint_ReverseColor:
                    if (value == 1 && [hints[stylehint_TextColor] isEqual:[NSNull null]] &&
                        [hints[stylehint_BackColor] isEqual:[NSNull null]]) {
                        NSColor *bgcolor = attributes[NSBackgroundColorAttributeName];
                        NSColor *fgcolor = attributes[NSForegroundColorAttributeName];

                        if (!bgcolor) {
                            Theme *theme = [self findTheme];
                            if (!theme)
                                bgcolor = [NSColor whiteColor];
                            else {
                                if (isBufferStyle)
                                    bgcolor = theme.bufferBackground;
                                else
                                    bgcolor = theme.gridBackground;
                            }
                        }
                        attributes[NSForegroundColorAttributeName] = bgcolor;
                        attributes[NSBackgroundColorAttributeName] = fgcolor;
                    }
                    break;
                    
                default:
                    
                    NSLog(@"Unhandled style hint: %ld", i);
                    break;
            }
        }
    }

    attributes[NSParagraphStyleAttributeName] = para;
    attributes[NSFontAttributeName] = font;
    
    return attributes;
}

- (Theme *)findTheme {
    Theme *theme = nil;
    NSInteger count = 0;

    if (self.gridNormal) {
        theme = self.gridNormal;
        count++;
    }

    if (self.bufferNormal) {
        theme = self.bufferNormal;
        count++;
    }

    if (self.gridAlert) {
        theme = self.gridAlert;
        count++;
    }

    if (self.gridBlock) {
        theme = self.gridBlock;
        count++;
    }

    if (self.gridEmph) {
        theme = self.gridEmph;
        count++;
    }

    if (self.gridPre) {
        theme = self.gridPre;
        count++;
    }

    if (self.gridHead) {
        theme = self.gridHead;
        count++;
    }

    if (self.gridInput) {
        theme = self.gridInput;
        count++;
    }

    if (self.gridNote) {
        theme = self.gridNote;
        count++;
    }

    if (self.gridSubH) {
        theme = self.gridSubH;
        count++;
    }

    if (self.gridUsr1) {
        theme = self.gridUsr1;
        count++;
    }

    if (self.gridUsr2) {
        theme = self.gridUsr2;
        count++;
    }

    if (self.bufAlert) {
        theme = self.bufAlert;
        count++;
    }

    if (self.bufBlock) {
        theme = self.bufBlock;
        count++;
    }

    if (self.bufEmph) {
        theme = self.bufEmph;
        count++;
    }

    if (self.bufPre) {
        theme = self.bufPre;
        count++;
    }

    if (self.bufHead) {
        theme = self.bufHead;
        count++;
    }

    if (self.bufInput) {
        theme = self.bufInput;
        count++;
    }

    if (self.bufNote) {
        theme = self.bufNote;
        count++;
    }

    if (self.bufSubH) {
        theme = self.bufSubH;
        count++;
    }

    if (self.bufUsr1) {
        theme = self.bufUsr1;
        count++;
    }

    if (self.bufUsr2) {
        theme = self.bufUsr2;
        count++;
    }

    if (count > 1)
        NSLog(@"Error! More than one theme!");
    
    if (!theme) {
        NSLog(@"Error! No theme found!");
        NSUInteger styleValue = (NSUInteger)((NSNumber *)(self.attributeDict)[@"GlkStyle"]).integerValue;

        if (styleValue >= style_NUMSTYLES) {
            NSLog(@"Error! Broken attributeDict!");
            [self createDefaultAttributeDictionary];
            styleValue = 0;
        }
        
        NSString *styleName = [self testGridStyle] ? gGridStyleNames[styleValue] : gBufferStyleNames[styleValue];
        NSLog(@"GlkStyle attribute: %ld (%@), count: %ld", styleValue, styleName, count);
    }
    
    return theme;
}

- (NSFont *)font {
    return (self.attributeDict)[NSFontAttributeName];
}

- (void)setFont:(NSFont *)font {
    if (!font) {
        NSLog(@"GlkStyle setFont called with bad Font!");
        return;
    }
    NSMutableDictionary *mutableDict = [self.attributeDict mutableCopy];
    mutableDict[NSFontAttributeName] = font;
    self.attributeDict = mutableDict;
}


- (NSColor *)color {
    return (self.attributeDict)[NSForegroundColorAttributeName];
}

- (void)setColor:(NSColor *)color {
    NSMutableDictionary *mutableDict = [self.attributeDict mutableCopy];
    mutableDict[NSForegroundColorAttributeName] = color;
    self.attributeDict = mutableDict;
}


- (NSInteger)index {
    for (NSUInteger i = 0 ; i < style_NUMSTYLES ; i++){
        if ([self valueForKey:gBufferStyleNames[i]] || [self valueForKey:gGridStyleNames[i]])
            return (NSInteger)i;
    }
    NSLog(@"GlkStyle styleIndex: This style does not belong to a theme, so style index can not be calculated!");
    return -1;
}

- (void)setIndex:(NSInteger)i {
    NSMutableDictionary *dict = [self.attributeDict mutableCopy];
    dict[@"GlkStyle"] = @(i);
    self.attributeDict = dict;
}

- (CGFloat)lineSpacing {
    return [(self.attributeDict)[NSParagraphStyleAttributeName] lineSpacing];
}

- (void)setLineSpacing:(CGFloat)lineSpacing {
    NSMutableDictionary *dict = [self.attributeDict mutableCopy];
    NSMutableParagraphStyle *paraStyle = [dict[NSParagraphStyleAttributeName] mutableCopy];
    if (!paraStyle) {
        paraStyle = [[NSMutableParagraphStyle alloc] init];
        paraStyle.paragraphStyle = [NSParagraphStyle defaultParagraphStyle];
    }
    paraStyle.lineSpacing = lineSpacing;
    dict[NSParagraphStyleAttributeName] = paraStyle;
    self.attributeDict = dict;
}

- (void)printDebugInfo {
    NSUInteger stylevalue = (NSUInteger)((NSNumber *)(self.attributeDict)[@"GlkStyle"]).integerValue;
    NSString *styleName = [self testGridStyle] ? gGridStyleNames[stylevalue] : gBufferStyleNames[stylevalue];
    NSColor *debugColor = self.color;
    NSString *fontColorName = [debugColor isEqual:[NSColor blackColor]]?@"black":@"not black";
    if ([debugColor isEqual:[NSColor whiteColor]]) {
        fontColorName = @"white";
    }
    debugColor = (self.attributeDict)[NSBackgroundColorAttributeName];
    NSString *backgroundColorName = [debugColor isEqual:[NSColor blackColor]]?@"black":@"not black";
    if ([debugColor isEqual:[NSColor whiteColor]]) {
        fontColorName = @"white";
    }

    NSFont *debugFont = self.font;
    NSLog(@"GlkStyle printDebugInfo: theme: %@ style: %ld (%@). Font: %@ Font size: %f Foreground color:%@ Background color: %@", [self findTheme].name, stylevalue, styleName, debugFont.fontName, debugFont.pointSize, fontColorName, backgroundColorName);
}

@end
