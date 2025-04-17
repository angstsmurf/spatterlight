//
//  InfocomV6MenuHandler.m
//  Spatterlight
//
//  Created by Administrator on 2025-01-06.
//

#import "GlkController.h"
#import "ZMenu.h"
#import "GlkWindow.h"
#import "GlkTextBufferWindow.h"


#import "InfocomV6MenuHandler.h"

@interface InfocomV6MenuHandler () {
    InfocomV6MenuType hintDepth;
    BOOL hasUpdatedMoveRanges;
    BOOL lastWasEdit;
}
@end

@implementation InfocomV6MenuHandler

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithDelegate:(GlkController *)delegate {
    self = [super init];
    if (self) {
        self.delegate = delegate;
        hintDepth = kV6MenuNone;
    }
    return self;
}

+ (NSString *)makeDefinitionStringVerbose:(NSString *)string {
    unichar c = [string characterAtIndex:string.length - 1];
    if (c == ' ')
        string = [string stringByAppendingString:@"(space)"];
    else if (c == '|')
        string = [string stringByReplacingOccurrencesOfString:@"|" withString:@" (vertical line)"];
    return string;
}

- (void)handleMenuItemOfType:(InfocomV6MenuType)type index:(NSUInteger)index total:(NSUInteger)total text:(char *)buf length:(NSUInteger)len {

    _selectedLine = index;
    hasUpdatedMoveRanges = NO;

    if (type == kV6MenuSelectionChanged) {
        _delegate.zmenu.haveSpokenMenu = YES;
        return;
    } else if (type == kV6MenuExited) {
        _delegate.zmenu.haveSpokenMenu = NO;
        _delegate.infocomV6MenuHandler = nil;
        return;
    }

    if (_menuItems == nil) {
        _menuItems = [NSMutableArray new];
    }

    NSString *str = @"";
    if (len > 0) {
        unichar cstring[len];
        for (NSUInteger i = 0; i < len; i++) {
            cstring[i] = (unichar)buf[i];
        }
        str = [NSString stringWithCharacters:cstring length:len];
        str = [str stringByTrimmingCharactersInSet:[NSCharacterSet illegalCharacterSet]];
        if (type == kV6MenuTypeDefine) {
            str = [InfocomV6MenuHandler makeDefinitionStringVerbose:str];
        }
    }

    if (type == kV6MenuTitle) {
        _title = str;
        hintDepth = (InfocomV6MenuType)total;
    } else {
        hintDepth = type;
        if (total > 0) {
            _numberOfItems = total;
        } else {
            _numberOfItems = _menuItems.count + 1;
        }
        if (str.length) {
            if (type == kV6MenuTypeHint &&
                [str isEqualToString:@"[No more hints.]\n"]) {
                str = [NSString stringWithFormat:@"%@\n[No more hints.]", _menuItems.lastObject];
                [_menuItems removeLastObject];
            }
            [_menuItems addObject:str];
        } else if (type == kV6MenuCurrentItemChanged) {
            // kV6MenuCurrentItemChanged means the player typed a character in a define menu.
            // We overload the total parameter for the typed character
            unichar c = (unichar)total;
            // 0 means delete
            str = _menuItems[_selectedLine];
            str = [str stringByReplacingOccurrencesOfString:@" (space)" withString:@" "];
            str = [str stringByReplacingOccurrencesOfString:@" (vertical line)" withString:@"|"];
            if (c == 0) {
                NSString *deleted = [str substringFromIndex:str.length - 1];
                [_delegate speakString:[NSString stringWithFormat:@"Delete %@", deleted]];
                str = [str substringToIndex:str.length - 1];
            } else {
                NSString *added = [NSString stringWithFormat:@"%c", c];
                [_delegate speakString:added];
                str = [str stringByAppendingString:added];
            }

            [_menuItems replaceObjectAtIndex:_selectedLine withObject:[InfocomV6MenuHandler makeDefinitionStringVerbose:str]];
            lastWasEdit = YES;
        }
    }
}

- (NSString *)constructMenuInstructionString {
    NSString *instructionString = @"";

    if (hintDepth == kV6MenuTypeShogun) {
        return @"Use the up and down arrow keys or type the first letters to change selection. ";
    }

    if (hintDepth == kV6MenuTypeDefine) {
        return @"Use the up and down arrow keys or press a function key, then type your definition. Vertical line means enter. ";
    }

    if (hintDepth != kV6MenuTypeHint) {
        instructionString = @"N for next item. P for previous item. ";
    }

    if (hintDepth != kV6MenuTypeTopic) {
        instructionString = [instructionString stringByAppendingString:@"M for hint menu. "];
    }

    if (hintDepth == kV6MenuTypeHint) {
        if (_selectedLine < _numberOfItems - 1 || _menuItems.count == 0) {
            instructionString = [instructionString stringByAppendingString:@"Return for a hint. "];
        }
    } else {
        instructionString = [instructionString stringByAppendingString:@"Return for hints. "];
    }

    instructionString = [instructionString stringByAppendingString:@"Q to resume story. "];

    if (hintDepth == kV6MenuTypeHint && _numberOfItems > 1 && _menuItems.count > 1) {
        instructionString = [instructionString stringByAppendingString:@"You can review the hints by stepping through moves."];
    }

    return instructionString;
}

- (void)updateMoveRanges:(GlkTextBufferWindow *)window {
    if (hintDepth == kV6MenuTypeHint && !hasUpdatedMoveRanges) {
        hasUpdatedMoveRanges = YES;
        [window movesRangesFromV6Menu:_menuItems];
    }
}

- (NSString *)menuLineStringWithTitle:(BOOL)useTitle index:(BOOL)useIndex total:(BOOL)useTotal instructions:(BOOL)useInstructions {

    _delegate.zmenu.haveSpokenMenu = YES;
    NSString *menuLineString = @"";

    if (_menuItems.count) {
        if (_selectedLine >= _menuItems.count) {
            _selectedLine = _menuItems.count - 1;
        }
        menuLineString = [menuLineString stringByAppendingString:_menuItems[_selectedLine]];
    }

    NSString *menuItemString = @"Menu item";
    NSString *weAreInAMenuString = @"We are in a menu";

    if (hintDepth == kV6MenuTypeHint) {
        menuItemString = @"Hint";
        weAreInAMenuString = @"Showing hints for";
    }

    if (useIndex) {
        NSString *indexString;

        if (hintDepth == kV6MenuTypeHint && _menuItems.count == 0) {
            indexString = [NSString stringWithFormat:@"%ld hint%@ available", _numberOfItems, _numberOfItems == 1 ? @"" : @"s"];
        } else {
            if (_selectedLine >= _numberOfItems) {
                _selectedLine = _numberOfItems - 1;
            }
            indexString = [NSString stringWithFormat:@".\n%@ %ld", menuItemString, _selectedLine + 1];
            if (useTotal) {
                indexString = [indexString stringByAppendingString:
                               [NSString stringWithFormat:@" of %ld", _numberOfItems]];
            }
        }

        indexString = [indexString stringByAppendingString:@".\n"];
        menuLineString = [menuLineString stringByAppendingString:indexString];
    }

    if (useInstructions) {
        menuLineString = [menuLineString stringByAppendingString:[self constructMenuInstructionString]];
    }

    if (useTitle) {
        NSString *titleString = _title;
        if (titleString.length) {
            titleString = [NSString stringWithFormat:@"%@, \"%@.\"\n", weAreInAMenuString, titleString];
        } else {
            titleString = @"We are in a menu.\n";
        }
        menuLineString = [titleString stringByAppendingString:menuLineString];
    }

    return menuLineString;
}

@end
