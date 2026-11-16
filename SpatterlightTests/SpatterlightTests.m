//
//  SpatterlightTests.m
//  Tests
//
//  Created by Administrator on 2021-07-20.
//
//  Draws the ScottFree test pictures with the Apple II and Atari 8-bit vector
//  renderers and compares the screen memory with the reference files.
//
//  The ScottFree interpreter is built as a command-line tool, not a linkable
//  library, so the test target compiles the filltest harness
//  (terps/scott/test/pictures/filltest.c) and the renderer sources it links; the
//  list is SRCS in terps/scott/test/pictures/Makefile. The harness stubs out Glk
//  and the rest of the interpreter, so the test needs no external process.
//

#import <XCTest/XCTest.h>

/* terps/scott/test/pictures/filltest.c */
int RunVectorFixtures(const char *gamedir, const char *imagedir);

@interface Tests : XCTestCase
@end

@implementation Tests

- (void)testScottDraw {
    // The test bundle holds the game disk and the picture files in one
    // flat resource directory.
    NSURL *url = [[NSBundle bundleForClass:Tests.class] URLForResource:@"atari8mi_boom"
                                                         withExtension:@"dat"
                                                          subdirectory:nil];
    XCTAssertNotNil(url, @"The picture test files are not in the test bundle");
    if (url == nil)
        return;

    NSString *supportpath = [[url URLByDeletingLastPathComponent].path stringByAppendingString:@"/"];

    XCTAssertTrue(RunVectorFixtures(supportpath.fileSystemRepresentation,
                                    supportpath.fileSystemRepresentation),
                  @"A vector picture differs from its reference");
}

@end
