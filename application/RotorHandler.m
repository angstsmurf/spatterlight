//
//  RotorHandler.m
//  Spatterlight
//
//  Created by Petter Sjölund on 2021-04-05.
//

#import "RotorHandler.h"
#import "GlkWindow.h"
#import "GlkGraphicsWindow.h"
#import "GlkTextGridWindow.h"
#import "GridTextView.h"
#import "GlkTextBufferWindow.h"
#import "BufferTextView.h"
#import "GlkController.h"
#import "GlkController+Speech.h"
#import "MarginContainer.h"
#import "MarginImage.h"
#import "MyAttachmentCell.h"
#import "Theme.h"
#import "SubImage.h"

#include "glk.h"

#import "DebugLog.h"


// Moves one step from current in the given direction. With no current
// item, starts at the first or last one. Returns NSNotFound past either end.
static NSUInteger SPRotorStep(NSUInteger current, NSUInteger count, NSAccessibilityCustomRotorSearchDirection direction) {
    BOOL next = (direction == NSAccessibilityCustomRotorSearchDirectionNext);
    BOOL previous = (direction == NSAccessibilityCustomRotorSearchDirectionPrevious);
    if (count == 0)
        return NSNotFound;
    if (current == NSNotFound) {
        if (next)
            return 0;
        if (previous)
            return count - 1;
        return NSNotFound;
    }
    if (previous)
        return current == 0 ? NSNotFound : current - 1;
    if (next)
        return current + 1 >= count ? NSNotFound : current + 1;
    return current;
}

// Trimmed from rotor item labels
static NSCharacterSet *SPRotorLabelTrimSet(void) {
    static NSCharacterSet *charset;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        charset = [NSCharacterSet characterSetWithCharactersInString:@"\u00A0 >\n_"];
    });
    return charset;
}

// A result for a range of a text view's text, clamped to the text.
static NSAccessibilityCustomRotorItemResult *SPTextRangeResult(NSTextView *textView, NSRange range) {
    NSAccessibilityCustomRotorItemResult *result = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:textView];
    result.targetRange = NSIntersectionRange(NSMakeRange(0, textView.string.length), range);
    return result;
}

@implementation RotorHandler

// The principle: Generate a list on-the-fly of all available elements, find the item that matches the parameters and return it as a NSAccessibilityCustomRotorItemResult with correct metadata.

// The system repeatedly calls the resultForSearchParameters method of the current rotor with either a) the result of the last call or, if this is the first call or the last result was nil, b) empty result.
// It keeps calling this until the results are equal to the first result, i.e. we have looped back to the start

- (NSAccessibilityCustomRotorItemResult *)rotor:(NSAccessibilityCustomRotor *)rotor
                      resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters {

    if (rotor.type == NSAccessibilityCustomRotorTypeAny) {
        NSAccessibilityCustomRotorItemResult *currentItemResult = searchParameters.currentItem;
        return [self textSearchResultForString:searchParameters.filterString
                                     fromRange:currentItemResult.targetRange
                                     direction:searchParameters.searchDirection];
    } else if ([rotor.label isEqualToString:NSLocalizedString(@"Command history", nil)]) {
        return [self commandHistoryRotor:rotor resultForSearchParameters:searchParameters];
    } else if ([rotor.label isEqualToString:NSLocalizedString(@"Game windows", nil)]) {
        return [self glkWindowRotor:rotor resultForSearchParameters:searchParameters];
    } else if (rotor.type == NSAccessibilityCustomRotorTypeImage && _glkctl.theme.vOSpeakImages != kVOImageNone) {
        return [self imagesRotor:rotor resultForSearchParameters:searchParameters];
    } else if (rotor.type == NSAccessibilityCustomRotorTypeLink) {
        return [self linksRotor:rotor resultForSearchParameters:searchParameters];
    }

    return nil;
}

- (NSAccessibilityCustomRotorItemResult *)textSearchResultForString:(NSString *)searchString fromRange:(NSRange)fromRange direction:(NSAccessibilityCustomRotorSearchDirection)direction {

    NSAccessibilityCustomRotorItemResult *searchResult = nil;

    NSTextView *bestMatch = nil;
    NSRange bestMatchRange = NSMakeRange(NSNotFound, 0);

    if (searchString.length) {
        BOOL searchFound = NO;
        GlkController *glkctl = _glkctl;
        for (GlkWindow *view in glkctl.windowsIncludingQuoteBox) {
            if (![view isKindOfClass:[GlkGraphicsWindow class]]) {
                NSString *contentString = view.textview.string;

                NSRange resultRange = [contentString rangeOfString:searchString options:NSCaseInsensitiveSearch range:NSMakeRange(0, contentString.length) locale:nil];

                if (resultRange.location == NSNotFound)
                    continue;

                if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
                    searchFound = (resultRange.location < fromRange.location);
                } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
                    searchFound = (resultRange.location >= NSMaxRange(fromRange));
                }
                if (searchFound) {
                    searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:view.textview];
                    searchResult.targetRange = resultRange;
                    return searchResult;
                }

                bestMatchRange = resultRange;
                bestMatch = view.textview;
            }
        }
    }
    if (bestMatch) {
        searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:bestMatch];
        searchResult.targetRange = bestMatchRange;
    }
    return searchResult;
}

- (NSAccessibilityCustomRotorItemResult *)linksRotor:(NSAccessibilityCustomRotor *)rotor
                           resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters {

    NSAccessibilityCustomRotorItemResult *searchResult = nil;

    NSAccessibilityCustomRotorItemResult *currentItemResult = searchParameters.currentItem;
    NSAccessibilityCustomRotorSearchDirection direction = searchParameters.searchDirection;
    NSString *filterText = searchParameters.filterString;
    NSRange currentRange = currentItemResult.targetRange;

    NSMutableArray<NSValue *> *children = [[NSMutableArray alloc] init];
    NSMutableArray *linkTargetViews = [[NSMutableArray alloc] init];

    NSUInteger currentItemIndex = NSNotFound;
    GlkController *glkctl = _glkctl;

    NSArray *allWindows = glkctl.gwindows.allValues;
    if (glkctl.gameID == kGameIsAColderLight && allWindows.count == 5) {
        // Reading order for A Colder Light's layout
        GlkWindow *w3 = glkctl.gwindows[@(3)], *w4 = glkctl.gwindows[@(4)], *w0 = glkctl.gwindows[@(0)], *w1 = glkctl.gwindows[@(1)];
        if (w3 && w4 && w0 && w1)
            allWindows = @[w3, w4, w0, w1];
    }
    for (GlkWindow *view in allWindows) {
        if (![view isKindOfClass:[GlkGraphicsWindow class]]) {
            NSTextView *targetTextView = view.textview;
            NSArray<NSValue *> *links = view.links;

            if (filterText.length && links.count) {
                NSString __block *text = targetTextView.string;
                links = [links filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSValue *object, NSDictionary *unused) {
                    NSRange range = object.rangeValue;
                    NSString *subString = [text substringWithRange:range];
                    return ([subString localizedCaseInsensitiveContainsString:filterText]);
                }]];
            }

            [children addObjectsFromArray:links];

            while (linkTargetViews.count < children.count)
                [linkTargetViews addObject:targetTextView];
        }
    }

    currentItemIndex = [children indexOfObject:[NSValue valueWithRange:currentRange]];

    if (children.count == 0) {
        return nil;
    }

    currentItemIndex = SPRotorStep(currentItemIndex, children.count, direction);

    if (currentItemIndex != NSNotFound) {
        NSTextView *targetElement = linkTargetViews[currentItemIndex];
        searchResult = SPTextRangeResult(targetElement, children[currentItemIndex].rangeValue);
        NSRange textRange = searchResult.targetRange;
        NSString *string = targetElement.textStorage.string;
        unichar firstChar = textRange.location < string.length ? [string characterAtIndex:textRange.location] : 0;
        if (glkctl.gameID == kGameIsAColderLight && firstChar == '<' && textRange.length == 1) {
            searchResult.customLabel = NSLocalizedString(@"Previous Menu", nil);
        } else if (firstChar == NSAttachmentCharacter) {
            NSDictionary *attrs = [targetElement.textStorage attributesAtIndex:textRange.location effectiveRange:nil];
            searchResult.customLabel = [NSString stringWithFormat:NSLocalizedString(@"Image with link I.D. %@", nil), attrs[NSLinkAttributeName]];
        } else {
            searchResult.customLabel = [string substringWithRange:textRange];
        }
    }
    return searchResult;
}

- (NSAccessibilityCustomRotorItemResult *)glkWindowRotor:(NSAccessibilityCustomRotor *)rotor
                               resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters {
    NSAccessibilityCustomRotorItemResult *searchResult = nil;

    NSAccessibilityCustomRotorItemResult *currentItemResult = searchParameters.currentItem;
    NSAccessibilityCustomRotorSearchDirection direction = searchParameters.searchDirection;
    NSString *filterText = searchParameters.filterString;

    NSMutableArray *children = [[NSMutableArray alloc] init];
    NSMutableArray *strings = [[NSMutableArray alloc] init];
    
    GlkController *glkctl = _glkctl;
    NSArray *allWindows = [glkctl.windowsIncludingQuoteBox sortedArrayUsingComparator:
                  ^NSComparisonResult(NSView * obj1, NSView * obj2){
        CGFloat y1 = obj1.frame.origin.y;
        CGFloat y2 = obj2.frame.origin.y;
        if (y1 > y2) {
            return (NSComparisonResult)NSOrderedDescending;
        }
        if (y1 < y2) {
            return (NSComparisonResult)NSOrderedAscending;
        }
        return (NSComparisonResult)NSOrderedSame;
    }];

    NSCharacterSet *charset = SPRotorLabelTrimSet();

    for (GlkWindow *win in allWindows) {
        if (![win isKindOfClass:[GlkGraphicsWindow class]]) {
            GlkTextBufferWindow *bufWin = (GlkTextBufferWindow *)win;
            NSTextView *textview = bufWin.textview;
            NSString *string = textview.string;
            if ([win isKindOfClass:[GlkTextBufferWindow class]]) {
                if (bufWin.moveRanges.count) {
                    NSRange range = bufWin.moveRanges.lastObject.rangeValue;
                    string = [string substringFromIndex:range.location];
                }
            }

            if (string.length && (filterText.length == 0 || [string localizedCaseInsensitiveContainsString:filterText])) {
                [children addObject:textview];
                string = [string stringByTrimmingCharactersInSet:charset];
                [strings addObject:string.copy];
            }
        } else {
            NSString *string = win.accessibilityRoleDescription;
            if (string.length && ((GlkGraphicsWindow *)win).showingImage && (filterText.length == 0 || [string localizedCaseInsensitiveContainsString:filterText])) {
                [children addObject:win];
                [strings addObject:string.copy];
            }
        }
    }

    if (!children.count)
        return nil;

    NSUInteger currentItemIndex = [children indexOfObject:currentItemResult.targetElement];

    currentItemIndex = SPRotorStep(currentItemIndex, children.count, direction);

    if (currentItemIndex == NSNotFound) {
        return nil;
    }

    NSTextView *targetWindow = children[currentItemIndex];

    if (targetWindow) {
        if (![targetWindow isKindOfClass:[GlkGraphicsWindow class]]) {
            NSRange range = NSMakeRange(0, targetWindow.string.length);
            NSArray<NSValue *> *moveRanges = ((GlkWindow *)targetWindow.delegate).moveRanges;
            if (moveRanges.count) {
                if ([targetWindow.delegate isKindOfClass:[GlkTextBufferWindow class]])
                    [(GlkTextBufferWindow *)targetWindow.delegate forceLayout];
                range = moveRanges.lastObject.rangeValue;
            }
            searchResult = SPTextRangeResult(targetWindow, range);
        } else {
            searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:targetWindow];
        }
        searchResult.customLabel = strings[currentItemIndex];
    }
    return searchResult;
}


- (NSAccessibilityCustomRotorItemResult *)imagesRotor:(NSAccessibilityCustomRotor *)rotor
                            resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters {

    if (_glkctl.theme.vOSpeakImages == kVOImageNone) {
        return nil;
    }

    NSAccessibilityCustomRotorItemResult *searchResult = nil;

    NSAccessibilityCustomRotorItemResult *currentItemResult = searchParameters.currentItem;
    NSAccessibilityCustomRotorSearchDirection direction = searchParameters.searchDirection;
    NSString *filterText = searchParameters.filterString;

    NSMutableArray *children = [[NSMutableArray alloc] init];
    NSMutableArray *targetViews = [[NSMutableArray alloc] init];

    NSArray *allWindows = _glkctl.gwindows.allValues;

    for (GlkWindow *view in allWindows) {
        if (![view isKindOfClass:[GlkTextGridWindow class]]) {
            NSArray *viewimages = view.images;
            NSString __block *label;
            if (filterText.length && viewimages.count) {
                viewimages = [viewimages filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
                    if ([object isKindOfClass:[SubImage class]]) {
                        label = ((SubImage *)object).accessibilityLabel;
                    } else if ([object isKindOfClass:[NSValue class]]) {
                        NSTextStorage *txtstorage = view.textview.textStorage;
                        NSTextAttachment *attachment = [txtstorage attribute:NSAttachmentAttributeName atIndex:((NSValue *)object).rangeValue.location effectiveRange:nil];
                        MyAttachmentCell *cell = (MyAttachmentCell *)attachment.attachmentCell;
                        label = cell.customA11yLabel;
                    } else NSLog(@"Unknown class! %@", [object class]);
                    BOOL result = [label localizedCaseInsensitiveContainsString:filterText];
                    return result;
                }]];
            }

            [children addObjectsFromArray:viewimages];

            id object = [NSNull null];
            while (targetViews.count < children.count) {
                if ([view isKindOfClass:[GlkTextBufferWindow class]])
                    object = view.textview;
                [targetViews addObject:object];
            }
        }
    }

    if (children.count == 0) {
        return nil;
    }

    NSUInteger currentItemIndex = NSNotFound;

    NSAccessibilityElement *targetElement = (NSAccessibilityElement *)currentItemResult.targetElement;

    if ([targetElement isKindOfClass:[BufferTextView class]]) {
        NSUInteger pos = currentItemResult.targetRange.location;
        NSUInteger index = 0;
        for (id child in children) {
            if ([child isKindOfClass:[NSValue class]]) {
                BufferTextView *view = targetViews[index];
                NSTextStorage *txtstorage = view.textStorage;
                NSTextAttachment *attachment = [txtstorage attribute:NSAttachmentAttributeName atIndex:((NSValue *)child).rangeValue.location effectiveRange:nil];
                MyAttachmentCell *cell = (MyAttachmentCell *)attachment.attachmentCell;
                if (pos == cell.pos) {
                    currentItemIndex = index;
                    break;
                }
            }
            index++;
        }
    } else if ([targetElement isKindOfClass:[SubImage class]]){
        for (id child in children) {
            if ([child isKindOfClass:[SubImage class]]) {
                if ([((SubImage *)child).uuid isEqualToString:((SubImage *)targetElement).uuid]) {
                    currentItemIndex = [children indexOfObject:child];
                    break;
                }
            }
        }
    }

    currentItemIndex = SPRotorStep(currentItemIndex, children.count, direction);

    if (currentItemIndex == NSNotFound) { // Reached end of list
        return nil;
    }

    id targetImage = children[currentItemIndex];

    NSString *label = NSLocalizedString(@"image", nil);

    if (targetImage) {
        if ([targetImage isKindOfClass:[NSValue class]]) {
            NSRange range = ((NSValue *)targetImage).rangeValue;
            BufferTextView *view = targetViews[currentItemIndex];
            NSTextAttachment *attachment = [view.textStorage attribute:NSAttachmentAttributeName atIndex:range.location effectiveRange:nil];
            MyAttachmentCell *cell = (MyAttachmentCell *)attachment.attachmentCell;

            // Because margin images are such a hack, VoiceOver does not
            // understand that they have a size, so we have to add an extra
            // character to the selection to make the view scroll to the image
            // (VoiceOver knows that letters have a size.)
            if (cell.isMargin) {
                range.length++;
            }
            searchResult = SPTextRangeResult(view, range);
            label = cell.customA11yLabel;
            cell.pos = searchResult.targetRange.location;
        } else {
            searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:targetImage];
            label = ((SubImage *)targetImage).accessibilityLabel;
            searchResult.targetRange = NSMakeRange(0, 0);
        }
        if (children.count > 1)
            label = [label stringByAppendingFormat:@" (%ld)", currentItemIndex + 1];
        searchResult.customLabel = label;
    }
    return searchResult;
}


- (NSAccessibilityCustomRotorItemResult *)commandHistoryRotor:(NSAccessibilityCustomRotor *)rotor
                                    resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters {

    NSAccessibilityCustomRotorItemResult *searchResult = nil;

    NSAccessibilityCustomRotorSearchDirection direction = searchParameters.searchDirection;
    NSRange currentRange = searchParameters.currentItem.targetRange;

    NSString *filterText = searchParameters.filterString;

    GlkTextBufferWindow *largest = (GlkTextBufferWindow *)_glkctl.largestWithMoves;
    if (!largest)
        return nil;

    NSArray *children = [largest.moveRanges reverseObjectEnumerator].allObjects;

    if (children.count > 50)
        children = [children subarrayWithRange:NSMakeRange(0, 50)];
    NSMutableArray *strings = [[NSMutableArray alloc] initWithCapacity:children.count];
    NSMutableArray *mutableChildren = [[NSMutableArray alloc] initWithCapacity:children.count];

    for (NSValue *child in children) {
        NSRange range = child.rangeValue;
        NSString *string = [largest.textview accessibilityAttributedStringForRange:range].string;
        if (filterText.length == 0 || [string localizedCaseInsensitiveContainsString:filterText]) {
            [strings addObject:string];
            [mutableChildren addObject:child];
        }
    }

    if (!mutableChildren.count)
        return nil;

    children = mutableChildren;

    // VoiceOver hands back the range it showed for the current item. The
    // newest move grows while the game prints more of it, so for that one
    // the range may be stale; then match on where the move starts, or the
    // rotor would start over at the newest item instead of stepping on.
    // VoiceOver also passes the element its cursor is on as the current
    // item, with an empty range; that must not match a move either way.
    NSUInteger currentItemIndex = NSNotFound;
    if (searchParameters.currentItem && currentRange.length > 0) {
        currentItemIndex = [children indexOfObject:[NSValue valueWithRange:currentRange]];
        if (currentItemIndex == NSNotFound) {
            currentItemIndex = [children indexOfObjectPassingTest:^BOOL(NSValue *value, NSUInteger idx, BOOL *stop) {
                return value.rangeValue.location == currentRange.location;
            }];
        }
    }

    currentItemIndex = SPRotorStep(currentItemIndex, children.count, direction);

    if (currentItemIndex == NSNotFound) {
        return nil;
    }

    NSValue *targetRangeValue = children[currentItemIndex];

    if (targetRangeValue) {
        searchResult = SPTextRangeResult(largest.textview, targetRangeValue.rangeValue);
        // By adding a custom label, all ranges are reliably listed in the rotor
        NSString *string = [strings[currentItemIndex] stringByTrimmingCharactersInSet:SPRotorLabelTrimSet()];
        // Strip command line if the speak command setting is off
        if (!_glkctl.theme.vOSpeakCommand && !_glkctl.showingInfocomV6Menu) {
            NSUInteger promptIndex = searchResult.targetRange.location;
            if (promptIndex != 0)
                promptIndex--;
            NSString *text = largest.textview.string;
            if (promptIndex < text.length && ([text characterAtIndex:promptIndex] == '>' || (promptIndex > 0 && [text characterAtIndex:promptIndex - 1] == '>'))) {
                NSRange foundRange = [string rangeOfString:@"\n"];
                if (foundRange.location != NSNotFound) {
                    string = [string substringFromIndex:foundRange.location];
                }
            }
        }
        if ([largest isKindOfClass:[GlkTextBufferWindow class]])
            [largest forceLayout];
        searchResult.customLabel = string;
    }

    return searchResult;
}

- (NSArray *)createCustomRotors {
    NSMutableArray *rotorsArray = [[NSMutableArray alloc] init];

    BOOL hasLinks = NO;
    BOOL hasImages = NO;
    GlkController *glkctl = _glkctl;

    for (GlkWindow *view in glkctl.gwindows.allValues) {
        if (![view isKindOfClass:[GlkGraphicsWindow class]] && view.links.count) {
            hasLinks = YES;
        }
        if (![view isKindOfClass:[GlkTextGridWindow class]] && view.images.count) {
            hasImages = YES;
        }
    }

    // Create the link rotor
    if (hasLinks) {
        NSAccessibilityCustomRotor *linkRotor = [[NSAccessibilityCustomRotor alloc] initWithRotorType:NSAccessibilityCustomRotorTypeLink itemSearchDelegate:self];
        [rotorsArray addObject:linkRotor];
    }

    // Create the images rotor
    if (hasImages && glkctl.theme.vOSpeakImages != kVOImageNone) {
        NSAccessibilityCustomRotor *imagesRotor = [[NSAccessibilityCustomRotor alloc] initWithRotorType:NSAccessibilityCustomRotorTypeImage itemSearchDelegate:self];
        [rotorsArray addObject:imagesRotor];
    }

    // Create the text search rotor.
    NSAccessibilityCustomRotor *textSearchRotor = [[NSAccessibilityCustomRotor alloc] initWithRotorType:NSAccessibilityCustomRotorTypeAny itemSearchDelegate:self];
    [rotorsArray addObject:textSearchRotor];

    // Create the command history rotor. Always offer it rather than gating on
    // largestWithMoves: VoiceOver caches an element's rotor set at focus time
    // and is not notified when moveRanges later become non-empty, so a rotor
    // built before the first move would stay missing. commandHistoryRotor:…
    // returns nil when there is nothing to show, so an empty rotor is harmless.
    NSAccessibilityCustomRotor *commandHistoryRotor = [[NSAccessibilityCustomRotor alloc] initWithLabel:NSLocalizedString(@"Command history", nil) itemSearchDelegate:self];
    [rotorsArray addObject:commandHistoryRotor];

    // Create the Glk windows rotor
    if (glkctl.gwindows.count) {
        NSAccessibilityCustomRotor *glkWindowRotor = [[NSAccessibilityCustomRotor alloc] initWithLabel:NSLocalizedString(@"Game windows", nil) itemSearchDelegate:self];
        [rotorsArray addObject:glkWindowRotor];
    }
    return rotorsArray;
}

@end
