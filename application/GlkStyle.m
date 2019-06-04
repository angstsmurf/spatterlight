#import "main.h"

@implementation GlkStyle

- (instancetype) init
{
    NSInteger enablearray[stylehint_NUMHINTS], valuearray[stylehint_NUMHINTS];

    for (NSInteger i = 0; i < stylehint_NUMHINTS; i++)
    {
        enablearray[i]=0;
        valuearray[i]=0;
    }

    return [self initWithStyle:0 windowType:wintype_Blank enable:enablearray value:valuearray];
}

- (instancetype) initWithStyle: (NSInteger)stylenumber_
     windowType: (NSInteger)windowtype_
         enable: (NSInteger*)enablearray
          value: (NSInteger*)valuearray
{
    NSInteger i;
    
    self = [super init];
    
    stylenumber = stylenumber_;
    windowtype = windowtype_;
    
    for (i = 0; i < stylehint_NUMHINTS; i++)
    {
        enabled[i] = enablearray[i];
        value[i] = valuearray[i];
    }
    
    [self prefsDidChange];
    
    return self;
}

- (instancetype) initWithCoder:(NSCoder *)decoder
{
	dict = [decoder decodeObjectForKey:@"dict"];
	stylenumber = [decoder decodeIntegerForKey:@"stylenumber"];
    windowtype = [decoder decodeIntegerForKey:@"windowtype"];
    NSMutableArray *enablearray = [decoder decodeObjectForKey:@"enabled"];
    NSMutableArray *valuearray = [decoder decodeObjectForKey:@"value"];

    for (NSInteger i = 0; i < stylehint_NUMHINTS; i++)
    {
        enabled[i] = ((NSNumber *)[enablearray objectAtIndex:i]).integerValue;
        value[i] = ((NSNumber *)[valuearray objectAtIndex:i]).integerValue;
    }

	return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder
{
    NSNumber *num;
    
	[encoder encodeObject:dict forKey:@"dict"];
    [encoder encodeInteger:stylenumber forKey:@"stylenumber"];
    [encoder encodeInteger:windowtype forKey:@"windowtype"];
    NSMutableArray *enablearray = [NSMutableArray arrayWithCapacity:stylehint_NUMHINTS];
    NSMutableArray *valuearray = [NSMutableArray arrayWithCapacity:stylehint_NUMHINTS];
    for (NSInteger i = 0; i < stylehint_NUMHINTS; i++)
    {
        num = [NSNumber numberWithInteger:enabled[i]];
        [enablearray addObject:num];
        num = [NSNumber numberWithInteger:value[i]];
        [valuearray addObject:num];
    }
    [encoder encodeObject:enablearray forKey:@"enabled"];
    [encoder encodeObject:valuearray forKey:@"value"];
}

- (NSDictionary*) attributes
{
    return dict;
}

- (void) prefsDidChange
{
    if (dict)
    {
        dict = nil;
    }
    
    /*
     * Get default attribute dictionary from the preferences
     */
    
    if (windowtype == wintype_TextGrid)
    {
        dict = [[Preferences attributesForGridStyle: (int)stylenumber] mutableCopy];
    }
    else
    {
        dict = [[Preferences attributesForBufferStyle: (int)stylenumber] mutableCopy];
    }
    
    if (![Preferences stylesEnabled])
        return;
    
    /*
     * Change indentation and justification.
     */
    
    // if (windowtype == wintype_TextBuffer)
    {
        NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];
        [para setParagraphStyle:[dict objectForKey:NSParagraphStyleAttributeName]];
        
        NSInteger indent = para.headIndent;
        NSInteger paraindent = para.firstLineHeadIndent - indent;
        
        if (enabled[stylehint_Indentation])
            indent = value[stylehint_Indentation] * 3;
        if (enabled[stylehint_ParaIndentation])
            paraindent = value[stylehint_ParaIndentation] * 3;
        
        para.headIndent = indent;
        para.firstLineHeadIndent = (indent + paraindent);
        
        if (enabled[stylehint_Justification])
        {
            switch (value[stylehint_Justification])
            {
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
            }
        }

        [dict setObject:para forKey:NSParagraphStyleAttributeName];
    }
    
    /*
     * Change font size and style.
     */
    
    {
        NSFontManager *fontmgr = [NSFontManager sharedFontManager];
        NSFont *font = [dict objectForKey:NSFontAttributeName];
        
        if (enabled[stylehint_Size] && windowtype == wintype_TextBuffer)
        {
            float size = font.matrix[0] + value[stylehint_Size] * 2;
            font = [fontmgr convertFont: font toSize: size];
        }
        
        if (enabled[stylehint_Weight])
        {
            font = [fontmgr convertFont: font toNotHaveTrait: NSBoldFontMask];
            if (value[stylehint_Weight] == -1)
                font = [fontmgr convertWeight: NO ofFont: font];
            if (value[stylehint_Weight] == 1)
                font = [fontmgr convertWeight: YES ofFont: font];
        }
        
        if (enabled[stylehint_Oblique])
        {
            /* buggy buggy buggy games, they don't read the glk spec */
            if (value[stylehint_Oblique] == 1)
                font = [fontmgr convertFont: font toHaveTrait: NSItalicFontMask];
            else
                font = [fontmgr convertFont: font toNotHaveTrait: NSItalicFontMask];
        }
        
        if (enabled[stylehint_Proportional])
        {
            if (value[stylehint_Proportional])
                font = [fontmgr convertFont: font toNotHaveTrait: NSFixedPitchFontMask];
            else
                font = [fontmgr convertFont: font toHaveTrait: NSFixedPitchFontMask];
        }
        
        [dict setObject:font forKey:NSFontAttributeName];
    }
    
    /*
     * Change colors
     */
    
    if (enabled[stylehint_TextColor])
    {
        NSInteger val = value[stylehint_TextColor];
        NSInteger r = (val >> 16) & 0xff;
        NSInteger g = (val >> 8) & 0xff;
        NSInteger b = (val >> 0) & 0xff;
        NSColor *color = [NSColor colorWithCalibratedRed: r / 255.0
                                                   green: g / 255.0
                                                    blue: b / 255.0
                                                   alpha: 1.0];
        [dict setObject:color forKey:NSForegroundColorAttributeName];
    }
    
    if (enabled[stylehint_BackColor])
    {
        NSInteger val = value[stylehint_BackColor];
        NSInteger r = (val >> 16) & 0xff;
        NSInteger g = (val >> 8) & 0xff;
        NSInteger b = (val >> 0) & 0xff;
        NSColor *color = [NSColor colorWithCalibratedRed: r / 255.0
                                                   green: g / 255.0
                                                    blue: b / 255.0
                                                   alpha: 1.0];
        [dict setObject:color forKey:NSBackgroundColorAttributeName];

    }

    if (enabled[stylehint_ReverseColor] && !(enabled[stylehint_TextColor] || enabled[stylehint_BackColor]))
    {
        if (windowtype == wintype_TextGrid)
        {
            [dict setObject:[Preferences gridBackground] forKey:NSForegroundColorAttributeName];
            [dict setObject:[Preferences gridForeground] forKey:NSBackgroundColorAttributeName];
        }
        else
        {
            [dict setObject:[Preferences bufferBackground] forKey:NSForegroundColorAttributeName];
            [dict setObject:[Preferences bufferForeground] forKey:NSBackgroundColorAttributeName];
        }
    }
}

- (BOOL) valueForHint: (NSInteger) hint value:(NSInteger *)val;
{
    if (enabled[hint])
    {
        *val = value[hint];
        return YES;
    }
    else return NO;
}

@end
