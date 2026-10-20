#import "PreferencesPrivate.h"

#import "Constants.h"
#import "CoreDataManager.h"
#import "GlkStyle.h"
#import "PreviewController.h"
#import "Theme.h"

@implementation Preferences (Window)

#pragma mark Preview

- (void)notePreferencesChanged:(NSNotification *)notify {
    // Change the theme of the sample text field
    [self.coreDataManager saveChanges];
}

- (NSSize)windowWillResize:(NSWindow *)window
                    toSize:(NSSize)frameSize {
    if (window != self.window) {
        return frameSize;
    }

    if (frameSize.height > defaultWindowHeight && frameSize.height > NSHeight(self.window.frame)) { // We are enlarging
        CGFloat maxHeight = MAX([self calculateWindowHeightWithPreview] + 40, NSHeight(self.window.frame));
        if (frameSize.height > maxHeight)
            frameSize.height = maxHeight;
    }

    return frameSize;
}

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window
                        defaultFrame:(NSRect)newFrame {

    if (window != self.window)
        return newFrame;

    CGFloat newHeight;

    if (!_previewShown) {
        newHeight = defaultWindowHeight;
    } else {
        newHeight = [self calculateWindowHeightWithPreview];
        _previewHeightConstraint.constant = [_previewController calculateHeight] + 40;
    }

    NSRect currentFrame = window.frame;

    CGFloat diff = currentFrame.size.height - newHeight;
    currentFrame.origin.y += diff;
    currentFrame.size.height = newHeight;

    return currentFrame;
}

- (BOOL)windowShouldZoom:(NSWindow *)window toFrame:(NSRect)newFrame {
    if (window != self.window)
        return YES;
    if (!_previewShown && newFrame.size.height > defaultWindowHeight)
        return NO;

    return YES;
}

- (void)resizeWindowToHeight:(CGFloat)height animate:(BOOL)animate {
    NSWindow *prefsPanel = self.window;

    CGFloat oldheight = NSHeight(prefsPanel.frame);

    _previewController.view.hidden = NO;
    BOOL shouldHideOnCompletion = (height == defaultWindowHeight);

    if (ceil(height) == ceil(oldheight)) {
        if (shouldHideOnCompletion)
            _previewController.view.hidden = YES;
        return;
    }

    CGRect screenframe = prefsPanel.screen.visibleFrame;

    CGRect winrect = prefsPanel.frame;

    winrect.size.height = height;
    winrect.size.width = kDefaultPrefWindowWidth;

    // If the entire text does not fit on screen, don't change height at all
    if (winrect.size.height > screenframe.size.height)
        winrect.size.height = oldheight;

    CGFloat offset = winrect.size.height - oldheight;
    winrect.origin.y -= offset;

    // If window is partly off the screen, move it (just) inside
    if (NSMaxX(winrect) > NSMaxX(screenframe))
        winrect.origin.x = NSMaxX(screenframe) - winrect.size.width;

    if (NSMinY(winrect) < 0)
        winrect.origin.y = NSMinY(screenframe);

    CGFloat newPrevHeightConstant = NSHeight(winrect) - defaultWindowHeight;
    CGFloat newTextHeightConstant = MIN(newPrevHeightConstant, [_previewController calculateHeight]);

    // Nothing to animate while the window is off screen (nib load, state
    // restoration), and a synchronous resize leaves no animation in flight
    // for a later resize to race.
    if (!prefsPanel.visible)
        animate = NO;

    NSUInteger generation = ++resizeGeneration;

    if (!animate) {
        _previewHeightConstraint.constant = newPrevHeightConstant;
        _previewController.textHeight.constant = newTextHeightConstant;
        [self.window setFrame:winrect display:YES];
        if (shouldHideOnCompletion)
            _previewController.view.hidden = YES;
    } else {
        Preferences * __weak weakSelf = self;
        [NSAnimationContext
         runAnimationGroup:^(NSAnimationContext *context) {
            context.duration = 0.3;
            [[prefsPanel animator]
             setFrame:winrect
             display:YES];
            [_previewHeightConstraint.animator setConstant:newPrevHeightConstant];
            [_previewController.textHeight.animator setConstant:newTextHeightConstant];
        } completionHandler:^{
            Preferences *strongSelf = weakSelf;
            // A newer resize decides whether the preview is shown
            if (strongSelf && shouldHideOnCompletion && strongSelf->resizeGeneration == generation) {
                strongSelf.previewController.view.hidden = YES;
            }
        }];
    }
}

- (CGFloat)calculateWindowHeightWithPreview {

    CGFloat proposedHeight = [_previewController calculateHeight];

    CGFloat totalHeight = defaultWindowHeight + proposedHeight + 40;
    NSScreen *screen = self.window.screen ?: [NSScreen mainScreen];
    CGRect screenframe = screen.visibleFrame;

    if (totalHeight > screenframe.size.height) {
        totalHeight = screenframe.size.height;
    }
    return totalHeight;
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
    _previewController.view.hidden = NO;
}

- (void)windowDidEndLiveResize:(id)sender {
    _previewShown = (NSHeight(self.window.frame) > defaultWindowHeight);

    [[NSUserDefaults standardUserDefaults] setBool:_previewShown forKey:kShowThemePreviewKey];
    _previewController.view.hidden = !_previewShown;

    // Scroll after the layout pass that showing or hiding the preview triggers
    PreviewController *blockPrevCtrl = _previewController;
    dispatch_async(dispatch_get_main_queue(), ^{
        [blockPrevCtrl scrollToTop];
    });
}

#pragma mark -
#pragma mark Windows restoration

- (void)window:(NSWindow *)window willEncodeRestorableState:(NSCoder *)state {
    NSString *selectedfontString = nil;
    if (selectedFontButton)
        selectedfontString = selectedFontButton.identifier;
    [state encodeObject:selectedfontString forKey:@"selectedFont"];
    [state encodeObject:(NSString *)self.window.toolbar.selectedItemIdentifier forKey:@"selectedItemIdentifier"];
    [state encodeObject:@(NSHeight(_previewController.view.frame)) forKey:@"previewHeight"];
}

- (void)window:(NSWindow *)window didDecodeRestorableState:(NSCoder *)state {
    NSString *selectedfontString = [state decodeObjectOfClass:[NSString class] forKey:@"selectedFont"];
    if (selectedfontString != nil) {
        NSArray *fontsButtons = @[btnBufferFont, btnGridFont, btnAnyFont];
        for (NSButton *button in fontsButtons) {
            if ([button.identifier isEqualToString:selectedfontString]) {
                selectedFontButton = button;
            }
        }
    }

    NSToolbarItem *restoredItem = nil;

    NSToolbarItemIdentifier toolbarItemIdentifier = [state decodeObjectOfClass:[NSString class] forKey:@"selectedItemIdentifier"];
    if (toolbarItemIdentifier) {
        for (NSToolbarItem *item in self.window.toolbar.items) {
            if ([item.itemIdentifier isEqual:toolbarItemIdentifier]) {
                restoredItem = item;
                break;
            }
        }
    }

    // A saved state from an older version may name a panel that no longer
    // exists, or have only a preview height: keep the panel that is shown.
    if (!restoredItem) {
        for (NSToolbarItem *item in self.window.toolbar.items) {
            if ([item.itemIdentifier isEqual:self.window.toolbar.selectedItemIdentifier]) {
                restoredItem = item;
                break;
            }
        }
    }
    if (!restoredItem)
        restoredItem = self.window.toolbar.items.firstObject;
    toolbarItemIdentifier = restoredItem.itemIdentifier;
    if (toolbarItemIdentifier)
        self.window.toolbar.selectedItemIdentifier = toolbarItemIdentifier;

    NSNumber *previewHeightNumber = (NSNumber *)[state decodeObjectOfClass:[NSNumber class] forKey:@"previewHeight"];
    if (previewHeightNumber) {
        restoredPreviewHeight = previewHeightNumber.floatValue;
        _previewHeightConstraint.constant = restoredPreviewHeight;
    } else {
        restoredPreviewHeight = 0;
    }

    if (restoredItem && itemIdentifierToViewDict[toolbarItemIdentifier]) {
        currentPanel = itemIdentifierToViewDict[toolbarItemIdentifier];
        defaultWindowHeight = ceil(NSHeight([self.window frameRectForContentRect:NSMakeRect(0, 0, kDefaultPrefWindowWidth, NSHeight(currentPanel.frame))]));
        [self switchToPanel:restoredItem resizePreview:YES];
        // The window is still off screen here, so the switch above and the
        // one in windowDidLoad resized synchronously and left no animation
        // running for this resize to race. It is synchronous too.
        [self resizeWindowToHeight:previewHeightNumber.floatValue + defaultWindowHeight animate:YES];
    }
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    return _managedObjectContext.undoManager;
}

- (IBAction)togglePreview:(id)sender {
    if (_previewShown) {
        [self resizeWindowToHeight:defaultWindowHeight animate:YES];
        _previewShown = NO;
    } else {
        _previewShown = YES;
        [self resizeWindowToHeight:[self calculateWindowHeightWithPreview] animate:YES];
    }
    [[NSUserDefaults standardUserDefaults] setBool:_previewShown forKey:kShowThemePreviewKey];
}

#pragma mark Toolbar

// label and tooltip are already localised
- (NSToolbarItem *)toolbarItemWithImage:(NSString *)imageName label:(NSString *)label tooltip:(NSString *)tooltip identifier:(NSToolbarItemIdentifier)identifier {
    NSToolbarItem *toolbarItem = [[NSToolbarItem alloc] initWithItemIdentifier:identifier];
    NSImage *image = nil;
    if (@available(macOS 11.0, *)) {
        NSImageSymbolConfiguration *config = [NSImageSymbolConfiguration configurationWithScale:NSImageSymbolScaleMedium];
        image = [NSImage imageWithSystemSymbolName:imageName accessibilityDescription:tooltip];
        if (!image)
            image = [NSImage imageNamed:imageName];
        image = [image imageWithSymbolConfiguration:config];
    } else {
        image = [NSImage imageNamed:imageName];
        image.accessibilityDescription = tooltip;
    }

    toolbarItem.image = image;
    toolbarItem.action = @selector(switchToPanel:);
    toolbarItem.target = self;
    toolbarItem.label = label;
    toolbarItem.toolTip = tooltip;
    return toolbarItem;
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
    NSToolbarItem *toolbarItem = nil;

    if (itemIdentifier == themesPanel) {
        return [self toolbarItemWithImage:@"paintbrush" label:NSLocalizedString(@"Themes", nil) tooltip:NSLocalizedString(@"Select and organize themes", nil) identifier:itemIdentifier];
    } else if (itemIdentifier == stylesPanel) {
        if (@available(macOS 11.0, *)) {
            return [self toolbarItemWithImage:@"butterfly" label:NSLocalizedString(@"Glk Styles", nil) tooltip:NSLocalizedString(@"Glk style settings", nil) identifier:itemIdentifier];
        } else {
            return [self toolbarItemWithImage:@"butterfly.square" label:NSLocalizedString(@"Glk Styles", nil) tooltip:NSLocalizedString(@"Glk style settings", nil) identifier:itemIdentifier];
        }
    } else if (itemIdentifier == presentationPanel) {
        return [self toolbarItemWithImage:@"theatermask.and.paintbrush" label:NSLocalizedString(@"Details", nil) tooltip:NSLocalizedString(@"Presentation settings not covered by Glk styles", nil) identifier:itemIdentifier];
    } else if (itemIdentifier == formatPanel) {
        return [self toolbarItemWithImage:@"compass.drawing" label:NSLocalizedString(@"Format", nil) tooltip:NSLocalizedString(@"Format-specific settings", nil) identifier:itemIdentifier];
    } else if (itemIdentifier == voiceOverPanel) {
        if (@available(macOS 11.0, *)) {
            return [self toolbarItemWithImage:@"voiceover" label:NSLocalizedString(@"VoiceOver", nil) tooltip:NSLocalizedString(@"Text-to-speech settings", nil) identifier:itemIdentifier];
        } else {
            return [self toolbarItemWithImage:@"voiceover.square" label:NSLocalizedString(@"VoiceOver", nil) tooltip:NSLocalizedString(@"Text-to-speech settings", nil) identifier:itemIdentifier];
        }
    } else if (itemIdentifier == miscPanel) {
        return [self toolbarItemWithImage:@"gearshape" label:NSLocalizedString(@"Misc", nil) tooltip:NSLocalizedString(@"Miscellaneous settings", nil) identifier:itemIdentifier];
    } else if (itemIdentifier == globalPanel) {
        return [self toolbarItemWithImage:@"globe" label:NSLocalizedString(@"Global", nil) tooltip:NSLocalizedString(@"Settings that apply to all themes", nil) identifier:itemIdentifier];
    }
    return toolbarItem;
}

- (void)switchToPanel:(NSToolbarItem *)item {
    [self switchToPanel:item resizePreview:NO];
}

- (void)switchToPanel:(NSToolbarItem *)item resizePreview:(BOOL)resizePreview {

    // This must be set here for selecting tab using VoiceOver to work properly
    self.window.toolbar.selectedItemIdentifier = item.itemIdentifier;

    NSView *preferencePane = itemIdentifierToViewDict[item.itemIdentifier];
    if (!preferencePane)
        return;

    currentPanel = preferencePane;

    CGFloat currentPanelHeight = NSHeight(currentPanel.frame);

    defaultWindowHeight = ceil(NSHeight([self.window frameRectForContentRect:NSMakeRect(0, 0,  kDefaultPrefWindowWidth, currentPanelHeight)]));
    self.window.minSize = NSMakeSize(kDefaultPrefWindowWidth, currentPanelHeight);

    NSWindow *window = self.window;
    if (_belowView.subviews.count && _belowView.subviews[0] == currentPanel) {
        return;
    }

    window.title = item.label;

    NSRect newFrame = window.frame;
    NSRect frameForContent = [window frameRectForContentRect:currentPanel.frame];

    CGFloat previewHeight = 0;

    if (_previewShown) {
        previewHeight = resizePreview ? _previewHeightConstraint.constant : NSHeight(_sampleTextBorderView.frame);
        if (restoredPreviewHeight != 0) {
            previewHeight = restoredPreviewHeight;
        }
    }

    restoredPreviewHeight = 0;

    _previewHeightConstraint.constant = previewHeight;

    newFrame.size.height = NSHeight(frameForContent) + previewHeight;

    NSRect visibleFrame = self.window.screen.visibleFrame;
    if (NSHeight(newFrame) > NSHeight(visibleFrame)) {
        newFrame.size.height = NSHeight(visibleFrame);
        frameForContent.size.height = NSHeight(visibleFrame);
        previewHeight = NSHeight(newFrame) - defaultWindowHeight;
        if (previewHeight < 0)
            previewHeight = 0;
        _previewHeightConstraint.constant = previewHeight;
    }

    newFrame.origin.y -= NSHeight(frameForContent) - NSHeight(window.frame) + previewHeight;

    if (newFrame.origin.y < NSMinY(visibleFrame))
        newFrame.origin.y = NSMinY(visibleFrame);

    if (_belowView.subviews.count)
        [_belowView.subviews[0] removeFromSuperview];
    [_belowView addSubview:currentPanel];
    currentPanel.frame = _belowView.bounds;
    currentPanel.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;

    _bottomConstraint.active = NO;
    _belowHeightConstraint.constant = currentPanelHeight;

    PreviewController* blockPrevCtrl = _previewController;

    // While the window is off screen (nib load, state restoration) there is
    // nothing to animate, and resizing synchronously leaves no animation
    // in flight for the restoration's own resize to race.
    if (!window.visible) {
        [window setFrame:newFrame display:YES];
        _bottomConstraint.active = YES;
        window.initialFirstResponder = preferencePane;
        [blockPrevCtrl scrollToTop];
        return;
    }

    Preferences * __weak weakSelf = self;

    [NSAnimationContext
     runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.3;
        [self.window.animator setFrame:newFrame display:YES];
    } completionHandler:^{
        Preferences *strongSelf = weakSelf;
        if (strongSelf) {
            strongSelf.bottomConstraint.active = YES;
            strongSelf.window.initialFirstResponder = preferencePane;

            [blockPrevCtrl scrollToTop];
        }
    }];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
    return @[themesPanel, stylesPanel, presentationPanel, formatPanel, voiceOverPanel, miscPanel, globalPanel];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
    return [self toolbarDefaultItemIdentifiers:toolbar];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar {
    return [self toolbarDefaultItemIdentifiers:toolbar];
}

@end
