//
//  XCTestCase+TemporaryDirectory.m
//  SpatterlightTests
//

#import "XCTestCase+TemporaryDirectory.h"

@implementation XCTestCase (TemporaryDirectory)

- (NSURL *)makeTemporaryDirectory {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSString *name = [NSString stringWithFormat:@"%@-%@",
                      NSStringFromClass(self.class), NSUUID.UUID.UUIDString];
    NSURL *url = [fm.temporaryDirectory URLByAppendingPathComponent:name isDirectory:YES]
        .URLByResolvingSymlinksInPath;
    NSError *error = nil;
    XCTAssertTrue([fm createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:&error],
                  @"Could not create %@: %@", url.path, error);
    [self addTeardownBlock:^{
        [NSFileManager.defaultManager removeItemAtURL:url error:NULL];
    }];
    return url;
}

- (NSURL *)writeData:(NSData *)data to:(NSString *)relativePath in:(NSURL *)directory {
    NSURL *url = [directory URLByAppendingPathComponent:relativePath];
    [NSFileManager.defaultManager createDirectoryAtURL:url.URLByDeletingLastPathComponent
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:NULL];
    XCTAssertTrue([data writeToURL:url atomically:YES],
                  @"Could not write fixture %@", relativePath);
    return url;
}

@end
