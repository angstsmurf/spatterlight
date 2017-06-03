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
        [para setParagraphStyle: dict[NSParagraphStyleAttributeName]];
        
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
        
        dict[NSParagraphStyleAttributeName] = para;
    }
    
    /*
     * Change font size and style.
     */
    
    {
        NSFontManager *fontmgr = [NSFontManager sharedFontManager];
        NSFont *font = dict[NSFontAttributeName];
        
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
        
        dict[NSFontAttributeName] = font;
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
        dict[NSForegroundColorAttributeName] = color;
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
        dict[NSBackgroundColorAttributeName] = color;
    }
    
    if (enabled[stylehint_ReverseColor] && !(enabled[stylehint_TextColor] || enabled[stylehint_BackColor]))
    {
        if (windowtype == wintype_TextGrid)
        {
            dict[NSForegroundColorAttributeName] = [Preferences gridBackground];
            dict[NSBackgroundColorAttributeName] = [Preferences gridForeground];
        }
        else
        {
            dict[NSForegroundColorAttributeName] = [Preferences bufferBackground];
            dict[NSBackgroundColorAttributeName] = [Preferences bufferForeground];
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
