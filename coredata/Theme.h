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

typedef NS_ENUM(int32_t, kSpacesFormatType) {
    TAG_SPACES_GAME,
    TAG_SPACES_ONE,
    TAG_SPACES_TWO
};

typedef NS_ENUM(int32_t, kCoverImagePrefsType) {
    kDontShow,
    kShowAndWait,
    kShowAndFade,
    kShowAsBezel
};

typedef NS_ENUM(int32_t, kBorderColorPrefsType) {
    kAutomatic,
    kUserOverride
};

typedef NS_ENUM(int32_t, kVOMenuPrefsType) {
    kVOMenuNone,
    kVOMenuTextOnly,
    kVOMenuIndex,
    kVOMenuTotal
};

typedef NS_ENUM(int32_t, kVOImagePrefsType) {
    kVOImageWithDescriptionOnly,
    kVOImageNone,
    kVOImageAll
};

typedef NS_ENUM(int32_t, kBZArrowsPrefsType) {
    kBZArrowsCompromise,
    kBZArrowsSwapped,
    kBZArrowsOriginal,
};

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
@property (nullable, nonatomic, retain) NSColor *bufLinkColor;
@property (nonatomic) int32_t bufLinkStyle;
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
@property (nonatomic) BOOL flicker;
@property (nullable, nonatomic, retain) NSColor *gridBackground;
@property (nullable, nonatomic, retain) NSColor *gridLinkColor;
@property (nonatomic) int32_t gridLinkStyle;
@property (nonatomic) int32_t gridMarginX;
@property (nonatomic) int32_t gridMarginY;
@property (nonatomic) BOOL hardDark;
@property (nonatomic) BOOL hardLight;
@property (nonatomic) BOOL hardLightOrDark;
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
@property (nonatomic) int32_t sADelays;
@property (nonatomic) int32_t sADisplayStyle;
@property (nonatomic) int32_t sAInventory;
@property (nonatomic) int32_t sAPalette;
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

@property (NS_NONATOMIC_IOSONLY, readonly, strong) Theme * _Nonnull clone;
- (void)copyAttributesFrom:(Theme *)theme;
- (void)populateStyles;
@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray<GlkStyle *> * _Nonnull allStyles;
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL hasCustomStyles;

@end

@interface Theme (CoreDataGeneratedAccessors)

- (void)addDefaultChildObject:(Theme *)value;
- (void)removeDefaultChildObject:(Theme *)value;
- (void)addDefaultChild:(NSSet<Theme *> *)values;
- (void)removeDefaultChild:(NSSet<Theme *> *)values;

- (void)addGamesObject:(Game *)value;
- (void)removeGamesObject:(Game *)value;
- (void)addGames:(NSSet<Game *> *)values;
- (void)removeGames:(NSSet<Game *> *)values;
- (void)resetCommonValues;

@end

NS_ASSUME_NONNULL_END
