//
//  LookForMissingFiles.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-22.
//

#import <Foundation/Foundation.h>

@class Game, TableViewController;

NS_ASSUME_NONNULL_BEGIN

@interface MissingFilesFinder : NSObject

- (void)lookForMissingFile:(Game *)game libController:(TableViewController *)libController;

@end

NS_ASSUME_NONNULL_END
