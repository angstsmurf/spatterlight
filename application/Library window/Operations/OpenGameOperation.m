//
//  OpenGameOperation.m
//  Spatterlight
//
//  Created by Administrator on 2024-11-24.
//

#import "GlkController.h"
#import "Game.h"
#import "OpenGameOperation.h"
#import "FolderAccess.h"

typedef NS_ENUM(NSUInteger, OperationState) {
    kReady,
    kExecuting,
    kFinished
};


@interface OpenGameOperation ()

@property NSFileCoordinator *coordinator;
@property NSURL *fileURL;

@property (copy)void (^completionHandler)(NSData * _Nullable __strong, NSURL * _Nullable __strong);

@property (atomic) OperationState state;

@end


@implementation OpenGameOperation


// default state is ready (when the operation is created)

@synthesize state = _state;

- (void)setState:(OperationState)state {
    @synchronized(self) {
        if (_state != state) {
            [self willChangeValueForKey:@"isExecuting"];
            [self willChangeValueForKey:@"isFinished"];
            _state = state;
            [self didChangeValueForKey: @"isExecuting"];
            [self didChangeValueForKey: @"isFinished"];
        }
    }
}

- (OperationState)state {
    @synchronized(self) {
        return _state;
    }
}

- (BOOL)isReady { return (self.state == kReady); }
- (BOOL)isExecuting { return (self.state == kExecuting); }
- (BOOL)isFinished { return (self.state == kFinished); }

- (BOOL)isAsynchronous {
    return YES;
}

+ (NSSet<NSString *> *)keyPathsForValuesAffectingValueForKey:(NSString *)key {
    NSSet *keyPaths = [super keyPathsForValuesAffectingValueForKey:key];
    if ([@[@"isReady", @"isFinished", @"isExecuting"] containsObject:key]) {
        return [keyPaths setByAddingObject:@"state"];
    }
    return keyPaths;
}

- (instancetype)initWithURL:(NSURL *)gameFileURL completionHandler:(nullable void (^)(NSData * _Nullable,  NSURL * _Nullable))completionHandler {

    self = [super init];

    if (self) {
        _state = kReady;
        _fileURL = gameFileURL;
        _completionHandler = completionHandler;
    }

    return self;
}

- (void)start {
    /*
     if the operation or queue got cancelled even
     before the operation has started, set the
     operation state to finished and return
     */
    if (self.cancelled) {
        self.state = kFinished;
        return;
    }

    // set the state to executing
    self.state = kExecuting;

    // start the downloading

    OpenGameOperation __weak *weakSelf = self;
    // use weak self to prevent retain cycle

    NSError __block *blockerror = nil;

    NSFileCoordinator *coordinator = [NSFileCoordinator new];

    [coordinator coordinateReadingItemAtURL:_fileURL options:NSFileCoordinatorReadingWithoutChanges error:&blockerror byAccessor:^(NSURL * _Nonnull newURL) {

        OpenGameOperation *strongSelf = weakSelf;
        if (!strongSelf)
            return;

        /*
         don't run completionHandler if cancelled
         */
        if (strongSelf.cancelled) {
            strongSelf.state = kFinished;
            return;
        }

        NSError *fileAccessError = nil;
        NSData *fileData = [NSData dataWithContentsOfURL:newURL options:0 error:&fileAccessError];
        if (fileAccessError != nil) {
            NSLog(@"Error: %@", fileAccessError);
        }

        /*
         if there is a custom completionHandler defined,
         pass the result gotten in coordinateReadingItemAtURL's completionHandler to the
         custom completionHandler
         */
        if (strongSelf.completionHandler) {
            BOOL fileNotFound = YES;
            /*
             If there was no file access error, there is no point in asking for permission.
             */
            if (fileAccessError != nil) {
                /*
                 Also no point in asking for permission if the file isn't there anymore.
                 Cocoa error domain code 260 means "The file couldn’t be opened because there is no such file."
                 */
                fileNotFound = fileAccessError.domain == NSCocoaErrorDomain && fileAccessError.code == 260;
                if (!fileNotFound) {
                    NSURL *secureURL = [FolderAccess forceRestoreURL:newURL];
                    if (secureURL) {
                        fileAccessError = nil;
                        fileData = [NSData dataWithContentsOfURL:secureURL options:NSDataReadingMappedIfSafe
                                                           error:&fileAccessError];
                        newURL = secureURL;
                    }

                    if (fileAccessError != nil) {
                        dispatch_async(dispatch_get_main_queue(), ^{
                            [FolderAccess forceAccessDialogToURL:newURL andThenRunBlock:^{
                                NSError *innerError = nil;
                                NSData *blockdata = [NSData dataWithContentsOfURL:newURL options:NSDataReadingMappedIfSafe error:&innerError];
                                if (innerError != nil)
                                    NSLog(@"Error: %@", innerError);
                                strongSelf.completionHandler(blockdata, newURL);
                            }];
                        });
                    }
                }
            }
            /*
             Run the completionHandler here if we didn't ask for file permission above.
             */
            if (fileNotFound || fileAccessError == nil) {
                strongSelf.completionHandler(fileData, newURL);
            }
        }
        /*
         set the operation state to finished once
         the file read operation is completed or have error
         */
        strongSelf.state = kFinished;
    }];

    if (blockerror != nil)
        NSLog(@"Error: %@", blockerror);
}

-(void)cancel {
    BOOL wasExecuting = (self.state == kExecuting);
    [super cancel];

    // cancel the downloading
    if (wasExecuting)
        [self.coordinator cancel];
}

@end
