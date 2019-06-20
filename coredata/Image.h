//
//  Image.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2019-06-25.
//
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>

@class Metadata;

@interface Image : NSManagedObject

@property (nonatomic, retain) id data;
@property (nonatomic, retain) NSString * originalURL;
@property (nonatomic, retain) NSString * type;
@property (nonatomic, retain) Metadata *metadata;

@end
