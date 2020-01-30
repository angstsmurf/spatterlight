//
//  Theme.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-01-30.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Game, GlkStyle, Interpreter, Theme;

@interface Theme : NSManagedObject

@property (nonatomic) int32_t border;
@property (nonatomic, retain) id bufferBackground;
@property (nonatomic) int32_t bufferMarginX;
@property (nonatomic) int32_t bufferMarginY;
@property (nonatomic) double cellHeight;
@property (nonatomic) double cellWidth;
@property (nonatomic) int32_t dashes;
@property (nonatomic) int32_t defaultCols;
@property (nonatomic) int32_t defaultRows;
@property (nonatomic) BOOL doGraphics;
@property (nonatomic) BOOL doSound;
@property (nonatomic) BOOL doStyles;
@property (nonatomic) BOOL editable;
@property (nonatomic, retain) id gridBackground;
@property (nonatomic) int32_t gridMarginX;
@property (nonatomic) int32_t gridMarginY;
@property (nonatomic) int32_t justify;
@property (nonatomic) int32_t maxCols;
@property (nonatomic) int32_t maxRows;
@property (nonatomic) int32_t minCols;
@property (nonatomic) int32_t minRows;
@property (nonatomic, retain) id morePrompt;
@property (nonatomic, retain) NSString * name;
@property (nonatomic) BOOL smartQuotes;
@property (nonatomic) int32_t spaceFormat;
@property (nonatomic, retain) id spacingColor;
@property (nonatomic) int32_t winSpacingX;
@property (nonatomic) int32_t winSpacingY;
@property (nonatomic, retain) GlkStyle *bufAlert;
@property (nonatomic, retain) GlkStyle *bufBlock;
@property (nonatomic, retain) GlkStyle *bufEmph;
@property (nonatomic, retain) GlkStyle *bufferNormal;
@property (nonatomic, retain) GlkStyle *bufHead;
@property (nonatomic, retain) GlkStyle *bufInput;
@property (nonatomic, retain) GlkStyle *bufNote;
@property (nonatomic, retain) GlkStyle *bufPre;
@property (nonatomic, retain) GlkStyle *bufSubH;
@property (nonatomic, retain) GlkStyle *bufUsr1;
@property (nonatomic, retain) GlkStyle *bufUsr2;
@property (nonatomic, retain) NSSet *defaultChild;
@property (nonatomic, retain) Theme *defaultParent;
@property (nonatomic, retain) NSSet *games;
@property (nonatomic, retain) GlkStyle *gridAlert;
@property (nonatomic, retain) GlkStyle *gridBlock;
@property (nonatomic, retain) GlkStyle *gridEmph;
@property (nonatomic, retain) GlkStyle *gridHead;
@property (nonatomic, retain) GlkStyle *gridInput;
@property (nonatomic, retain) GlkStyle *gridNormal;
@property (nonatomic, retain) GlkStyle *gridNote;
@property (nonatomic, retain) GlkStyle *gridPre;
@property (nonatomic, retain) GlkStyle *gridSubH;
@property (nonatomic, retain) GlkStyle *gridUsr1;
@property (nonatomic, retain) GlkStyle *gridUsr2;
@property (nonatomic, retain) Interpreter *interpreter;
@property (nonatomic, retain) Game *overrides;
@property (nonatomic, retain) Theme *darkTheme;
@property (nonatomic, retain) Theme *lightTheme;

- (Theme *)clone;
- (void)populateStyles;

@end

@interface Theme (CoreDataGeneratedAccessors)

- (void)addDefaultChildObject:(Theme *)value;
- (void)removeDefaultChildObject:(Theme *)value;
- (void)addDefaultChild:(NSSet *)values;
- (void)removeDefaultChild:(NSSet *)values;

- (void)addGamesObject:(Game *)value;
- (void)removeGamesObject:(Game *)value;
- (void)addGames:(NSSet *)values;
- (void)removeGames:(NSSet *)values;

@end
