//
//  JourneyMenuHandler+Coding.m
//  Spatterlight
//
//  The archive keys predate the property names and must not change:
//  autosaves have them.
//

#import "GlkTextGridWindow.h"
#import "GlkTextBufferWindow.h"
#import "JourneyMenuHandler_Private.h"
#import "JourneyMenuItem.h"

@implementation JourneyMenuHandler (Coding)

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
        NSSet *itemArray = [NSSet setWithObjects:[NSMutableArray class], [JourneyMenuItem class], nil];
        NSSet *itemDictionary = [NSSet setWithObjects:[NSMutableDictionary class], [NSNumber class], [JourneyMenuItem class], nil];
        NSSet *menuItemArray = [NSSet setWithObjects:[NSMutableArray class], [NSMenuItem class], nil];

        self.dialogMenuItems = [[decoder decodeObjectOfClasses:itemArray forKey:@"journeyDialogMenuItems"] mutableCopy];
        self.verbMenuItems = [[decoder decodeObjectOfClasses:itemDictionary forKey:@"journeyVerbMenuItems"] mutableCopy];
        self.partyMenuItems = [[decoder decodeObjectOfClasses:itemArray forKey:@"journeyPartyMenuItems"] mutableCopy];
        self.lastPartyMenuItems = [[decoder decodeObjectOfClasses:itemArray forKey:@"journeyLastPartyMenuItems"] mutableCopy];
        self.glueStrings = [[decoder decodeObjectOfClasses:itemArray forKey:@"journeyGlueStrings"] mutableCopy];
        self.capturedMembersMenu = [[decoder decodeObjectOfClasses:menuItemArray forKey:@"capturedMembersMenu"] mutableCopy];

        // Older autosaves have no tag counter. Start above every restored
        // verb tag, so that new verbs don't replace restored ones.
        if ([decoder containsValueForKey:@"tagCounter"]) {
            self.tagCounter = [decoder decodeIntegerForKey:@"tagCounter"];
        } else {
            NSInteger tagCounter = 0;
            for (NSNumber *tag in self.verbMenuItems)
                tagCounter = MAX(tagCounter, tag.integerValue + 1);
            self.tagCounter = tagCounter;
        }

        self.gridTextWinName = [decoder decodeIntegerForKey:@"gridTextWinName"];
        self.bufferTextWinName = [decoder decodeIntegerForKey:@"bufferTextWinName"];

        self.dialogState = [decoder decodeBoolForKey:@"shouldShowDialog"] ? JourneyDialogStateRestored : JourneyDialogStateIdle;
        self.dialogIncludesLastMove = [decoder decodeBoolForKey:@"lastDialogAddedMove"];
        self.restoredDialogIncludesLastMove = self.dialogIncludesLastMove;
        self.dialogType = (JourneyDialogType)[decoder decodeIntegerForKey:@"storedDialogType"];
        self.dialogText = [decoder decodeObjectOfClass:[NSString class] forKey:@"storedDialogText"];

        self.startNewDialogMenu = [decoder decodeBoolForKey:@"shouldStartNewJourneyDialogMenu"];
        self.startNewPartyMenu = YES;
        self.startNewMembersMenu = YES;
        self.dialogClosedTimestamp = [NSDate distantPast];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:self.dialogMenuItems forKey:@"journeyDialogMenuItems"];

    [encoder encodeObject:self.verbMenuItems forKey:@"journeyVerbMenuItems"];
    [encoder encodeObject:self.partyMenuItems forKey:@"journeyPartyMenuItems"];
    [encoder encodeObject:self.lastPartyMenuItems forKey:@"journeyLastPartyMenuItems"];

    [encoder encodeObject:self.glueStrings forKey:@"journeyGlueStrings"];
    [encoder encodeObject:self.capturedMembersMenu forKey:@"capturedMembersMenu"];

    [encoder encodeInteger:self.textGridWindow.name forKey:@"gridTextWinName"];
    [encoder encodeInteger:self.textBufferWindow.name forKey:@"bufferTextWinName"];

    [encoder encodeBool:self.startNewDialogMenu forKey:@"shouldStartNewJourneyDialogMenu"];
    // A showing dialog is shown again after an autorestore.
    [encoder encodeBool:(self.dialogState != JourneyDialogStateIdle) forKey:@"shouldShowDialog"];
    [encoder encodeBool:self.dialogIncludesLastMove forKey:@"lastDialogAddedMove"];
    [encoder encodeInteger:self.dialogType forKey:@"storedDialogType"];
    [encoder encodeObject:self.dialogText forKey:@"storedDialogText"];
    [encoder encodeInteger:self.tagCounter forKey:@"tagCounter"];
}

@end
