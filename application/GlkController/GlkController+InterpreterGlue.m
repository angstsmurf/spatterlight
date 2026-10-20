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
#import "CommandScriptHandler.h"
#import "SPAccessibility.h"

#include "protocol.h"
#include "messagenames.h"
#include "glkimp.h"

#import "DebugLog.h"
#import "NSAlert+Spatterlight.h"

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

- (void)openLogFile {
    NSString *logDirectory = [[NSUserDefaults standardUserDefaults] stringForKey:@"InterpreterLogDirectory"];
    if (!logDirectory || logDirectory.length == 0) {
        // Default to Desktop if no directory specified
        logDirectory = [NSHomeDirectory() stringByAppendingPathComponent:@"Desktop"];
    }
    
    // Create log filename with timestamp
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    formatter.dateFormat = @"yyyy-MM-dd HH.mm.ss";
    NSString *timestamp = [formatter stringFromDate:[NSDate date]];
    
    NSString *gameName = [self.gamefile.lastPathComponent stringByDeletingPathExtension];
    NSString *logFilename = [NSString stringWithFormat:@"Spatterlight-%@-%@.log", gameName, timestamp];
    NSString *logPath = [logDirectory stringByAppendingPathComponent:logFilename];
    
    // Create/open the log file
    [[NSFileManager defaultManager] createFileAtPath:logPath contents:nil attributes:nil];
    self.interpreterLogFileHandle = [NSFileHandle fileHandleForWritingAtPath:logPath];
    
    if (self.interpreterLogFileHandle) {
        NSLog(@"Logging interpreter output to: %@", logPath);
        
        // Write header to log file
        NSString *header = [NSString stringWithFormat:@"=== Spatterlight Interpreter Log ===\nGame: %@\nInterpreter: %@\n\n",
                            self.gamefile.lastPathComponent, self.terpname];
        [self.interpreterLogFileHandle writeData:[header dataUsingEncoding:NSUTF8StringEncoding]];
    } else {
        NSLog(@"Warning: Could not create interpreter log file at %@", logPath);
    }
}

- (void)closeLogFile {
    // Close and cleanup log file handle if logging was enabled
    if (self.interpreterLogFileHandle) {
        NSString *footer = @"\n=== Session ended ===\n";
        [self.interpreterLogFileHandle writeData:[footer dataUsingEncoding:NSUTF8StringEncoding]];
        [self.interpreterLogFileHandle closeFile];
        self.interpreterLogFileHandle = nil;
        NSLog(@"Closed interpreter log file");
    }
}

- (void)noteTaskDidTerminate:(id)sender {
    NSLog(@"noteTaskDidTerminate:");
    if (windowClosedAlready)
        return;

    // A termination notice for an interpreter we have already replaced (or
    // dropped): reset: clears the handler, but a terp that exits on its own
    // right after asking for a RESET may have queued this block already.
    // Acting on it would wait for the new interpreter to exit, which blocks
    // the main thread for as long as the restarted game runs.
    if (sender != task)
        return;

    dead = YES;
    terpHasStopped = YES;
    restartingAlready = NO;

    if (timer) {
        [timer invalidate];
        timer = nil;
    }

    [self flushDisplay];
    [task waitUntilExit];

    if (task.terminationStatus != 0) {
        NSString *info = [NSString stringWithFormat:NSLocalizedString(@"Error code: %@.", nil), signalToName(task)];
        if (self.pendingErrorMessage && self.errorTimeStamp.timeIntervalSinceNow > -3)
            info = self.pendingErrorMessage;
        self.pendingErrorMessage = nil;
        self.mustBeQuiet = YES;
        NSAlert *alert = [NSAlert alertWithMessage:NSLocalizedString(@"The game has unexpectedly terminated.", nil)
                                              info:info
                                           buttons:@[ NSLocalizedString(@"Oops", nil) ]];
        [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode) {}];
    } else {
        NotificationBezel *bezel = [[NotificationBezel alloc] initWithScreen:self.window.screen];
        [bezel showGameOver];

        runOnMainAfterDelay(1, ^{
            SPAnnounce([NSString stringWithFormat:@"%@ has finished.", self.game.metadata.title],
                       NSAccessibilityPriorityHigh, nil);
        });
    }

    [self.journeyMenuHandler deleteAllMenus];

    for (GlkWindow *win in self.gwindows.allValues)
        [win terpDidStop];

    [self closeLogFile];

    // Post notification that command script has completed
    [[NSNotificationCenter defaultCenter] postNotificationName:GlkCommandScriptDidCompleteNotification
                                                        object:self
                                                      userInfo:nil];

    self.window.title = [self.window.title stringByAppendingString:NSLocalizedString(@" (finished)", nil)];
    [self performScroll];
    task = nil;
    libcontroller.gameTableDirty = YES;
    [libcontroller updateTableViews];

    // We autosave the UI but delete the terp autosave files
    if (!restartingAlready)
        [self autoSaveOnExit];

    [self deleteAutosaveFilesKeepingLateGUI:YES];
}

- (void)queueEvent:(GlkEvent *)gevent {
    GlkEvent *redrawEvent = nil;

    // Once the interpreter has stopped, nothing will ever drain the queue, so
    // an input event added here is swallowed silently and the window appears to
    // lock up. Windows should already be refusing input at this point (see
    // terpDidStop), so this is a backstop for any path that slips through.
    BOOL isInputEvent = (gevent.type == EVTKEY || gevent.type == EVTLINE ||
                         gevent.type == EVTHYPER || gevent.type == EVTMOUSE);
    if (terpHasStopped && isInputEvent)
        return;

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

    if (isInputEvent)
        self.shouldScrollOnCharEvent = YES;

    if (waitforevent && !waitforfilename) {
        [gevent writeEvent:sendfh.fileDescriptor];
        waitforevent = NO;
        [readfh waitForDataInBackgroundAndNotify];
    } else {
        [self.queue addObject:gevent];
    }
    if (redrawEvent)
        [self queueEvent:redrawEvent];
}

// Write all of len bytes, retrying on short writes and EINTR.
static BOOL writeAll(int fd, const void *data, size_t len) {
    const char *p = data;
    while (len > 0) {
        ssize_t t = write(fd, p, len);
        if (t < 0) {
            if (errno == EINTR)
                continue;
            return NO;
        }
        p += t;
        len -= (size_t)t;
    }
    return YES;
}

- (void)writeToInterpreterLog:(NSString *)string {
    @try {
        [self.interpreterLogFileHandle writeData:[string dataUsingEncoding:NSUTF8StringEncoding]];
    } @catch (NSException *exception) {
        NSLog(@"noteDataAvailable: Could not write to log file: %@", exception);
    }
}

- (void)logRequestHeader:(struct message *)request {
    // The header comes straight off the pipe: only look up a name the
    // table has, and log anything else by number.
    const char *name = (request->cmd >= 0 && (size_t)request->cmd < msgnames_count) ? msgnames[request->cmd] : NULL;
    NSString *cmdName = name ? @(name) : [NSString stringWithFormat:@"%d", request->cmd];
    [self writeToInterpreterLog:[NSString stringWithFormat:@"[MSG] cmd=%@ a1=%d a2=%d a3=%d len=%zu\n",
                                 cmdName, request->a1, request->a2, request->a3, request->len]];
}

- (void)logRequestBody:(struct message *)request buffer:(const char *)buf {
    // PRINT bodies are UTF-16 text; log them as such
    if (request->cmd == PRINT && request->len) {
        NSString *text = [[NSString alloc] initWithBytes:buf length:request->len encoding:NSUTF16LittleEndianStringEncoding];
        if (text)
            [self writeToInterpreterLog:[NSString stringWithFormat:@"[TEXT] %@\n", text]];
        else
            [self writeToInterpreterLog:[NSString stringWithFormat:@"[DATA] %zu bytes\n", request->len]];
    }
}

// Read one message (header and body) from the interpreter, handle it and
// send any reply. Returns NO if nothing more should be read: the pipe
// failed, or the handler asked us to stop.
- (BOOL)readAndHandleMessageFrom:(int)readfd replyTo:(int)sendfd {
    struct message request;
    struct message reply;
    char minibuf[GLKBUFSIZE + 1];
    char *maxibuf = NULL;
    char *buf = minibuf;
    ssize_t n, t;

    n = read(readfd, &request, sizeof(struct message));
    if (n < (ssize_t)sizeof(struct message))
    {
        if (n < 0) {
            NSLog(@"glkctl: could not read message header");
        } else {
            NSLog(@"glkctl: connection closed");
        }
        return NO;
    }

    if (self.interpreterLogFileHandle)
        [self logRequestHeader:&request];

    /* this should only happen when sending resources */
    if (request.len > GLKBUFSIZE)
    {
        // + 1: handlers may NUL-terminate the payload (see protocol.h)
        maxibuf = malloc(request.len + 1);
        if (!maxibuf)
        {
            NSLog(@"glkctl: out of memory for message (%zu bytes)", request.len);
            return NO;
        }
        buf = maxibuf;
    }

    n = 0;
    while (n < (ssize_t)request.len)
    {
        t = read(readfd, buf + n, request.len - (size_t)n);
        if (t <= 0)
        {
            NSLog(@"glkctl: could not read message body");
            free(maxibuf);
            return NO;
        }
        n += t;
    }

    if (self.interpreterLogFileHandle)
        [self logRequestBody:&request buffer:buf];

    memset(&reply, 0, sizeof reply);

    BOOL stop = [self handleRequest: &request reply: &reply buffer: buf];

    if (reply.cmd > NOREPLY)
    {
        if (!writeAll(sendfd, &reply, sizeof(struct message)) ||
            (reply.len && !writeAll(sendfd, buf, reply.len)))
            NSLog(@"glkctl: could not write reply: %s", strerror(errno));

        if (self.interpreterLogFileHandle)
            [self writeToInterpreterLog:[NSString stringWithFormat:@"[REPLY] cmd=%u a1=%d a2=%d a3=%d len=%zu\n",
                                         reply.cmd, reply.a1, reply.a2, reply.a3, reply.len]];
    }

    free(maxibuf);

    /* if stop, don't read or wait for more data */
    return !stop;
}

- (void)noteDataAvailable: (id)sender {
    int readfd = readfh.fileDescriptor;
    int sendfd = sendfh.fileDescriptor;

    if (!waitForData(readfd, 0))
        return;

    do {
        if (![self readAndHandleMessageFrom:readfd replyTo:sendfd])
            return;
    } while (waitForData(readfd, 0));

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
