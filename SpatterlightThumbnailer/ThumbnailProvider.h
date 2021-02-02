//
//  ThumbnailProvider.h
//  SpatterlightThumbnails
//
//  Created by Administrator on 2021-01-29.
//

#import <QuickLookThumbnailing/QuickLookThumbnailing.h>

@class NSPersistentContainer;

NS_ASSUME_NONNULL_BEGIN

API_AVAILABLE(macos(10.15))
@interface ThumbnailProvider : QLThumbnailProvider

@property (readonly) NSPersistentContainer *persistentContainer;

@end

NS_ASSUME_NONNULL_END
