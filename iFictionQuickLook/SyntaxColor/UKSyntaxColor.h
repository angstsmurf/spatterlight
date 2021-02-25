//
//  UKSyntaxColor.h
//
//  Created by Uli Kusterer on Tue May 27 2003.
//  Copyright (c) 2003 M. Uli Kusterer. All rights reserved.
//


#import <Cocoa/Cocoa.h>

#define TD_USER_DEFINED_IDENTIFIERS			@"SyntaxColoring:UserIdentifiers"		// Key in user defaults holding user-defined identifiers to colorize.
#define TD_SYNTAX_COLORING_MODE_ATTR		@"UKTextDocumentSyntaxColoringMode"		// Anything we colorize gets this attribute.



// Syntax-colored text file viewer:
@interface UKSyntaxColor : NSObject

@property NSMutableAttributedString *coloredString;

//// Override any of the following in one of your subclasses to customize this object further:
//@property NSString *syntaxDefinitionFilename;   // Defaults to "SyntaxDefinition.plist" in the app bundle's "Resources" directory.
@property NSDictionary *syntaxDefinitionDictionary; // Defaults to loading from -syntaxDefinitionFilename.

@property NSDictionary *defaultTextAttributes;		// Style attributes dictionary for an NSAttributedString.


-(instancetype) initWithString:(NSString *)string;

- (IBAction)recolorCompleteFile: (id)sender;

// Private:
-(void) recolorRange: (NSRange) range;

-(void)	colorOneLineComment: (NSString*) startCh inString: (NSMutableAttributedString*) s
				withColor: (NSColor*) col andMode:(NSString*)attr;
-(void)	colorCommentsFrom: (NSString*) startCh to: (NSString*) endCh inString: (NSMutableAttributedString*) s
				withColor: (NSColor*) col andMode:(NSString*)attr;
-(void)	colorIdentifier: (NSString*) ident inString: (NSMutableAttributedString*) s
				withColor: (NSColor*) col andMode:(NSString*)attr charset: (NSCharacterSet*)cset;
-(void)	colorStringsFrom: (NSString*) startCh to: (NSString*) endCh inString: (NSMutableAttributedString*) s
				withColor: (NSColor*) col andMode:(NSString*)attr andEscapeChar: (NSString*)vStringEscapeCharacter;
-(void)	colorTagFrom: (NSString*) startCh to: (NSString*)endCh inString: (NSMutableAttributedString*) s
				withColor: (NSColor*) col andMode:(NSString*)attr exceptIfMode: (NSString*)ignoreAttr;


@end


