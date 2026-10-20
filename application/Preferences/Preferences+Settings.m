#include "glk.h"

#import "PreferencesPrivate.h"

#import "Constants.h"
#import "DummyTextView.h"
#import "GlkStyle.h"
#import "NotificationBezel.h"
#import "NSColor+integer.h"
#import "ParagraphPopOver.h"
#import "TableViewController.h"
#import "TableViewController+TableDelegate.h"
#import "TableViewController+LibraryManagement.h"
#import "Theme.h"

#import "DebugLog.h"

typedef NS_ENUM(NSUInteger, kSettingStore) {
    kThemeSetting,
    kDefaultsSetting
};

// Controls that store one value under one key and do nothing else: a check
// box whether it is on, a pop-up button its selected tag. setUpSimpleSettings
// makes changeSimpleSetting: their action, and updateSimpleSettings shows the
// stored values. Settings with side effects or a different mapping (styles,
// space format, autosave, VoiceOver delay, missing-file check) keep their own
// actions.
static const struct {
    __unsafe_unretained NSString *control; // outlet name
    __unsafe_unretained NSString *key;
    kSettingStore store;
} simpleSettings[] = {
    // Presentation
    { @"btnSmartQuotes", @"smartQuotes", kThemeSetting },
    { @"btnEnableGraphics", @"doGraphics", kThemeSetting },
    { @"btnEnableSound", @"doSound", kThemeSetting },
    { @"btnSmoothScroll", @"smoothScroll", kThemeSetting },
    { @"coverImagePopup", @"coverArtStyle", kThemeSetting },

    // Details
    { @"zterpMenu", @"zMachineTerp", kThemeSetting },
    { @"bZArrowsMenu", @"bZTerminator", kThemeSetting },
    { @"quoteBoxCheckBox", @"quoteBox", kThemeSetting },
    { @"zMachineNoErrWinCheckbox", @"zMachineNoErrWin", kThemeSetting },
    { @"comprehendGraphicsPopup", @"comprehendGraphicsType", kThemeSetting },
    { @"z6GraphicsPopup", @"z6GraphicsType", kThemeSetting },
    { @"z6ColorizeCheckBox", @"z6Colorize1Bit", kThemeSetting },
    { @"palettePopup", @"sAPalette", kThemeSetting },
    { @"inventoryPopup", @"sAInventory", kThemeSetting },
    { @"delaysCheckbox", @"sADelays", kThemeSetting },
    { @"slowDrawCheckbox", @"slowDrawing", kThemeSetting },
    { @"scottAdamsFlickerCheckbox", @"flicker", kThemeSetting },

    // VoiceOver
    { @"btnVOSpeakCommands", @"vOSpeakCommand", kThemeSetting },
    { @"vOMenuButton", @"vOSpeakMenu", kThemeSetting },
    { @"vOImagesButton", @"vOSpeakImages", kThemeSetting },

    // Misc
    { @"btnAutosaveOnTimer", @"autosaveOnTimer", kThemeSetting },
    { @"errorHandlingPopup", @"errorHandling", kThemeSetting },
    { @"btnDeterminism", @"determinism", kThemeSetting },
    { @"btnNoHacks", @"nohacks", kThemeSetting },

    // Global. A static table cannot use the Constants.h key names, so
    // PreferencesSimpleSettingsTests checks that these match them.
    { @"imageReplacePopup", @"ImageReplacement", kDefaultsSetting },
    { @"saveInGameDirCheckbox", @"SaveInGameDirectory", kDefaultsSetting },
    { @"btnShowBezels", @"ShowBezels", kDefaultsSetting },
    { @"libraryAtStartCheckbox", @"ShowLibrary", kDefaultsSetting },
    { @"addToLibraryCheckbox", @"AddToLibrary", kDefaultsSetting },
};

static const NSUInteger simpleSettingsCount = sizeof(simpleSettings) / sizeof(simpleSettings[0]);

@implementation Preferences (Settings)

#pragma mark Simple settings

+ (NSDictionary<NSString *, NSString *> *)simpleSettingKeys {
    NSMutableDictionary *keys = [NSMutableDictionary dictionaryWithCapacity:simpleSettingsCount];
    for (NSUInteger i = 0; i < simpleSettingsCount; i++)
        keys[simpleSettings[i].control] = simpleSettings[i].key;
    return keys;
}

- (void)setUpSimpleSettings {
    for (NSUInteger i = 0; i < simpleSettingsCount; i++) {
        NSControl *control = [self valueForKey:simpleSettings[i].control];
        control.target = self;
        control.action = @selector(changeSimpleSetting:);
    }
}

- (void)updateSimpleSettings {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    for (NSUInteger i = 0; i < simpleSettingsCount; i++) {
        NSString *key = simpleSettings[i].key;
        id value = (simpleSettings[i].store == kThemeSetting) ? [self.theme valueForKey:key] : [defaults objectForKey:key];
        NSControl *control = [self valueForKey:simpleSettings[i].control];
        if ([control isKindOfClass:[NSPopUpButton class]])
            [(NSPopUpButton *)control selectItemWithTag:[value integerValue]];
        else
            ((NSButton *)control).state = [value boolValue] ? NSControlStateValueOn : NSControlStateValueOff;
    }
}

- (IBAction)changeSimpleSetting:(NSControl *)sender {
    for (NSUInteger i = 0; i < simpleSettingsCount; i++) {
        if ([self valueForKey:simpleSettings[i].control] != sender)
            continue;
        NSNumber *value;
        if ([sender isKindOfClass:[NSPopUpButton class]])
            value = @(((NSPopUpButton *)sender).selectedTag);
        else
            value = @(((NSButton *)sender).state == NSControlStateValueOn);
        if (simpleSettings[i].store == kThemeSetting)
            [self changeThemeValue:value forKey:simpleSettings[i].key];
        else
            [[NSUserDefaults standardUserDefaults] setObject:value forKey:simpleSettings[i].key];
        return;
    }
}

#pragma mark User actions

// Sets the theme attribute, on a clone if the theme is not editable, unless
// it already has this value. Returns the theme changed, or nil.
- (nullable Theme *)changeThemeValue:(id)value forKey:(NSString *)attribute {
    if ([[self.theme valueForKey:attribute] isEqual:value])
        return nil;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    [themeToChange setValue:value forKey:attribute];
    return themeToChange;
}

- (void)changeBooleanAttribute:(NSString *)attribute fromButton:(NSButton *)button {
    [self changeThemeValue:@(button.state) forKey:attribute];
}

- (void)changeDefaultsBool:(NSString *)key fromButton:(NSButton *)button {
    [[NSUserDefaults standardUserDefaults] setBool:(button.state == NSControlStateValueOn) forKey:key];
}

- (IBAction)changeDefaultSize:(id)sender {
    if (sender != txtCols && sender != txtRows)
        return;
    int32_t value = MAX(5, MIN(200, [sender intValue]));
    [sender setIntValue:value];
    if (sender == txtCols ? self.theme.defaultCols == value : self.theme.defaultRows == value)
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    if (sender == txtCols)
        themeToChange.defaultCols = value;
    else
        themeToChange.defaultRows = value;
    /* send notification that default size has changed -- resize all windows */
    NSNotification *notification = [NSNotification notificationWithName:kDefaultSizeChangedNotification object:themeToChange];
    [[NSNotificationCenter defaultCenter]
     postNotification:notification];
}

- (void)changeBorderColor:(NSColor *)color {
    if ([color isEqualToColor:self.theme.borderColor])
        return;

    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.borderColor = color;
}

- (IBAction)changeMargin:(id)sender  {
    int32_t val = [sender intValue];
    Theme *themeToChange;

    if (sender == txtGridMargin) {
        if (self.theme.gridMarginX == val)
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        themeToChange.gridMarginX = val;
        themeToChange.gridMarginY = val;
    } else if (sender == txtBufferMargin) {
        if (self.theme.bufferMarginX == val)
            return;
        themeToChange = [self cloneThemeIfNotEditable];
        themeToChange.bufferMarginX = val;
        themeToChange.bufferMarginY = val;
    } else return;

    [Preferences rebuildTextAttributes];
}

- (IBAction)changeSpaceFormatting:(id)sender {
    [self changeBooleanAttribute:@"spaceFormat" fromButton:sender];
}

- (IBAction)changeEnableStyles:(id)sender {
    [self changeBooleanAttribute:@"doStyles" fromButton:sender];
    [Preferences rebuildTextAttributes];
}

- (IBAction)changeStylePopup:(id)sender {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSInteger windowType = _windowTypePopup.selectedTag;
    [defaults setInteger:windowType forKey:kSelectedGlkWindowTypeKey];
    [defaults setInteger:_styleNamePopup.selectedTag forKey:kSelectedStyleKey];
    GlkStyle *selectedStyle = [self selectedStyle];
    clrAnyFg.color = selectedStyle.color;
    btnAnyFont.title = fontToString(selectedStyle.font);
    selectedFontButton = btnAnyFont;
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    [self.dummyTextView updateTextWithAttributes:selectedStyle.attributeDict];
    NSMutableDictionary *convertedAttributes = selectedStyle.attributeDict.mutableCopy;

    convertedAttributes[@"NSDocumentBackgroundColor"] = (windowType == wintype_TextGrid) ?
    self.theme.gridBackground : self.theme.bufferBackground;
    [fontManager setSelectedFont:selectedStyle.font isMultiple:NO];
    [fontManager setSelectedAttributes:convertedAttributes isMultiple:NO];
}

- (IBAction)changeUnderlineLinks:(id)sender {
    Theme *themeToChange;
    NSInteger windowType = (sender == _btnUnderlineLinksGrid) ? wintype_TextGrid : wintype_TextBuffer;
    NSUnderlineStyle selectedStyle = (((NSButton *)sender).state == NSControlStateValueOn) ? NSUnderlineStyleSingle : NSUnderlineStyleNone;
    switch (windowType) {
        case wintype_TextGrid:
            if (self.theme.gridLinkStyle == selectedStyle)
                return;
            themeToChange = [self cloneThemeIfNotEditable];
            themeToChange.gridLinkStyle = (int32_t)selectedStyle;
            break;
        case wintype_TextBuffer:
            if (self.theme.bufLinkStyle == selectedStyle)
                return;
            themeToChange = [self cloneThemeIfNotEditable];
            themeToChange.bufLinkStyle = (int32_t)selectedStyle;
            break;
        default:
            NSLog(@"Unhandled hyperlink window type");
            break;
    }
}

#pragma mark Margin Popover

- (IBAction)showMarginPopover:(id)sender {
    _marginHorizontalGridTextField.integerValue = self.theme.gridMarginX;
    _marginHorizontalGridStepper.integerValue = self.theme.gridMarginX;
    
    _marginVerticalGridTextField.integerValue = self.theme.gridMarginY;
    _marginVerticalGridStepper.integerValue = self.theme.gridMarginY;

    _marginHorizontalBufferTextField.integerValue = self.theme.bufferMarginX;
    _marginHorizontalBufferStepper.integerValue = self.theme.bufferMarginX;

    _marginVerticalBufferTextField.integerValue = self.theme.bufferMarginY;
    _marginVerticalBufferStepper.integerValue = self.theme.bufferMarginY;

    [_marginsPopover showRelativeToRect:[sender bounds] ofView:sender preferredEdge:NSMaxYEdge];
}

// Shared body of the four margins popover actions
- (void)changeMarginAttribute:(NSString *)attribute fromControl:(id)sender
                    textField:(NSTextField *)textField stepper:(NSStepper *)stepper {
    if (((NSNumber *)[self.theme valueForKey:attribute]).integerValue == [sender integerValue])
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    [themeToChange setValue:@([sender intValue]) forKey:attribute];
    NSInteger value = ((NSNumber *)[themeToChange valueForKey:attribute]).integerValue;
    textField.integerValue = value;
    stepper.integerValue = value;
}

- (IBAction)changeGridHorizontalMargin:(id)sender {
    [self changeMarginAttribute:@"gridMarginX" fromControl:sender
                      textField:_marginHorizontalGridTextField stepper:_marginHorizontalGridStepper];
}

- (IBAction)changeGridVerticalMargin:(id)sender {
    [self changeMarginAttribute:@"gridMarginY" fromControl:sender
                      textField:_marginVerticalGridTextField stepper:_marginVerticalGridStepper];
}

- (IBAction)changeBufferHorizontalMargin:(id)sender {
    [self changeMarginAttribute:@"bufferMarginX" fromControl:sender
                      textField:_marginHorizontalBufferTextField stepper:_marginHorizontalBufferStepper];
}

- (IBAction)changeBufferVerticalMargin:(id)sender {
    [self changeMarginAttribute:@"bufferMarginY" fromControl:sender
                      textField:_marginVerticalBufferTextField stepper:_marginVerticalBufferStepper];
}

- (IBAction)showParagraphPopOver:(id)sender {
    if (!_paragraphPopover)
        _paragraphPopover = [[ParagraphPopOver alloc] initWithNibName:@"ParagraphPopOver" bundle:nil];

    [_paragraphPopover presentViewController:_paragraphPopover asPopoverRelativeToRect:[sender frame] ofView:[sender superview] preferredEdge:NSMaxYEdge behavior:NSPopoverBehaviorTransient];
    [_paragraphPopover refreshForStyle:[self selectedStyle]];
}

#pragma mark VoiceOver menu

- (IBAction)changeVODelaySlider:(id)sender {
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    _vODelaySlider.doubleValue = [sender doubleValue];
    _vODelayTextField.doubleValue = [sender doubleValue];
    themeToChange.vOHackDelay = [sender doubleValue];
    _vODelaySlider.accessibilityValueDescription = [self secondsAccessibilityString];
}

- (IBAction)changeVODelayCheckbox:(id)sender {
    [self changeBooleanAttribute:@"vODelayOn" fromButton:sender];
    _vODelaySlider.enabled = (_vODelayCheckbox.state == NSControlStateValueOn);
    _vODelayTextField.enabled = _vODelaySlider.enabled;
}

#pragma mark ZCode menu

// Plays the selected sound and returns the name to store, mapping Big Sur
// sound names to their Catalina equivalents when only the latter exist.
// Falls back to the menu title if the map misses.
- (NSString *)playBeepFromMenu:(NSPopUpButton *)sender {
    NSString *title = sender.titleOfSelectedItem;
    NSSound *sound = [NSSound soundNamed:title];
    if (@available(macOS 11, *)) {
        NSString *catalinaTitle = bigSurSoundsToCatalina[title];
        if (!sound && catalinaTitle) {
            title = catalinaTitle;
            sound = [NSSound soundNamed:title];
        }
    }
    if (sound) {
        [sound stop];
        [sound play];
    }
    return title;
}

- (IBAction)changeBeepHighMenu:(id)sender {
    NSString *title = [self playBeepFromMenu:sender];
    if (title)
        [self changeThemeValue:title forKey:@"beepHigh"];
}

- (IBAction)changeBeepLowMenu:(id)sender {
    NSString *title = [self playBeepFromMenu:sender];
    if (title)
        [self changeThemeValue:title forKey:@"beepLow"];
}

- (IBAction)changeZVersion:(id)sender {
    if ([sender stringValue].length == 0) {
        _zVersionTextField.stringValue = self.theme.zMachineLetter;
        return;
    }
    [self changeThemeValue:[sender stringValue] forKey:@"zMachineLetter"];
}

- (IBAction)changeBZVerticalAdjustment:(id)sender {
    Theme *themeToChange = [self changeThemeValue:@([sender intValue]) forKey:@"bZAdjustment"];
    if (!themeToChange)
        return;
    _bZVerticalStepper.integerValue = themeToChange.bZAdjustment;
    _bZVerticalTextField.integerValue = themeToChange.bZAdjustment;
}

#pragma mark Misc menu

- (IBAction)resetDialogs:(NSButton *)sender {
    NSUInteger counter = 0;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray<NSString *> *keys = AlertSuppressionKeys();

    for (NSString *key in keys) {
        if ([defaults boolForKey:key])
            counter++;
        [defaults setBool:NO forKey:key];
    }

    NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:self.window.screen];
    [notification showStandardWithText:(counter == 1) ? NSLocalizedString(@"1 alert reset", nil) :
     [NSString stringWithFormat:NSLocalizedString(@"%ld alerts reset", nil), counter]];
}

// The buffer scrollback limit is a per-theme setting, stored in the theme's
// scrollbackLimit Core Data attribute. Both the text field and the stepper
// drive this. 0 means unlimited (never trim); any other value is clamped to
// the supported range. Keep both controls and the unit label in sync so the
// "= unlimited" affordance is visible.
- (IBAction)changeScrollbackLimit:(id)sender {
    NSInteger value = [sender integerValue];
    if (value <= 0)
        value = 0; // unlimited
    else if (value < 2000)
        value = 2000;
    else if (value > 2000000)
        value = 2000000;
    if (self.theme.scrollbackLimit != value) {
        Theme *themeToChange = [self cloneThemeIfNotEditable];
        themeToChange.scrollbackLimit = (int32_t)value;
    }
    _scrollbackLimitTextField.integerValue = value;
    _scrollbackLimitStepper.integerValue = value;
    _scrollbackUnitLabel.stringValue = (value == 0) ? NSLocalizedString(@"= unlimited", nil) : NSLocalizedString(@"characters", nil);
}

- (IBAction)changeAutosave:(id)sender {
    [self changeBooleanAttribute:@"autosave" fromButton:sender];
    _btnAutosaveOnTimer.enabled = ([sender state] == NSControlStateValueOn);
}

- (IBAction)changeTimerSlider:(id)sender {
    if ([sender integerValue] == 0 || self.theme.minTimer == 1000.0 / [sender floatValue]) {
        return;
    }
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.minTimer = (1000.0 / [sender floatValue]);
    _timerTextField.integerValue = [sender integerValue];
    _timerSlider.integerValue = [sender integerValue];
    _timerSlider.accessibilityValueDescription = [NSString stringWithFormat:NSLocalizedString(@"%ld per second", nil), _timerSlider.integerValue];
}

#pragma mark Global menu

- (IBAction)changeCheckMissing:(id)sender {
    [self changeDefaultsBool:kRecheckForMissingKey fromButton:sender];
    _recheckFrequencyTextfield.enabled = ([sender state] == NSControlStateValueOn);
    if (_recheckMissingCheckbox.state == NSControlStateValueOn) {
        [_libcontroller startVerifyTimer];
    } else {
        [_libcontroller stopVerifyTimer];
    }
}

- (IBAction)changeCheckFrequency:(id)sender {
    // Minutes; at least one, and store the same rounded value that is displayed
    NSInteger minutes = MAX(1, (NSInteger)round([sender doubleValue]));
    [[NSUserDefaults standardUserDefaults] setInteger:minutes forKey:kRecheckFrequencyKey];
    _recheckFrequencyTextfield.integerValue = minutes;
}

#pragma mark End of Global menu

- (IBAction)changeOverwriteStyles:(id)sender {
    if ([sender state] == NSControlStateValueOn) {
        if (![[NSUserDefaults standardUserDefaults] boolForKey:kOverwriteStylesAlertSuppressionKey]) {
            NSMutableArray *customStyles = [[NSMutableArray alloc] initWithCapacity:style_NUMSTYLES * 2];
            for (GlkStyle *style in self.theme.allStyles) {
                if (!style.autogenerated && style != self.theme.bufferNormal && style != self.theme.gridNormal) {
                    [customStyles addObject:style];
                }
            }
            if (customStyles.count) {
                [self showOverwriteStylesAlert:customStyles];
                return;
            }
        }
        [self overWriteStyles];
    }
}

- (void)showOverwriteStylesAlert:(NSArray *)styles {
    BOOL one = (styles.count == 1);
    NSString *message = one ? NSLocalizedString(@"This theme uses a custom style.", nil) :
    [NSString stringWithFormat:NSLocalizedString(@"This theme uses %ld custom styles.", nil), styles.count];
    NSString *info = one ?
        NSLocalizedString(@"Do you want to replace it with an autogenerated style?", nil) :
        NSLocalizedString(@"Do you want to replace them with autogenerated styles?", nil);

    Preferences * __weak weakSelf = self;
    [self showOkayCancelAlertWithMessage:message
                                    info:info
                          suppressionKey:kOverwriteStylesAlertSuppressionKey
                                 handler:^(BOOL okay) {
        if (okay)
            [weakSelf overWriteStyles];
    }];
}

- (void)overWriteStyles {
    self.theme = [self cloneThemeIfNotEditable];
    NSUInteger counter = 0;
    for (GlkStyle *style in self.theme.allStyles) {
        if (!style.autogenerated) {
            counter++;
            style.autogenerated = YES;
        }
    }
    if (counter) {
        [self.theme populateStyles];
        [Preferences rebuildTextAttributes];
    }
    NotificationBezel *notification = [[NotificationBezel alloc] initWithScreen:self.window.screen];
    [notification showStandardWithText:(counter == 1) ? NSLocalizedString(@"1 style changed", nil) :
     [NSString stringWithFormat:NSLocalizedString(@"%ld styles changed", nil), counter]];
}

- (IBAction)changeBorderSize:(id)sender {
    if (self.theme.border == [sender intValue])
        return;
    self.theme = [self cloneThemeIfNotEditable];
    NSInteger oldBorder = self.theme.border;
    self.theme.border = [sender intValue];
    NSInteger diff = self.theme.border - oldBorder;
    [[NSNotificationCenter defaultCenter]
     postNotificationName:kBorderChangedNotification
     object:self.theme
     userInfo:@{@"diff":@(diff)}];
}
- (IBAction)changeAutomaticBorderColor:(id)sender {
    if (self.theme.borderBehavior == ([sender state] == NSControlStateValueOff))
        return;
    Theme *themeToChange = [self cloneThemeIfNotEditable];
    themeToChange.borderBehavior = ([sender state] == NSControlStateValueOff);
    if (themeToChange.borderBehavior == kUserOverride) {
        NSColorWell *colorWell = _borderColorWell;
        themeToChange.borderColor = colorWell.color;
        if ([NSColorPanel sharedColorPanel].visible)
            [colorWell activate:YES];
    }
}

@end
