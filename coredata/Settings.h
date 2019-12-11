//
//  Settings.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-25.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Font, Game;

@interface Settings : NSManagedObject

@property (nonatomic, retain) NSNumber * dashes;
@property (nonatomic, retain) NSNumber * defaultHeight;
@property (nonatomic, retain) NSNumber * defaultWidth;
@property (nonatomic, retain) NSNumber * doGraphics;
@property (nonatomic, retain) NSNumber * doSound;
@property (nonatomic, retain) NSNumber * doStyles;
@property (nonatomic, retain) NSNumber * justify;
@property (nonatomic, retain) NSNumber * smartQuotes;
@property (nonatomic, retain) NSNumber * spaceFormat;
@property (nonatomic, retain) NSNumber * wborder;
@property (nonatomic, retain) NSNumber * wmargin;
@property (nonatomic, retain) NSNumber * wpadding;
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
@property (nonatomic, retain) Game *game;
@property (nonatomic, retain) Game *overrides;

@property (readonly, strong) Settings *clone;

@end
