/* =============================================================================
 FILE:		UKSyntaxColoredTextDocument.m
 PROJECT:	CocoaTads

 AUTHORS:	M. Uli Kusterer <witness_at_zathras_dot_de>, (c) 2003, all rights
 reserved.

 REVISIONS:
 2003-05-31	UK	Created.
 ========================================================================== */

#import "UKSyntaxColor.h"
#import "NSScanner+SkipUpToCharset.h"


@interface UKSyntaxColor ()
{
    // Status display for things like syntax coloring or background syntax checks.
    NSString*                        sourceCode;                // Temp. storage for data from file until NIB has been read.
    BOOL                            syntaxColoringBusy;        // Set while recolorRange is busy, so we don't recursively call recolorRange.
}
@end

@implementation UKSyntaxColor


/* -----------------------------------------------------------------------------
 init:
 Constructor that inits sourceCode member variable as a flag. It's
 storage for the text until the NIB's been loaded.
 -------------------------------------------------------------------------- */

-(instancetype)	initWithString:(NSString *)string
{
    self = [super init];
    if (self)
    {
        sourceCode = string;
        syntaxColoringBusy = NO;
        _coloredString = [[NSMutableAttributedString alloc] initWithString:string];
        [_coloredString addAttribute:NSForegroundColorAttributeName value:[NSColor controlTextColor] range:NSMakeRange(0, _coloredString.length)];
    }
    return self;
}





/* -----------------------------------------------------------------------------
 recolorCompleteFile:
 IBAction to do a complete recolor of the whole friggin' document.
 This is called once after the document's been loaded and leaves some
 custom styles in the document which are used by recolorRange to properly
 perform recoloring of parts.
 -------------------------------------------------------------------------- */

-(IBAction)	recolorCompleteFile: (id)sender
{
        NSRange		range = NSMakeRange(0,[_coloredString length]);
        [self recolorRange: range];
}


/* -----------------------------------------------------------------------------
 recolorRange:
 Try to apply syntax coloring to the text in our text view. This
 overwrites any styles the text may have had before. This function
 guarantees that it'll preserve the selection.

 Note that the order in which the different things are colorized is
 important. E.g. identifiers go first, followed by comments, since that
 way colors are removed from identifiers inside a comment and replaced
 with the comment color, etc.

 The range passed in here is special, and may not include partial
 identifiers or the end of a comment. Make sure you include the entire
 multi-line comment etc. or it'll lose color.
 -------------------------------------------------------------------------- */

-(void)	recolorRange: (NSRange)range
{
    if (syntaxColoringBusy)
        return;
    // Kludge fix for case where we sometimes exceed text length:ra
    NSRange all = NSMakeRange(0, _coloredString.length);
    range = NSIntersectionRange(all, range);

    syntaxColoringBusy = YES;


    // Get the text we'll be working with:
    NSMutableAttributedString*	vString = [[NSMutableAttributedString alloc] initWithString: [[_coloredString string] substringWithRange: range]];

    // Load colors and fonts to use from preferences:

    // Load our dictionary which contains info on coloring this language:
    NSDictionary*				vSyntaxDefinition = [self syntaxDefinitionDictionary];
    NSEnumerator*				vComponentsEnny = [[vSyntaxDefinition objectForKey: @"Components"] objectEnumerator];

    // Loop over all available components:
    NSDictionary*				vCurrComponent = nil;
    NSDictionary*				vStyles = [self defaultTextAttributes];
//    NSUserDefaults*				vPrefs = [NSUserDefaults standardUserDefaults];

    while( (vCurrComponent = [vComponentsEnny nextObject]) )
    {
        NSString*   vComponentType = [vCurrComponent objectForKey: @"Type"];
        NSString*   vComponentName = [vCurrComponent objectForKey: @"Name"];
//        NSString*   vColorKeyName = [@"SyntaxColoring:Color:" stringByAppendingString: vComponentName];
//        NSArray * colorArray =  [vCurrComponent objectForKey: @"Color"];
        NSString * colorName =  [vCurrComponent objectForKey: @"Color"];
        NSColor*    vColor;
        if (colorName && colorName.length) {
            if (@available(macOS 10.13, *)) {
                vColor = [NSColor colorNamed:colorName];
            }
        }

        if( !vColor )
            vColor =  [NSColor textColor];

        if( [vComponentType isEqualToString: @"BlockComment"] )
        {
            [self colorCommentsFrom: [vCurrComponent objectForKey: @"Start"]
                                 to: [vCurrComponent objectForKey: @"End"] inString: vString
                          withColor: vColor andMode: vComponentName];
        }
        else if( [vComponentType isEqualToString: @"OneLineComment"] )
        {
            [self colorOneLineComment: [vCurrComponent objectForKey: @"Start"]
                             inString: vString withColor: vColor andMode: vComponentName];
        }
        else if( [vComponentType isEqualToString: @"String"] )
        {
            [self colorStringsFrom: [vCurrComponent objectForKey: @"Start"]
                                to: [vCurrComponent objectForKey: @"End"]
                          inString: vString withColor: vColor andMode: vComponentName
                     andEscapeChar: [vCurrComponent objectForKey: @"EscapeChar"]];
        }
        else if( [vComponentType isEqualToString: @"Tag"] )
        {
            [self colorTagFrom: [vCurrComponent objectForKey: @"Start"]
                            to: [vCurrComponent objectForKey: @"End"] inString: vString
                     withColor: vColor andMode: vComponentName
                  exceptIfMode: [vCurrComponent objectForKey: @"IgnoredComponent"]];
        }
        else if( [vComponentType isEqualToString: @"Keywords"] )
        {
            NSArray* vIdents = [vCurrComponent objectForKey: @"Keywords"];
            if( !vIdents )
                vIdents = [[NSUserDefaults standardUserDefaults] objectForKey: [@"SyntaxColoring:Keywords:" stringByAppendingString: vComponentName]];
            if( !vIdents && [vComponentName isEqualToString: @"UserIdentifiers"] )
                vIdents = [[NSUserDefaults standardUserDefaults] objectForKey: TD_USER_DEFINED_IDENTIFIERS];
            if( vIdents )
            {
                NSCharacterSet*		vIdentCharset = nil;
                NSString*			vCurrIdent = nil;
                NSString*			vCsStr = [vCurrComponent objectForKey: @"Charset"];
                if( vCsStr )
                    vIdentCharset = [NSCharacterSet characterSetWithCharactersInString: vCsStr];

                NSEnumerator*	vItty = [vIdents objectEnumerator];
                while( vCurrIdent = [vItty nextObject] )
                    [self colorIdentifier: vCurrIdent inString: vString withColor: vColor
                                  andMode: vComponentName charset: vIdentCharset];
            }
        }
    }

    // Replace the range with our recolored part:
    [vString addAttributes: vStyles range: NSMakeRange( 0, [vString length] )];
    [_coloredString replaceCharactersInRange: range withAttributedString: vString];
    [_coloredString fixFontAttributeInRange: range];    // Make sure Japanese etc. fallback fonts get applied.
    syntaxColoringBusy = NO;
}




/* -----------------------------------------------------------------------------
 syntaxDefinitionFilename:
 Like windowNibName, this should return the name of the syntax
 definition file to use. Advanced users may use this to allow different
 coloring to take place depending on the file extension by returning
 different file names here.

 Note that the ".plist" extension is automatically appended to the file
 name.
 -------------------------------------------------------------------------- */

//@synthesize syntaxDefinitionFilename;

//- (NSString*)syntaxDefinitionFilename
//{
//    return @"SyntaxDefinition";
//}


/* -----------------------------------------------------------------------------
 syntaxDefinitionDictionary:
 This returns the syntax definition dictionary to use, which indicates
 what ranges of text to colorize. Advanced users may use this to allow
 different coloring to take place depending on the file extension by
 returning different dictionaries here.

 By default, this simply reads a dictionary from the .plist file
 indicated by -syntaxDefinitionFilename.
 -------------------------------------------------------------------------- */

//-(NSDictionary*)	syntaxDefinitionDictionary
//{
//    return [NSDictionary dictionaryWithContentsOfFile: [[NSBundle mainBundle] pathForResource: [self syntaxDefinitionFilename] ofType:@"plist"]];
//}


/* -----------------------------------------------------------------------------
 colorStringsFrom:
 Apply syntax coloring to all strings. This is basically the same code
 as used for multi-line comments, except that it ignores the end
 character if it is preceded by a backslash.
 -------------------------------------------------------------------------- */

-(void) colorStringsFrom: (NSString*) startCh to: (NSString*) endCh inString: (NSMutableAttributedString*) s
                            withColor: (NSColor*) col andMode:(NSString*)attr andEscapeChar: (NSString*)vStringEscapeCharacter
{
    NS_DURING
        NSScanner*                    vScanner = [NSScanner scannerWithString: [s string]];
        NSDictionary*                vStyles = [NSDictionary dictionaryWithObjectsAndKeys:
                                                    col, NSForegroundColorAttributeName,
                                                    attr, TD_SYNTAX_COLORING_MODE_ATTR,
                                                    nil];
        BOOL                        vIsEndChar = NO;
        unichar                        vEscChar = '\\';

        if( vStringEscapeCharacter )
        {
            if( [vStringEscapeCharacter length] != 0 )
                vEscChar = [vStringEscapeCharacter characterAtIndex: 0];
        }

        while( ![vScanner isAtEnd] )
        {
            NSInteger        vStartOffs,
                    vEndOffs;
            vIsEndChar = NO;

            // Look for start of string:
            [vScanner scanUpToString: startCh intoString: nil];
            vStartOffs = [vScanner scanLocation];
            if( ![vScanner scanString:startCh intoString:nil] )
                NS_VOIDRETURN;

            while( !vIsEndChar && ![vScanner isAtEnd] )    // Loop until we find end-of-string marker or our text to color is finished:
            {
                [vScanner scanUpToString: endCh intoString: nil];
                if( ([vStringEscapeCharacter length] == 0) || [[s string] characterAtIndex: ([vScanner scanLocation] -1)] != vEscChar )    // Backslash before the end marker? That means ignore the end marker.
                    vIsEndChar = YES;    // A real one! Terminate loop.
                if( ![vScanner scanString:endCh intoString:nil] )    // But skip this char before that.
                    NS_VOIDRETURN;

            }

            vEndOffs = [vScanner scanLocation];

            // Now mess with the string's styles:
            [s setAttributes: vStyles range: NSMakeRange( vStartOffs, vEndOffs -vStartOffs )];
        }
    NS_HANDLER
        // Just ignore it, syntax coloring isn't that important.
    NS_ENDHANDLER
}


/* -----------------------------------------------------------------------------
 colorCommentsFrom:
 Colorize block-comments in the text view.

 REVISIONS:
 2004-05-18  witness Documented.
 -------------------------------------------------------------------------- */

-(void)	colorCommentsFrom: (NSString*) startCh to: (NSString*) endCh inString: (NSMutableAttributedString*) s
                withColor: (NSColor*) col andMode:(NSString*)attr
{
    NS_DURING
    NSScanner*					vScanner = [NSScanner scannerWithString: [s string]];
    NSDictionary*				vStyles = [NSDictionary dictionaryWithObjectsAndKeys:
                                           col, NSForegroundColorAttributeName,
                                           attr, TD_SYNTAX_COLORING_MODE_ATTR,
                                           nil];

    while( ![vScanner isAtEnd] )
    {
        NSInteger		vStartOffs,
        vEndOffs;

        // Look for start of multi-line comment:
        [vScanner scanUpToString: startCh intoString: nil];
        vStartOffs = [vScanner scanLocation];
        if( ![vScanner scanString:startCh intoString:nil] )
            NS_VOIDRETURN;

        // Look for associated end-of-comment marker:
        [vScanner scanUpToString: endCh intoString: nil];
        if( ![vScanner scanString:endCh intoString:nil] )
        /*NS_VOIDRETURN*/;  // Don't exit. If user forgot trailing marker, indicate this by "bleeding" until end of string.
        vEndOffs = [vScanner scanLocation];

        // Now mess with the string's styles:
        [s setAttributes: vStyles range: NSMakeRange( vStartOffs, vEndOffs -vStartOffs )];

    }
    NS_HANDLER
    // Just ignore it, syntax coloring isn't that important.
    NS_ENDHANDLER
}


/* -----------------------------------------------------------------------------
 colorOneLineComment:
 Colorize one-line-comments in the text view.

 REVISIONS:
 2004-05-18  witness Documented.
 -------------------------------------------------------------------------- */

-(void)    colorOneLineComment: (NSString*) startCh inString: (NSMutableAttributedString*) s
                withColor: (NSColor*) col andMode:(NSString*)attr
{
    NS_DURING
        NSScanner*                    vScanner = [NSScanner scannerWithString: [s string]];
        NSDictionary*                vStyles = [NSDictionary dictionaryWithObjectsAndKeys:
                                                    col, NSForegroundColorAttributeName,
                                                    attr, TD_SYNTAX_COLORING_MODE_ATTR,
                                                    nil];

        while( ![vScanner isAtEnd] )
        {
            NSInteger        vStartOffs,
                    vEndOffs;

            // Look for start of one-line comment:
            [vScanner scanUpToString: startCh intoString: nil];
            vStartOffs = [vScanner scanLocation];
            if( ![vScanner scanString:startCh intoString:nil] )
                NS_VOIDRETURN;

            // Look for associated line break:
            if( ![vScanner skipUpToCharactersFromSet:[NSCharacterSet characterSetWithCharactersInString: @"\n\r"]] )
                ;

            vEndOffs = [vScanner scanLocation];

            // Now mess with the string's styles:
            [s setAttributes: vStyles range: NSMakeRange( vStartOffs, vEndOffs -vStartOffs )];
        }
    NS_HANDLER
        // Just ignore it, syntax coloring isn't that important.
    NS_ENDHANDLER
}


/* -----------------------------------------------------------------------------
 colorIdentifier:
 Colorize keywords in the text view.

 REVISIONS:
 2004-05-18  witness Documented.
 -------------------------------------------------------------------------- */

-(void)    colorIdentifier: (NSString*) ident inString: (NSMutableAttributedString*) s
            withColor: (NSColor*) col andMode:(NSString*)attr charset: (NSCharacterSet*)cset
{
    NS_DURING
        NSScanner*                    vScanner = [NSScanner scannerWithString: [s string]];
        NSDictionary*                vStyles = [NSDictionary dictionaryWithObjectsAndKeys:
                                                    col, NSForegroundColorAttributeName,
                                                    attr, TD_SYNTAX_COLORING_MODE_ATTR,
                                                    nil];
        NSInteger                            vStartOffs = 0;

        // Skip any leading whitespace chars, somehow NSScanner doesn't do that:
        if( cset )
        {
            while( vStartOffs < [[s string] length] )
            {
                if( [cset characterIsMember: [[s string] characterAtIndex: vStartOffs]] )
                    break;
                vStartOffs++;
            }
        }

        [vScanner setScanLocation: vStartOffs];

        while( ![vScanner isAtEnd] )
        {
            // Look for start of identifier:
            [vScanner scanUpToString: ident intoString: nil];
            vStartOffs = [vScanner scanLocation];
            if( ![vScanner scanString:ident intoString:nil] )
                NS_VOIDRETURN;

            if( vStartOffs > 0 )    // Check that we're not in the middle of an identifier:
            {
                // Alphanum character before identifier start?
                if( [cset characterIsMember: [[s string] characterAtIndex: (vStartOffs -1)]] )  // If charset is NIL, this evaluates to NO.
                    continue;
            }

            if( (vStartOffs +[ident length] +1) < [s length] )
            {
                // Alphanum character following our identifier?
                if( [cset characterIsMember: [[s string] characterAtIndex: (vStartOffs +[ident length])]] )  // If charset is NIL, this evaluates to NO.
                    continue;
            }

            // Now mess with the string's styles:
            [s setAttributes: vStyles range: NSMakeRange( vStartOffs, [ident length] )];
        }

    NS_HANDLER
        // Just ignore it, syntax coloring isn't that important.
    NS_ENDHANDLER
}


/* -----------------------------------------------------------------------------
 colorTagFrom:
 Colorize HTML tags or similar constructs in the text view.

 REVISIONS:
 2004-05-18  witness Documented.
 -------------------------------------------------------------------------- */

-(void)	colorTagFrom: (NSString*) startCh to: (NSString*)endCh inString: (NSMutableAttributedString*) s
           withColor: (NSColor*) col andMode:(NSString*)attr exceptIfMode: (NSString*)ignoreAttr
{
    NS_DURING
    NSScanner*					vScanner = [NSScanner scannerWithString: [s string]];
    NSDictionary*				vStyles = [NSDictionary dictionaryWithObjectsAndKeys:
                                           col, NSForegroundColorAttributeName,
                                           attr, TD_SYNTAX_COLORING_MODE_ATTR,
                                           nil];

    while( ![vScanner isAtEnd] )
    {
        NSInteger		vStartOffs,
        vEndOffs;

        // Look for start of one-line comment:
        [vScanner scanUpToString: startCh intoString: nil];
        vStartOffs = [vScanner scanLocation];
        if( vStartOffs >= [s length] )
            NS_VOIDRETURN;
        NSString*   scMode = [[s attributesAtIndex:vStartOffs effectiveRange: nil] objectForKey: TD_SYNTAX_COLORING_MODE_ATTR];
        if( ![vScanner scanString:startCh intoString:nil] )
            NS_VOIDRETURN;

        // If start lies in range of ignored style, don't colorize it:
        if( ignoreAttr != nil && [scMode isEqualToString: ignoreAttr] )
            continue;

        // Look for matching end marker:
        while( ![vScanner isAtEnd] )
        {
            // Scan up to the next occurence of the terminating sequence:
            [vScanner scanUpToString: endCh intoString:nil];

            // Now, if the mode of the end marker is not the mode we were told to ignore,
            //  we're finished now and we can exit the inner loop:
            vEndOffs = [vScanner scanLocation];
            if( vEndOffs < [s length] )
            {
                scMode = [[s attributesAtIndex:vEndOffs effectiveRange: nil] objectForKey: TD_SYNTAX_COLORING_MODE_ATTR];
                [vScanner scanString: endCh intoString: nil];   // Also skip the terminating sequence.
                if( ignoreAttr == nil || ![scMode isEqualToString: ignoreAttr] )
                    break;
            }

            // Otherwise we keep going, look for the next occurence of endCh and hope it isn't in that style.
        }

        vEndOffs = [vScanner scanLocation];

        // Now mess with the string's styles:
        [s setAttributes: vStyles range: NSMakeRange( vStartOffs, vEndOffs -vStartOffs )];

    }
    NS_HANDLER
    // Just ignore it, syntax coloring isn't that important.
    NS_ENDHANDLER
}


/* -----------------------------------------------------------------------------
 defaultTextAttributes:
 Return the text attributes to use for the text in our text view.

 REVISIONS:
 2004-05-18  witness Documented.
 -------------------------------------------------------------------------- */

//-(NSDictionary*)	defaultTextAttributes
//{
//    return [NSDictionary dictionaryWithObject: [NSFont userFixedPitchFontOfSize:10.0] forKey: NSFontAttributeName];
//}




@end
