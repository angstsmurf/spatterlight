//
//  GlkController+InterpreterGlue.m
//  Spatterlight
//

#import "GlkController_Private.h"
#import "GlkController+Autorestore.h"
#import "GlkController+GlkRequests.h"

#import "GlkEvent.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"
#import "GlkTextGridWindow.h"
#import "GlkGraphicsWindow.h"
#import "Game.h"
#import "Theme.h"
#import "Metadata.h"
#import "Preferences.h"
#import "NotificationBezel.h"
#import "SoundHandler.h"
#import "ImageHandler.h"
#import "FolderAccess.h"
#import "TableViewController.h"
#import "TableViewController+TableDelegate.h"
#import "JourneyMenuHandler.h"
#import "GlkStyle.h"

#include "protocol.h"
#include "glkimp.h"

#ifndef DEBUG
#define NSLog(...)
#endif

@implementation GlkController (InterpreterGlue)

static NSString *signalToName(NSTask *task) {
    switch (task.terminationStatus) {
        case 1:
            return @"sighup";
        case 2:
            return @"sigint";
        case 3:
            return @"sigquit";
        case 4:
            return @"sigill";
        case 5:
            return @"sigtrap";
        case 6:
            return @"sigabrt";
        case 8:
            return @"sigfpe";
        case 9:
            return @"sigkill";
        case 10:
            return @"sigbus";
        case 11:
            return @"sigsegv";
        case 13:
            return @"sigpipe";
        case 15:
            return @"sigterm";
        default:
            return [NSString stringWithFormat:@"%d", task.terminationStatus];
    }
}

static BOOL pollMoreData(int fd) {
    fd_set fdset;
    struct timeval tmout = { 0, 0 }; // return immediately
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    return (select(fd + 1, &fdset, NULL, NULL, &tmout) > 0);
}

- (void)noteTaskDidTerminate:(id)sender {
    if (windowClosedAlready)
        return;

    dead = YES;
    restartingAlready = NO;

    if (timer) {
        [timer invalidate];
        timer = nil;
    }

    [self flushDisplay];
    [task waitUntilExit];

    if (task.terminationStatus != 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = NSLocalizedString(@"The game has unexpectedly terminated.", nil);
        alert.informativeText = [NSString stringWithFormat:NSLocalizedString(@"Error code: %@.", nil), signalToName(task)];
        if (self.pendingErrorMessage && self.errorTimeStamp.timeIntervalSinceNow > -3)
            alert.informativeText = self.pendingErrorMessage;
        self.pendingErrorMessage = nil;
        self.mustBeQuiet = YES;
        [alert addButtonWithTitle:NSLocalizedString(@"Oops", nil)];
        [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode) {}];
    } else {
        NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:self.window.screen];
        [bezel showGameOver];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            NSAccessibilityPostNotificationWithUserInfo(
                                                        NSApp.mainWindow,
                                                        NSAccessibilityAnnouncementRequestedNotification,
                                                        @{NSAccessibilityPriorityKey: @(NSAccessibilityPriorityHigh),
                                                          NSAccessibilityAnnouncementKey: [NSString stringWithFormat:@"%@ has finished.", self.game.metadata.title]
                                                        });
        });
    }

    [self.journeyMenuHandler deleteAllJourneyMenus];

    for (GlkWindow *win in self.gwindows.allValues)
        [win terpDidStop];

    self.window.title = [self.window.title stringByAppendingString:NSLocalizedString(@" (finished)", nil)];
    [self performScroll];
    task = nil;
    libcontroller.gameTableDirty = YES;
    [libcontroller updateTableViews];

    // We autosave the UI but delete the terp autosave files
    if (!restartingAlready)
        [self autoSaveOnExit];

    [self deleteFiles:@[ [NSURL fileURLWithPath:self.autosaveFileTerp isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave.glksave"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-GUI.plist"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.glksave"] isDirectory:NO],
                         [NSURL fileURLWithPath:[self.appSupportDir stringByAppendingPathComponent:@"autosave-tmp.plist"] isDirectory:NO] ]];
}

- (void)queueEvent:(GlkEvent *)gevent {
    GlkEvent *redrawEvent = nil;
    if (gevent.type == EVTARRANGE) {
        Theme *theme = self.theme;
        NSDictionary *newArrangeValues = @{
            @"width" : @(gevent.val1),
            @"height" : @(gevent.val2),
            @"bufferMargin" : @(theme.bufferMarginX),
            @"gridMargin" : @(theme.gridMarginX),
            @"charWidth" : @(theme.cellWidth),
            @"lineHeight" : @(theme.cellHeight),
            @"leading" : @(((NSParagraphStyle *)(theme.gridNormal.attributeDict)[NSParagraphStyleAttributeName]).lineSpacing)
        };

        if (!gevent.forced && [lastArrangeValues isEqualToDictionary:newArrangeValues]) {
            return;
        }

        lastArrangeValues = newArrangeValues;
        // Some Inform 7 games only resize graphics on evtype_Redraw
        // so we send a redraw event after every resize event
        if (self.eventcount > 2)
            redrawEvent = [[GlkEvent alloc] initRedrawEvent];
    }

    if (gevent.type == EVTKEY || gevent.type == EVTLINE || gevent.type == EVTHYPER || gevent.type == EVTMOUSE)
        self.shouldScrollOnCharEvent = YES;

    if (waitforfilename) {
        [self.queue addObject:gevent];
    } else if (waitforevent) {
        [gevent writeEvent:sendfh.fileDescriptor];
        waitforevent = NO;
        [readfh waitForDataInBackgroundAndNotify];
    } else {
        [self.queue addObject:gevent];
    }
    if (redrawEvent)
        [self queueEvent:redrawEvent];
}

- (void)noteDataAvailable: (id)sender {
    struct message request;
    struct message reply;
    char minibuf[GLKBUFSIZE + 1];
    char *maxibuf;
    char *buf;
    ssize_t n, t;
    BOOL stop;

    int readfd = readfh.fileDescriptor;
    int sendfd = sendfh.fileDescriptor;

again:

    buf = minibuf;
    maxibuf = NULL;

    if (!pollMoreData(readfd))
        return;
    n = read(readfd, &request, sizeof(struct message));
    if (n < (ssize_t)sizeof(struct message))
    {
        if (n < 0) {
            NSLog(@"glkctl: could not read message header");
        } else {
            NSLog(@"glkctl: connection closed");
        }
        return;
    }

    /* this should only happen when sending resources */
    if (request.len > GLKBUFSIZE)
    {
        maxibuf = malloc(request.len);
        if (!maxibuf)
        {
            NSLog(@"glkctl: out of memory for message (%zu bytes)", request.len);
            return;
        }
        buf = maxibuf;
    }

    if (request.len)
    {
        n = 0;
        while (n < (ssize_t)request.len)
        {
            t = read(readfd, buf + n, request.len - (size_t)n);
            if (t <= 0)
            {
                NSLog(@"glkctl: could not read message body");
                if (maxibuf)
                    free(maxibuf);
                return;
            }
            n += t;
        }
    }

    memset(&reply, 0, sizeof reply);

    stop = [self handleRequest: &request reply: &reply buffer: buf];

    if (reply.cmd > NOREPLY)
    {
        write(sendfd, &reply, sizeof(struct message));
        if (reply.len)
            write(sendfd, buf, reply.len);
    }

    if (maxibuf)
        free(maxibuf);

    /* if stop, don't read or wait for more data */
    if (stop)
        return;

    if (pollMoreData(readfd))
        goto again;
    else
        [readfh waitForDataInBackgroundAndNotify];
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    NSSet *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];
    NSSet *refreshedObjects = (notification.userInfo)[NSRefreshedObjectsKey];
    if (!updatedObjects)
        updatedObjects = [NSSet new];
    updatedObjects = [updatedObjects setByAddingObjectsFromSet:refreshedObjects];

    if ([updatedObjects containsObject:self.game.metadata])
    {
        if (![self.game.metadata.title isEqualToString:self.gamefile.lastPathComponent]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                self.window.title = self.game.metadata.title;
            });
        }
    }
}

@end
