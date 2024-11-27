//
//  FolderAccess.h
//  Spatterlight
//
//  Created by Administrator on 2021-09-15.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface FolderAccess : NSObject

+ (void)loadBookmarks;
+ (void)saveBookmarks;
+ (void)deleteBookmarks;
+ (void)storeBookmark:(NSURL *)url;
+ (void)releaseBookmark:(NSURL *)url;
+ (BOOL)needsPermissionForURL:(NSURL *)url;
+ (void)forceAccessDialogToURL:(NSURL *)url andThenRunBlock:(void (^)(void))block;
+ (NSURL *)forceRestoreURL:(NSURL *)url;

+ (void)askForAccessToURL:(NSURL *)url andThenRunBlock:(void (^)(void))block;

+ (NSURL *)grantAccessToFile:(NSURL *)url;
+ (nullable NSURL *)grantAccessToFolder:(NSURL *)url;

+ (NSURL *)suitableDirectoryForURL:(NSURL *)url;

+ (nullable NSURL *)restoreURL:(NSURL *)url;
+ (void)listActiveSecurityBookmarks;

@end

NS_ASSUME_NONNULL_END
