#import "main.h"

@implementation GlkStyle

- initWithStyle: (int)stylenumber_
     windowType: (int)windowtype_
	 enable: (int*)enablearray
	  value: (int*)valuearray
{
    int i;
    
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

- (void) dealloc
{
    if (dict)
	[dict release];
    [super dealloc];
}

- (NSDictionary*) attributes
{
    return dict;
}

- (void) prefsDidChange
{
    if (dict)
    {
	[dict release];
	dict = nil;
    }

    /*
     * Get default attribute dictionary from the preferences
     */
    
    if (windowtype == wintype_TextGrid)
    {
	dict = [[Preferences attributesForGridStyle: stylenumber] mutableCopy];
    }
    else
    {
	dict = [[Preferences attributesForBufferStyle: stylenumber] mutableCopy];
    }
    
    if (![Preferences stylesEnabled])
	return;
    
    /*
     * Change indentation and justification.
     */
    
    // if (windowtype == wintype_TextBuffer)
    {
	NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];
	[para setParagraphStyle: [dict objectForKey: NSParagraphStyleAttributeName]];
	
	int indent = [para headIndent];
	int paraindent = [para firstLineHeadIndent] - indent;
	
	if (enabled[stylehint_Indentation])
	    indent = value[stylehint_Indentation] * 3;
	if (enabled[stylehint_ParaIndentation])
	    paraindent = value[stylehint_ParaIndentation] * 3;
	
	[para setHeadIndent: indent];
	[para setFirstLineHeadIndent: (indent + paraindent)];
	
	if (enabled[stylehint_Justification])
	{
	    switch (value[stylehint_Justification])
	    {
		case stylehint_just_LeftFlush:
		    [para setAlignment: NSLeftTextAlignment];
		    break;
		case stylehint_just_LeftRight:
		    [para setAlignment: NSJustifiedTextAlignment];
		    break;
		case stylehint_just_Centered:
		    [para setAlignment: NSCenterTextAlignment];
		    break;
		case stylehint_just_RightFlush:
		    [para setAlignment: NSRightTextAlignment];
		    break;
	    }
	}
	
	[dict setObject: para forKey: NSParagraphStyleAttributeName];
	[para release];
    }
    
    /*
     * Change font size and style.
     */
    
    {
	NSFontManager *fontmgr = [NSFontManager sharedFontManager];
	NSFont *font = [dict objectForKey: NSFontAttributeName];
	
	if (enabled[stylehint_Size] && windowtype == wintype_TextBuffer)
	{
	    float size = [font matrix][0] + value[stylehint_Size] * 2;
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
	
	[dict setObject: font forKey: NSFontAttributeName];
    }
    
    /*
     * Change colors
     */
    
    if (enabled[stylehint_TextColor])
    {
	int val = value[stylehint_TextColor];
	int r = (val >> 16) & 0xff;
	int g = (val >> 8) & 0xff;
	int b = (val >> 0) & 0xff;
	NSColor *color = [NSColor colorWithCalibratedRed: r / 255.0
						   green: g / 255.0
						    blue: b / 255.0
						   alpha: 1.0];
	[dict setObject: color forKey: NSForegroundColorAttributeName];
    }
    
    if (enabled[stylehint_BackColor])
    {
	int val = value[stylehint_BackColor];
	int r = (val >> 16) & 0xff;
	int g = (val >> 8) & 0xff;
	int b = (val >> 0) & 0xff;
	NSColor *color = [NSColor colorWithCalibratedRed: r / 255.0
						   green: g / 255.0
						    blue: b / 255.0
						   alpha: 1.0];
	[dict setObject: color forKey: NSBackgroundColorAttributeName];
    }
    
    if (enabled[stylehint_ReverseColor] && !(enabled[stylehint_TextColor] || enabled[stylehint_BackColor]))
    {
	if (windowtype == wintype_TextGrid)
	{
	    [dict setObject: [Preferences gridBackground] forKey: NSForegroundColorAttributeName];
	    [dict setObject: [Preferences gridForeground] forKey: NSBackgroundColorAttributeName];
	}
	else
	{
	    [dict setObject: [Preferences bufferBackground] forKey: NSForegroundColorAttributeName];
	    [dict setObject: [Preferences bufferForeground] forKey: NSBackgroundColorAttributeName];
	}
    }
}

@end
