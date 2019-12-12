//
//  Font.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-12-12.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Settings;

@interface Font : NSManagedObject

@property (nonatomic, retain) NSNumber * baseline;
@property (nonatomic, retain) id bgcolor;
@property (nonatomic, retain) id color;
@property (nonatomic, retain) NSNumber * leading;
@property (nonatomic, retain) NSString * name;
@property (nonatomic, retain) NSNumber * pitch;
@property (nonatomic, retain) NSNumber * size;
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

@property (readonly, strong) Font *clone;

@end
