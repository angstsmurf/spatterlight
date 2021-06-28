//
//  Theme.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2020-01-30.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Game, GlkStyle, Interpreter, Theme, NSColor;

typedef enum kSpacesFormatType : int32_t {
    TAG_SPACES_GAME,
    TAG_SPACES_ONE,
    TAG_SPACES_TWO
} kSpacesFormatType;

typedef enum kCoverImagePrefsType : int32_t {
    kDontShow,
    kShowAndWait,
    kShowAndFade,
    kShowDramatic
} kCoverImagePrefsType;

typedef enum kBorderColorPrefsType : int32_t {
    kAutomatic,
    kUserOverride
} kBorderColorPrefsType;

typedef enum kVOMenuPrefsType : int32_t {
    kVOMenuNone,
    kVOMenuTextOnly,
    kVOMenuIndex,
    kVOMenuTotal
} kVOMenuPrefsType;

typedef enum kVOImagePrefsType : int32_t {
    kVOImageWithDescriptionOnly,
    kVOImageNone,
    kVOImageAll
} kVOImagePrefsType;

typedef enum kBZArrowsPrefsType : int32_t {
    kBZArrowsCompromise,
    kBZArrowsSwapped,
    kBZArrowsOriginal,
} kBZArrowsPrefsType;


NS_ASSUME_NONNULL_BEGIN

@interface Theme : NSManagedObject

@property (nonatomic) BOOL autosave;
@property (nonatomic) BOOL autosaveOnTimer;
@property (nullable, nonatomic, copy) NSString *beepHigh;
@property (nullable, nonatomic, copy) NSString *beepLow;
@property (nonatomic) int32_t border;
@property (nonatomic) kBorderColorPrefsType borderBehavior;
@property (nullable, nonatomic, retain) NSColor *borderColor;
@property (nullable, nonatomic, retain) NSColor *bufferBackground;
@property (nonatomic) double bufferCellHeight;
@property (nonatomic) double bufferCellWidth;
@property (nonatomic) int32_t bufferMarginX;
@property (nonatomic) int32_t bufferMarginY;
@property (nonatomic) int32_t bZAdjustment;
@property (nonatomic) kBZArrowsPrefsType bZTerminator;
@property (nonatomic) double cellHeight;
@property (nonatomic) double cellWidth;
@property (nonatomic) kCoverImagePrefsType coverArtStyle;
@property (nonatomic) int32_t cursorShape;
@property (nonatomic) int32_t dashes;
@property (nonatomic) int32_t defaultCols;
@property (nonatomic) int32_t defaultRows;
@property (nonatomic) BOOL determinism;
@property (nonatomic) BOOL doGraphics;
@property (nonatomic) BOOL doSound;
@property (nonatomic) BOOL doStyles;
@property (nonatomic) BOOL editable;
@property (nonatomic) int32_t errorHandling;
@property (nullable, nonatomic, retain) NSColor *gridBackground;
@property (nonatomic) int32_t gridMarginX;
@property (nonatomic) int32_t gridMarginY;
@property (nonatomic) int32_t imageSizing;
@property (nonatomic) int32_t justify;
@property (nonatomic) int32_t maxCols;
@property (nonatomic) int32_t maxRows;
@property (nonatomic) int32_t minCols;
@property (nonatomic) int32_t minRows;
@property (nonatomic) double minTimer;
@property (nullable, nonatomic, retain) NSObject *morePrompt;
@property (nullable, nonatomic, copy) NSString *name;
@property (nonatomic) int32_t nohacks;
@property (nonatomic) int32_t quoteBox;
@property (nonatomic) BOOL slowDrawing;
@property (nonatomic) BOOL smartQuotes;
@property (nonatomic) BOOL smoothScroll;
@property (nonatomic) kSpacesFormatType spaceFormat;
@property (nullable, nonatomic, retain) NSColor *spacingColor;
@property (nonatomic) int32_t winSpacingX;
@property (nonatomic) int32_t winSpacingY;
@property (nonatomic) int32_t zMachineTerp;
@property (nullable, nonatomic, copy) NSString *zMachineLetter;
@property (nonatomic) int32_t vOExtraElements;
@property (nonatomic) int32_t vOSpeakCommand;
@property (nonatomic) int32_t vOSpeakInputType;
@property (nonatomic) kVOImagePrefsType vOSpeakImages;
@property (nonatomic) kVOMenuPrefsType vOSpeakMenu;
@property (nullable, nonatomic, retain) GlkStyle *bufAlert;
@property (nullable, nonatomic, retain) GlkStyle *bufBlock;
@property (nullable, nonatomic, retain) GlkStyle *bufEmph;
@property (nullable, nonatomic, retain) GlkStyle *bufferNormal;
@property (nullable, nonatomic, retain) GlkStyle *bufHead;
@property (nullable, nonatomic, retain) GlkStyle *bufInput;
@property (nullable, nonatomic, retain) GlkStyle *bufNote;
@property (nullable, nonatomic, retain) GlkStyle *bufPre;
@property (nullable, nonatomic, retain) GlkStyle *bufSubH;
@property (nullable, nonatomic, retain) GlkStyle *bufUsr1;
@property (nullable, nonatomic, retain) GlkStyle *bufUsr2;
@property (nullable, nonatomic, retain) Theme *darkTheme;
@property (nullable, nonatomic, retain) NSSet<Theme *> *defaultChild;
@property (nullable, nonatomic, retain) Theme *defaultParent;
@property (nullable, nonatomic, retain) NSSet<Game *> *games;
@property (nullable, nonatomic, retain) GlkStyle *gridAlert;
@property (nullable, nonatomic, retain) GlkStyle *gridBlock;
@property (nullable, nonatomic, retain) GlkStyle *gridEmph;
@property (nullable, nonatomic, retain) GlkStyle *gridHead;
@property (nullable, nonatomic, retain) GlkStyle *gridInput;
@property (nullable, nonatomic, retain) GlkStyle *gridNormal;
@property (nullable, nonatomic, retain) GlkStyle *gridNote;
@property (nullable, nonatomic, retain) GlkStyle *gridPre;
@property (nullable, nonatomic, retain) GlkStyle *gridSubH;
@property (nullable, nonatomic, retain) GlkStyle *gridUsr1;
@property (nullable, nonatomic, retain) GlkStyle *gridUsr2;
@property (nullable, nonatomic, retain) Interpreter *interpreter;
@property (nullable, nonatomic, retain) Theme *lightTheme;
@property (nullable, nonatomic, retain) Game *overrides;

- (Theme *)clone;
- (void)copyAttributesFrom:(Theme *)theme;
- (void)populateStyles;
- (NSArray *)allStyles;
- (BOOL)hasCustomStyles;

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
- (void)resetCommonValues;

@end

NS_ASSUME_NONNULL_END
