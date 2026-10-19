//
//  XCTestCase+TemporaryDirectory.h
//  SpatterlightTests
//

#import <XCTest/XCTest.h>

NS_ASSUME_NONNULL_BEGIN

@interface XCTestCase (TemporaryDirectory)

// A new, empty folder of its own under the temporary directory, named after
// the test class, removed again when the test finishes. Symlinks are
// resolved (/var -> /private/var), so paths built from it compare equal to
// the ones the app gets back from the file system.
- (NSURL *)makeTemporaryDirectory;

// Writes data to relativePath under directory, creating any folders on the
// way, and fails the test if it can't.
- (NSURL *)writeData:(NSData *)data to:(NSString *)relativePath in:(NSURL *)directory;

@end

NS_ASSUME_NONNULL_END
