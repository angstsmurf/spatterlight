//
//  Settings.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Font, Game, Interpreter, Settings;

@interface Settings : NSManagedObject

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
@property (nonatomic, retain) Font *bufAlert;
@property (nonatomic, retain) Font *bufBlock;
@property (nonatomic, retain) Font *bufEmph;
@property (nonatomic, retain) Font *bufferFont;
@property (nonatomic, retain) Font *bufHead;
@property (nonatomic, retain) Font *bufInput;
@property (nonatomic, retain) Font *bufNote;
@property (nonatomic, retain) Font *bufPre;
@property (nonatomic, retain) Font *bufSubH;
@property (nonatomic, retain) Font *bufUsr1;
@property (nonatomic, retain) Font *bufUsr2;
@property (nonatomic, retain) NSSet *game;
@property (nonatomic, retain) Font *gridAlert;
@property (nonatomic, retain) Font *gridBlock;
@property (nonatomic, retain) Font *gridEmph;
@property (nonatomic, retain) Font *gridFont;
@property (nonatomic, retain) Font *gridHead;
@property (nonatomic, retain) Font *gridInput;
@property (nonatomic, retain) Font *gridNote;
@property (nonatomic, retain) Font *gridPre;
@property (nonatomic, retain) Font *gridSubH;
@property (nonatomic, retain) Font *gridUsr1;
@property (nonatomic, retain) Font *gridUsr2;
@property (nonatomic, retain) Game *overrides;
@property (nonatomic, retain) Interpreter *interpreter;
@property (nonatomic, retain) Settings *defaultParent;
@property (nonatomic, retain) NSSet *defaultChild;

@property (readonly, strong) Settings *clone;

@end

@interface Settings (CoreDataGeneratedAccessors)

- (void)addGameObject:(Game *)value;
- (void)removeGameObject:(Game *)value;
- (void)addGame:(NSSet *)values;
- (void)removeGame:(NSSet *)values;

- (void)addDefaultChildObject:(Settings *)value;
- (void)removeDefaultChildObject:(Settings *)value;
- (void)addDefaultChild:(NSSet *)values;
- (void)removeDefaultChild:(NSSet *)values;

@end
