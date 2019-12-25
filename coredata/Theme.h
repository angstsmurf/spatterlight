//
//  Theme.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Game, Interpreter, GlkStyle;

@interface Theme : NSManagedObject

@property (nonatomic, retain) NSNumber * dashes;
@property (nonatomic, retain) NSNumber * defaultRows;
@property (nonatomic, retain) NSNumber * defaultCols;
@property (nonatomic, retain) NSNumber * doGraphics;
@property (nonatomic, retain) NSNumber * doSound;
@property (nonatomic, retain) NSNumber * doStyles;
@property (nonatomic, retain) NSNumber * justify;
@property (nonatomic, retain) NSNumber * smartQuotes;
@property (nonatomic, retain) NSNumber * spaceFormat;
@property (nonatomic, retain) NSNumber * border;
@property (nonatomic, retain) NSNumber * bufferMarginX;
@property (nonatomic, retain) NSNumber * winSpacingX;
@property (nonatomic, retain) NSString * themeName;
@property (nonatomic, retain) NSNumber * minRows;
@property (nonatomic, retain) NSNumber * minCols;
@property (nonatomic, retain) NSNumber * maxRows;
@property (nonatomic, retain) NSNumber * maxCols;
@property (nonatomic, retain) id morePrompt;
@property (nonatomic, retain) NSNumber * bufferMarginY;
@property (nonatomic, retain) NSNumber * winSpacingY;
@property (nonatomic, retain) id spacingColor;
@property (nonatomic, retain) NSNumber * gridMarginX;
@property (nonatomic, retain) NSNumber * gridMarginY;
@property (nonatomic, retain) id gridBackground;
@property (nonatomic, retain) id bufferBackground;
@property (nonatomic, retain) NSNumber * editable;
@property (nonatomic, retain) GlkStyle *bufAlert;
@property (nonatomic, retain) GlkStyle *bufBlock;
@property (nonatomic, retain) GlkStyle *bufEmph;
@property (nonatomic, retain) GlkStyle *bufferFont;
@property (nonatomic, retain) GlkStyle *bufHead;
@property (nonatomic, retain) GlkStyle *bufInput;
@property (nonatomic, retain) GlkStyle *bufNote;
@property (nonatomic, retain) GlkStyle *bufPre;
@property (nonatomic, retain) GlkStyle *bufSubH;
@property (nonatomic, retain) GlkStyle *bufUsr1;
@property (nonatomic, retain) GlkStyle *bufUsr2;
@property (nonatomic, retain) NSSet *game;
@property (nonatomic, retain) GlkStyle *gridAlert;
@property (nonatomic, retain) GlkStyle *gridBlock;
@property (nonatomic, retain) GlkStyle *gridEmph;
@property (nonatomic, retain) GlkStyle *gridFont;
@property (nonatomic, retain) GlkStyle *gridHead;
@property (nonatomic, retain) GlkStyle *gridInput;
@property (nonatomic, retain) GlkStyle *gridNote;
@property (nonatomic, retain) GlkStyle *gridPre;
@property (nonatomic, retain) GlkStyle *gridSubH;
@property (nonatomic, retain) GlkStyle *gridUsr1;
@property (nonatomic, retain) GlkStyle *gridUsr2;
@property (nonatomic, retain) Game *overrides;
@property (nonatomic, retain) Interpreter *interpreter;
@property (nonatomic, retain) Theme *defaultParent;
@property (nonatomic, retain) NSSet *defaultChild;

@property (readonly, strong) Theme *clone;

@end

@interface Theme (CoreDataGeneratedAccessors)

- (void)addGameObject:(Game *)value;
- (void)removeGameObject:(Game *)value;
- (void)addGame:(NSSet *)values;
- (void)removeGame:(NSSet *)values;

- (void)addDefaultChildObject:(Theme *)value;
- (void)removeDefaultChildObject:(Theme *)value;
- (void)addDefaultChild:(NSSet *)values;
- (void)removeDefaultChild:(NSSet *)values;

@end
