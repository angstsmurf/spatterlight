//
//  GlkStyle.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-29.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import <AppKit/NSColor.h>

@class Theme;

@interface GlkStyle : NSManagedObject

@property (nonatomic, retain) NSDictionary *attributeDict;
@property (nonatomic) BOOL autogenerated;
@property (nonatomic, retain) Theme *bufAlert;
@property (nonatomic, retain) Theme *bufBlock;
@property (nonatomic, retain) Theme *bufEmph;
@property (nonatomic, retain) Theme *bufferNormal;
@property (nonatomic, retain) Theme *bufHead;
@property (nonatomic, retain) Theme *bufInput;
@property (nonatomic, retain) Theme *bufNote;
@property (nonatomic, retain) Theme *bufPre;
@property (nonatomic, retain) Theme *bufSubH;
@property (nonatomic, retain) Theme *bufUsr1;
@property (nonatomic, retain) Theme *bufUsr2;
@property (nonatomic, retain) Theme *gridAlert;
@property (nonatomic, retain) Theme *gridBlock;
@property (nonatomic, retain) Theme *gridEmph;
@property (nonatomic, retain) Theme *gridNormal;
@property (nonatomic, retain) Theme *gridHead;
@property (nonatomic, retain) Theme *gridInput;
@property (nonatomic, retain) Theme *gridNote;
@property (nonatomic, retain) Theme *gridPre;
@property (nonatomic, retain) Theme *gridSubH;
@property (nonatomic, retain) Theme *gridUsr1;
@property (nonatomic, retain) Theme *gridUsr2;

@property (NS_NONATOMIC_IOSONLY, readonly, strong) GlkStyle *clone;
@property (NS_NONATOMIC_IOSONLY, readonly) NSSize cellSize;
- (void)createDefaultAttributeDictionary;
- (BOOL)testGridStyle;
- (NSMutableDictionary<NSAttributedStringKey, id> *)attributesWithHints:(NSArray *)hints;
- (void)printDebugInfo;

@property NSInteger index;
@property (strong, nonatomic) NSFont *font;
@property (strong, nonatomic) NSColor *color;
@property CGFloat lineSpacing;

@end
