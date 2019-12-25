//
//  GlkStyle.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-25.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Settings;

@interface GlkStyle : NSManagedObject

@property (nonatomic, retain) id attributeDict;
@property (nonatomic, retain) NSNumber * baseline;
@property (nonatomic, retain) id bgcolor;
@property (nonatomic, retain) id color;
@property (nonatomic, retain) NSNumber * columnWidth;
@property (nonatomic, retain) id font;
@property (nonatomic, retain) NSNumber * kerning;
@property (nonatomic, retain) NSNumber * leading;
@property (nonatomic, retain) NSNumber * ligatures;
@property (nonatomic, retain) NSNumber * lineHeight;
@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) id paragraphStyle;
@property (nonatomic, retain) NSNumber * pitch;
@property (nonatomic, retain) id shadow;
@property (nonatomic, retain) NSNumber * size;
@property (nonatomic, retain) NSNumber * strikethrough;
@property (nonatomic, retain) id strikethroughColor;
@property (nonatomic, retain) NSNumber * underline;
@property (nonatomic, retain) id underlineColor;
@property (nonatomic, retain) NSString * variant;
@property (nonatomic, retain) Settings *bufAlert;
@property (nonatomic, retain) Settings *bufBlock;
@property (nonatomic, retain) Settings *bufEmph;
@property (nonatomic, retain) Settings *bufferFont;
@property (nonatomic, retain) Settings *bufHead;
@property (nonatomic, retain) Settings *bufInput;
@property (nonatomic, retain) Settings *bufNote;
@property (nonatomic, retain) Settings *bufPre;
@property (nonatomic, retain) Settings *bufSubH;
@property (nonatomic, retain) Settings *bufUsr1;
@property (nonatomic, retain) Settings *bufUsr2;
@property (nonatomic, retain) Settings *gridAlert;
@property (nonatomic, retain) Settings *gridBlock;
@property (nonatomic, retain) Settings *gridEmph;
@property (nonatomic, retain) Settings *gridFont;
@property (nonatomic, retain) Settings *gridHead;
@property (nonatomic, retain) Settings *gridInput;
@property (nonatomic, retain) Settings *gridNote;
@property (nonatomic, retain) Settings *gridPre;
@property (nonatomic, retain) Settings *gridSubH;
@property (nonatomic, retain) Settings *gridUsr1;
@property (nonatomic, retain) Settings *gridUsr2;

- (GlkStyle *)clone;
- (void)recalculateCell;
- (NSMutableDictionary *)attributesWithHints:(NSArray *)hints andGridWindow:(BOOL)gridWindow;

@end
