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
#import "MarginContainer.h"
#import "MarginImage.h"
#import "MyAttachmentCell.h"
#import "Theme.h"
#import "SubImage.h"

#include "glk.h"


@implementation RotorHandler


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
    NSRange bestMatchRange;

    if (searchString.length) {
        BOOL searchFound = NO;
        GlkController *glkctl = _glkctl;
        NSArray<GlkWindow *> *allWindows = glkctl.gwindows.allValues;
        if (glkctl.quoteBoxes.count)
            allWindows = [allWindows arrayByAddingObject:glkctl.quoteBoxes.lastObject];
        for (GlkWindow *view in allWindows) {
            if (![view isKindOfClass:[GlkGraphicsWindow class]]) {
                NSString *contentString = ((GlkTextGridWindow *)view).textview.string;

                NSRange resultRange = [contentString rangeOfString:searchString options:NSCaseInsensitiveSearch range:NSMakeRange(0, contentString.length) locale:nil];

                if (resultRange.location == NSNotFound)
                    continue;

                if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
                    searchFound = (resultRange.location < fromRange.location);
                } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
                    searchFound = (resultRange.location >= NSMaxRange(fromRange));
                }
                if (searchFound) {
                    searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:((GlkTextGridWindow *)view).textview];
                    searchResult.targetRange = resultRange;
                    return searchResult;
                }

                bestMatchRange = resultRange;
                bestMatch = ((GlkTextGridWindow *)view).textview;
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
    if (glkctl.colderLight && allWindows.count == 5) {
        allWindows = @[glkctl.gwindows[@(3)], glkctl.gwindows[@(4)], glkctl.gwindows[@(0)], glkctl.gwindows[@(1)]];
    }
    for (GlkWindow *view in allWindows) {
        if (![view isKindOfClass:[GlkGraphicsWindow class]]) {
            id targetTextView = ((GlkTextBufferWindow *)view).textview;
            NSArray *links = [view links];

            if (filterText.length && links.count) {
                NSString __block *text = ((NSTextView *)targetTextView).string;
                links = [links filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
                    NSRange range = ((NSValue *)object).rangeValue;
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

    if (children.count == 0)
        return nil;

    if (currentItemIndex == NSNotFound) {
        // Find the start or end element.
        if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            currentItemIndex = 0;
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            currentItemIndex = children.count - 1;
        }
    } else {
        if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            if (currentItemIndex == 0)
                currentItemIndex = NSNotFound;
            else
                currentItemIndex--;
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            if (currentItemIndex == children.count - 1)
                currentItemIndex = NSNotFound;
            else
                currentItemIndex++;
        }
    }

    if (currentItemIndex != NSNotFound) {
        NSRange textRange = children[currentItemIndex].rangeValue;
        id targetElement = linkTargetViews[currentItemIndex];
        searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:targetElement];
        NSString *string = ((NSTextView *)targetElement).textStorage.string;
        NSRange allText = NSMakeRange(0, string.length);
        textRange = NSIntersectionRange(allText, textRange);
        unichar firstChar = [string characterAtIndex:textRange.location];
        if (glkctl.colderLight && firstChar == '<' && textRange.length == 1) {
            searchResult.customLabel = NSLocalizedString(@"Previous Menu", nil);
        } else if (firstChar == NSAttachmentCharacter) {
            NSDictionary *attrs = [((NSTextView *)targetElement).textStorage attributesAtIndex:textRange.location effectiveRange:nil];
            searchResult.customLabel = [NSString stringWithFormat:@"Image with link I.D. %@", attrs[NSLinkAttributeName]];
        } else {
            searchResult.customLabel = [string substringWithRange:textRange];
        }
        searchResult.targetRange = textRange;
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
    NSArray *allWindows = glkctl.gwindows.allValues;

    if (glkctl.quoteBoxes.count)
        allWindows = [allWindows arrayByAddingObject:glkctl.quoteBoxes.lastObject];

    allWindows = [allWindows sortedArrayUsingComparator:
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

    NSString *charSetString = @"\u00A0 >\n_";
    NSCharacterSet *charset = [NSCharacterSet characterSetWithCharactersInString:charSetString];

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
            if (string.length && win.images.count && (filterText.length == 0 || [string localizedCaseInsensitiveContainsString:filterText])) {
                [children addObject:win];
                [strings addObject:string.copy];
            }
        }
    }

    if (!children.count)
        return nil;

    NSUInteger currentItemIndex = [children indexOfObject:currentItemResult.targetElement];

    if (currentItemIndex == NSNotFound) {
        // Find the start or end element.
        if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            currentItemIndex = 0;
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            currentItemIndex = children.count - 1;
        }
    } else {
        if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            if (currentItemIndex == 0) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex--;
            }
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            if (currentItemIndex == children.count - 1) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex++;
            }
        }
    }

    if (currentItemIndex == NSNotFound) {
        return nil;
    }

    NSTextView *targetWindow = children[currentItemIndex];

    if (targetWindow) {
        searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement: targetWindow];
        searchResult.customLabel = strings[currentItemIndex];
        if (![targetWindow isKindOfClass:[GlkGraphicsWindow class]]) {
            NSRange allText = NSMakeRange(0, targetWindow.string.length);
            NSArray<NSValue *> *moveRanges = ((GlkWindow *)targetWindow.delegate).moveRanges;
            if (moveRanges && moveRanges.count) {
                if ([targetWindow.delegate isKindOfClass:[GlkTextBufferWindow class]])
                    [(GlkTextBufferWindow *)targetWindow.delegate forceLayout];
                NSRange range = moveRanges.lastObject.rangeValue;
                searchResult.targetRange = NSIntersectionRange(allText, range);
            } else searchResult.targetRange = allText;
        }
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
            NSArray *viewimages = [view images];
            NSString __block *label;
            if (filterText.length && viewimages.count) {
                viewimages = [viewimages filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary *bindings) {
                    if ([object isKindOfClass:[SubImage class]]) {
                        label = ((SubImage *)object).accessibilityLabel;
                    } else if ([object isKindOfClass:[NSValue class]]) {
                        NSTextStorage *txtstorage = ((GlkTextBufferWindow *)view).textview.textStorage;
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
                    object = ((GlkTextBufferWindow *)view).textview;
                [targetViews addObject:object];
            }
        }
    }

    if (!children.count) {
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
    } else {
        currentItemIndex = [children indexOfObject:targetElement];
    }

    if (currentItemIndex == NSNotFound) {
        // Find the start or end element.
        if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            currentItemIndex = 0;
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            currentItemIndex = children.count - 1;
        }
    } else {
        if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            if (currentItemIndex == 0) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex--;
            }
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            if (currentItemIndex == children.count - 1) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex++;
            }
        }
    }

    if (currentItemIndex == NSNotFound) { // Reached end of list
        return nil;
    }

    id targetImage = children[currentItemIndex];

    NSString *label = @"image";

    if (targetImage) {
        if ([targetImage isKindOfClass:[NSValue class]]) {
            NSRange range = ((NSValue *)targetImage).rangeValue;
            BufferTextView *view = targetViews[currentItemIndex];
            NSTextAttachment *attachment = [view.textStorage attribute:NSAttachmentAttributeName atIndex:range.location effectiveRange:nil];
            MyAttachmentCell *cell = (MyAttachmentCell *)attachment.attachmentCell;
            NSRange allText = NSMakeRange(0, view.string.length);
            range = NSIntersectionRange(range, allText);
            searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:view];
            searchResult.targetRange = range;
            label = cell.customA11yLabel;
        } else {
            searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:targetImage];
            label = ((SubImage *)targetImage).accessibilityLabel;
            searchResult.targetRange = NSMakeRange(0, 0);
        }
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

    NSUInteger currentItemIndex = [children indexOfObject:[NSValue valueWithRange:currentRange]];

    if (currentItemIndex == NSNotFound) {
        // Find the start or end element.
        if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            currentItemIndex = 0;
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            currentItemIndex = children.count - 1;
        }
    } else {
        if (direction == NSAccessibilityCustomRotorSearchDirectionPrevious) {
            if (currentItemIndex == 0) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex--;
            }
        } else if (direction == NSAccessibilityCustomRotorSearchDirectionNext) {
            if (currentItemIndex == children.count - 1) {
                currentItemIndex = NSNotFound;
            } else {
                currentItemIndex++;
            }
        }
    }

    if (currentItemIndex == NSNotFound) {
        return nil;
    }

    NSValue *targetRangeValue = children[currentItemIndex];

    if (targetRangeValue) {
        NSRange textRange = targetRangeValue.rangeValue;
        searchResult = [[NSAccessibilityCustomRotorItemResult alloc] initWithTargetElement:largest.textview];
        NSRange allText = NSMakeRange(0, largest.textview.string.length);
        searchResult.targetRange = NSIntersectionRange(allText, textRange);
        // By adding a custom label, all ranges are reliably listed in the rotor
        NSString *charSetString = @"\u00A0 >\n_";
        NSCharacterSet *charset = [NSCharacterSet characterSetWithCharactersInString:charSetString];
        NSString *string = strings[currentItemIndex];
        string = [string stringByTrimmingCharactersInSet:charset];
        {
            // Strip command line if the speak command setting is off
            if (!_glkctl.theme.vOSpeakCommand)
            {
                NSUInteger promptIndex = searchResult.targetRange.location;
                if (promptIndex != 0)
                    promptIndex--;
                if ([largest.textview.string characterAtIndex:promptIndex] == '>' || (promptIndex > 0 && [largest.textview.string characterAtIndex:promptIndex - 1] == '>')) {
                    NSRange foundRange = [string rangeOfString:@"\n"];
                    if (foundRange.location != NSNotFound)
                    {
                        string = [string substringFromIndex:foundRange.location];
                    }
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
    // Create the command history rotor
    if ([glkctl largestWithMoves]) {
        NSAccessibilityCustomRotor *commandHistoryRotor = [[NSAccessibilityCustomRotor alloc] initWithLabel:NSLocalizedString(@"Command history", nil) itemSearchDelegate:self];
        [rotorsArray addObject:commandHistoryRotor];
    }
    // Create the Glk windows rotor
    if (glkctl.gwindows.count) {
        NSAccessibilityCustomRotor *glkWindowRotor = [[NSAccessibilityCustomRotor alloc] initWithLabel:NSLocalizedString(@"Game windows", nil) itemSearchDelegate:self];
        [rotorsArray addObject:glkWindowRotor];
    }
    return rotorsArray;
}

@end
