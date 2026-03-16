//
//  SpatterlightTEsts.m
//  Tests
//
//  Created by Administrator on 2021-07-20.
//

#import <XCTest/XCTest.h>

#import "FolderAccess.h"

#include "glk.h"
#include "protocol.h"

@interface Tests : XCTestCase


@end

@interface Tests () {
    /* for talking to an interpreter */
    NSTask *task;
    NSFileHandle *readfh;
    NSFileHandle *sendfh;
    XCTestExpectation *terpExpectation;
    NSURL *secureBookmark;
}

@end

@implementation Tests
- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.

}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)forkInterpreterTask:(NSString *)terpName {
    /* Fork the interpreter process */

    NSString *terppath;
    NSPipe *readpipe;
    NSPipe *sendpipe;

    terppath = [[NSBundle mainBundle] pathForAuxiliaryExecutable:terpName];
    readpipe = [NSPipe pipe];
    sendpipe = [NSPipe pipe];
    readfh = readpipe.fileHandleForReading;
    sendfh = sendpipe.fileHandleForWriting;

    NSTask *task = [[NSTask alloc] init];
    task.currentDirectoryPath = NSHomeDirectory();
    task.standardOutput = readpipe;
    task.standardInput = sendpipe;

    task.launchPath = terppath;

    NSURL *url = [[NSBundle bundleForClass:Tests.class] URLForResource:@"atari8mi_boom"
                                                         withExtension:@"dat"
                                                          subdirectory:nil];

    NSString *supportpath = [[url URLByDeletingLastPathComponent].path stringByAppendingString:@"/"];

    task.arguments = @[@"-z", supportpath];

    Tests * __weak weakSelf = self;

    task.terminationHandler = ^(NSTask *aTask) {
        [aTask.standardOutput fileHandleForReading].readabilityHandler = nil;
        Tests *strongSelf = weakSelf;
        if(strongSelf) {
            [aTask waitUntilExit];
            if (aTask.terminationStatus != 0) {
                NSLog (@"The interpreter unexpectedly terminated.");
            } else {
                NSLog (@"The interpreter exited normally.");
            }
        }
    };

    [[NSNotificationCenter defaultCenter]
     addObserver: self
     selector: @selector(noteProcessReplied:)
     name: NSFileHandleDataAvailableNotification
     object: readfh];

    if (secureBookmark == nil) {
        secureBookmark = [FolderAccess grantAccessToFile:url];
    }

    [task launch];

    [self writeEvent:sendfh.fileDescriptor];
    [readfh waitForDataInBackgroundAndNotify];
}

static BOOL pollMoreData(int fd) {
    fd_set fdset;
    struct timeval tmout = { 0, 0 }; // return immediately
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    return (select(fd + 1, &fdset, NULL, NULL, &tmout) > 0);
}

- (void)noteProcessReplied: (id)sender {
    NSLog(@"noteProcessReplied");
    struct message request;
    struct message reply;
    char buf[GLKBUFSIZE + 1];
    ssize_t n;
    BOOL stop;

    int readfd = readfh.fileDescriptor;
    int sendfd = sendfh.fileDescriptor;

again:

    if (!pollMoreData(readfd))
        return;
    n = read(readfd, &request, sizeof(struct message));
    if (n < (ssize_t)sizeof(struct message))
    {
        if (n < 0)
            NSLog(@"glkctl: could not read message header");
        else
            NSLog(@"glkctl: connection closed");
        return;
    }

    memset(&reply, 0, sizeof reply);

    stop = [self handleRequest: &request reply: &reply buffer: buf];

    if (reply.cmd > NOREPLY)
    {
        write(sendfd, &reply, sizeof(struct message));
    }

    /* if stop, don't read or wait for more data */
    if (stop)
        return;

    if (pollMoreData(readfd))
        goto again;
    else
        [readfh waitForDataInBackgroundAndNotify];
}

- (BOOL)handleRequest:(struct message *)req
                reply:(struct message *)ans
               buffer:(char *)buf {

    ans->cmd = OKAY;
    ans->a1 = 1;
    ans->a2 = 1;

    switch (req->cmd) {
        case HELLO:
            ans->cmd = EVTTEST;
            return NO;
            break;
        case NEXTEVENT:
            return NO;
            break;
        case OKAY:
            NSLog(@"handleRequest: Received an OKAY from process. Expectation fulfilled.");
            [terpExpectation fulfill];
            break;
        default:
            break;

    }
    return NO;
}

- (void)writeEvent:(int)fd {
    struct message reply;
    reply.len = 0;
    reply.cmd = (int)EVTTEST;

    write(fd, &reply, sizeof(struct message));
}

- (void)testScottDraw {

    [self forkInterpreterTask:@"scott"];

    terpExpectation= [[XCTestExpectation alloc] initWithDescription:@"The interpreters's test suite succeeded"];

    [self waitForExpectations:@[terpExpectation] timeout:5];

}

@end
