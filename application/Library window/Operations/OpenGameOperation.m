//
//  OpenGameOperation.m
//  Spatterlight
//
//  Created by Administrator on 2024-11-24.
//

#import "GlkController.h"
#import "Game.h"
#import "OpenGameOperation.h"

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

        /*
         don't run completionHandler if data is null or zero bytes
         */
        NSData *fileData = [NSData dataWithContentsOfURL:newURL];
        if (fileData.length == 0) {
            strongSelf.state = kFinished;
            return;
        }

        /*
         if there is a custom completionHandler defined,
         pass the result gotten in coordinateReadingItemAtURL's completionHandler to the
         custom completionHandler
         */
        if (_completionHandler) {
            _completionHandler(fileData, newURL);
        }

        /*
         set the operation state to finished once
         the download task is completed or have error
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
