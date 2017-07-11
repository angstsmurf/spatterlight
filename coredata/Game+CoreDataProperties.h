//
//  Game+CoreDataProperties.h
//  Spatterlight
//
//  Created by Administrator on 2017-06-16.
//
//

#import "Game+CoreDataClass.h"


NS_ASSUME_NONNULL_BEGIN

@interface Game (CoreDataProperties)

+ (NSFetchRequest<Game *> *)fetchRequest;

@property (nullable, nonatomic, copy) NSDate *added;
@property (nullable, nonatomic, retain) NSObject *fileLocation;
@property (nonatomic) BOOL found;
@property (nullable, nonatomic, copy) NSString *group;
@property (nullable, nonatomic, copy) NSDate *lastPlayed;
@property (nonatomic) int32_t row;
@property (nullable, nonatomic, retain) NSObject *thumbnail;
@property (nullable, nonatomic, retain) NSObject *infoWindow;
@property (nullable, nonatomic, retain) Interpreter *interpreter;
@property (nullable, nonatomic, retain) Metadata *metadata;
@property (nullable, nonatomic, retain) Settings *setting;

@end

NS_ASSUME_NONNULL_END
