//
//  Preferences+StylesEditor.m
//  Spatterlight
//

#import "Preferences+StylesEditor.h"

#import "GlkStyle.h"
#import "NSColor+integer.h"
#import "Theme.h"

#import <objc/runtime.h>

@implementation ThemeColorWell
@end

@implementation ThemePreviewView

static const CGFloat previewTileWidth = 33, previewTileHeight = 24, previewGap = 4, previewTextSize = 15;

+ (NSSize)previewSize {
    return NSMakeSize(previewTileWidth * 2 + previewGap, previewTileHeight);
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        self.toolTip = NSLocalizedString(@"How games using this theme look with Dark mode off, and with Dark mode on.", nil);
        self.accessibilityElement = YES;
        self.accessibilityRole = NSAccessibilityImageRole;
        self.accessibilityLabel = NSLocalizedString(@"Theme preview", nil);
    }
    return self;
}

- (BOOL)isFlipped {
    return YES;
}

static NSRect sampleInkBounds(NSFont *font) {
    return [@"Aa" boundingRectWithSize:NSZeroSize
                               options:NSStringDrawingUsesDeviceMetrics
                            attributes:@{ NSFontAttributeName: font }];
}

// The theme's proportional font, sized so the letters of "Aa" are as tall as
// in system text of previewTextSize. Measured by the letters rather than the
// point size, as pixel fonts draw small glyphs on tall lines.
- (NSFont *)sampleFont {
    static CGFloat targetHeight;
    if (!targetHeight)
        targetHeight = NSHeight(sampleInkBounds([NSFont systemFontOfSize:previewTextSize]));
    NSFont *font = self.theme.bufferNormal.font ?: [NSFont systemFontOfSize:previewTextSize];
    font = [NSFont fontWithDescriptor:font.fontDescriptor size:previewTextSize] ?: font;
    NSRect ink = sampleInkBounds(font);
    if (NSIsEmptyRect(ink))
        return font;
    CGFloat scale = MIN(targetHeight / NSHeight(ink), (previewTileWidth - 7) / NSWidth(ink));
    return [NSFont fontWithDescriptor:font.fontDescriptor size:previewTextSize * scale] ?: font;
}

- (void)drawTileInRect:(NSRect)rect paper:(NSColor *)paper ink:(NSColor *)ink font:(NSFont *)font {
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 0.5, 0.5) xRadius:3 yRadius:3];
    [(paper ?: NSColor.textBackgroundColor) setFill];
    [path fill];
    [[NSColor.labelColor colorWithAlphaComponent:0.25] setStroke];
    path.lineWidth = 1;
    [path stroke];
    NSDictionary *attributes = @{ NSFontAttributeName: font,
                                  NSForegroundColorAttributeName: ink ?: NSColor.textColor };
    // Without NSStringDrawingUsesLineFragmentOrigin, the origin is the baseline.
    NSRect letters = sampleInkBounds(font);
    NSPoint baseline = NSMakePoint(round(NSMidX(rect) - NSMidX(letters)),
                                   round(NSMidY(rect) + NSMidY(letters)));
    [@"Aa" drawWithRect:(NSRect){ baseline, NSZeroSize } options:0 attributes:attributes];
}

- (void)drawRect:(NSRect)dirtyRect {
    Theme *theme = self.theme;
    if (!theme)
        return;
    GlkStyle *normal = theme.bufferNormal;
    NSColor *lightPaper = theme.bufferBackground;
    NSColor *darkPaper = theme.bufferBackgroundDark ?: lightPaper;
    NSColor *lightInk = normal.color;
    NSColor *darkInk = normal.darkColor ?: lightInk;
    NSFont *font = [self sampleFont];
    [self drawTileInRect:NSMakeRect(0, 0, previewTileWidth, previewTileHeight) paper:lightPaper ink:lightInk font:font];
    [self drawTileInRect:NSMakeRect(previewTileWidth + previewGap, 0, previewTileWidth, previewTileHeight) paper:darkPaper ink:darkInk font:font];
}

@end

@interface Preferences ()
@property (strong) IBOutlet NSView *stylesView;
/// The Glk Styles tab controls that only have instance variable outlets.
- (NSDictionary<NSString *, NSView *> *)stylesTabControls;
@end

@interface Preferences (AppearanceSwitch)
@property (nonatomic, readonly) NSButton *darkModeSwitch;
@end

static NSTextField *smallLabel(NSString *text, NSTextAlignment alignment) {
    NSTextField *label = [NSTextField labelWithString:text];
    label.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
    label.alignment = alignment;
    return label;
}

static BOOL isThemeKey(NSString *key) {
    return [key hasSuffix:@"Background"];
}

@implementation Preferences (StylesEditor)

- (NSMutableArray<ThemeColorWell *> *)themeColorWells {
    NSMutableArray *wells = objc_getAssociatedObject(self, @selector(themeColorWells));
    if (!wells) {
        wells = [NSMutableArray new];
        objc_setAssociatedObject(self, @selector(themeColorWells), wells, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return wells;
}

- (NSMutableArray<NSView *> *)lightColumnViews {
    NSMutableArray *views = objc_getAssociatedObject(self, @selector(lightColumnViews));
    if (!views) {
        views = [NSMutableArray new];
        objc_setAssociatedObject(self, @selector(lightColumnViews), views, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return views;
}

- (NSMutableArray<NSView *> *)darkColumnViews {
    NSMutableArray *views = objc_getAssociatedObject(self, @selector(darkColumnViews));
    if (!views) {
        views = [NSMutableArray new];
        objc_setAssociatedObject(self, @selector(darkColumnViews), views, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return views;
}

#pragma mark Reading and writing colors

- (NSString *)styleOrThemeKeyForKey:(NSString *)key {
    if ([key isEqualToString:@"selectedStyle"])
        return [self selectedStyleName];
    return key;
}

- (NSColor *)themeColorForKey:(NSString *)key dark:(BOOL)dark {
    Theme *theme = [Preferences currentTheme];
    key = [self styleOrThemeKeyForKey:key];
    if (isThemeKey(key)) {
        NSColor *light = [theme valueForKey:key];
        if (!dark)
            return light;
        return [theme valueForKey:[key stringByAppendingString:@"Dark"]] ?: light;
    }
    GlkStyle *style = [theme valueForKey:key];
    return dark ? (style.darkColor ?: style.color) : style.color;
}

- (void)setThemeColor:(NSColor *)color forKey:(NSString *)key dark:(BOOL)dark {
    NSColor *oldColor = [self themeColorForKey:key dark:dark];
    if (oldColor && [oldColor isEqualToColor:color])
        return;

    Theme *theme = self.cloneThemeIfNotEditable;
    key = [self styleOrThemeKeyForKey:key];
    if (isThemeKey(key)) {
        [theme setValue:color forKey:dark ? [key stringByAppendingString:@"Dark"] : key];
    } else {
        GlkStyle *style = [theme valueForKey:key];
        if (dark)
            style.darkColor = color;
        else
            style.color = color;
        style.autogenerated = NO;
    }
    // Autogenerated styles follow the Normal style of their window type
    [Preferences rebuildTextAttributes];
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
    [self syncStylesEditor];
}

#pragma mark Controls

- (ThemeColorWell *)themeColorWellForKey:(NSString *)key dark:(BOOL)dark {
    ThemeColorWell *well = [[ThemeColorWell alloc] initWithFrame:NSMakeRect(0, 0, 40, 22)];
    well.key = key;
    well.dark = dark;
    well.target = self;
    well.action = @selector(changeThemeColor:);
    NSString *what;
    if ([key isEqualToString:@"gridNormal"])
        what = NSLocalizedString(@"Monospaced ink", nil);
    else if ([key isEqualToString:@"gridBackground"])
        what = NSLocalizedString(@"Monospaced paper", nil);
    else if ([key isEqualToString:@"bufferNormal"])
        what = NSLocalizedString(@"Proportional ink", nil);
    else if ([key isEqualToString:@"bufferBackground"])
        what = NSLocalizedString(@"Proportional paper", nil);
    else
        what = NSLocalizedString(@"Style ink", nil);
    well.accessibilityLabel = [NSString stringWithFormat:@"%@, %@", what,
                               dark ? NSLocalizedString(@"dark colors", nil) : NSLocalizedString(@"light colors", nil)];
    well.toolTip = well.accessibilityLabel;
    [self.themeColorWells addObject:well];
    return well;
}

- (IBAction)changeThemeColor:(ThemeColorWell *)sender {
    if (sender.color)
        [self setThemeColor:sender.color forKey:sender.key dark:sender.dark];
}

// The tag is 0 for monospaced, 1 for proportional, plus 2 for the dark colors.
- (NSButton *)swapButtonWithTag:(NSInteger)tag {
    NSButton *button = [NSButton buttonWithTitle:@"⇆" target:self action:@selector(swapThemeColors:)];
    if (@available(macOS 11.0, *)) {
        button.image = [NSImage imageWithSystemSymbolName:@"arrow.left.arrow.right" accessibilityDescription:NSLocalizedString(@"Swap ink and paper colors", nil)];
        button.imagePosition = NSImageOnly;
    }
    button.bordered = NO;
    button.tag = tag;
    button.toolTip = NSLocalizedString(@"Swap ink and paper colors.", nil);
    return button;
}

- (IBAction)swapThemeColors:(NSButton *)sender {
    BOOL dark = (sender.tag & 2) != 0;
    NSString *ink = (sender.tag & 1) ? @"bufferNormal" : @"gridNormal";
    NSString *paper = (sender.tag & 1) ? @"bufferBackground" : @"gridBackground";
    NSColor *inkColor = [self themeColorForKey:ink dark:dark];
    NSColor *paperColor = [self themeColorForKey:paper dark:dark];
    if (!inkColor || !paperColor)
        return;
    [self setThemeColor:paperColor forKey:ink dark:dark];
    [self setThemeColor:inkColor forKey:paper dark:dark];
}

#pragma mark Copying between the light and dark colors

static NSArray<NSString *> *themeColorKeys(void) {
    return @[ @"bufferBackground", @"gridBackground", @"borderColor", @"spacingColor", @"bufLinkColor", @"gridLinkColor" ];
}

- (NSTextField *)colorsExplanation {
    NSTextField *label = objc_getAssociatedObject(self, @selector(colorsExplanation));
    if (!label) {
        label = [NSTextField wrappingLabelWithString:@""];
        label.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
        label.textColor = NSColor.secondaryLabelColor;
        label.alignment = NSTextAlignmentCenter;
        objc_setAssociatedObject(self, @selector(colorsExplanation), label, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return label;
}

- (NSMutableArray<NSButton *> *)copyColorsButtons {
    NSMutableArray *buttons = objc_getAssociatedObject(self, @selector(copyColorsButtons));
    if (!buttons) {
        buttons = [NSMutableArray new];
        objc_setAssociatedObject(self, @selector(copyColorsButtons), buttons, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return buttons;
}

// The tag is 1 for the button that copies into the dark colors.
- (NSButton *)copyColorsButtonToDark:(BOOL)toDark {
    NSString *title = toDark ? NSLocalizedString(@"Replace with Light", nil) : NSLocalizedString(@"Replace with Dark", nil);
    NSButton *button = [NSButton buttonWithTitle:title target:self action:@selector(copyThemeColors:)];
    button.controlSize = NSControlSizeSmall;
    button.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
    button.tag = toDark ? 1 : 0;
    button.toolTip = toDark
        ? NSLocalizedString(@"Replace every dark color in this theme, including the colors of every Glk style, with its light color.", nil)
        : NSLocalizedString(@"Replace every light color in this theme, including the colors of every Glk style, with its dark color.", nil);
    [self.copyColorsButtons addObject:button];
    return button;
}

- (BOOL)themeLightAndDarkColorsMatch:(Theme *)theme {
    for (NSString *key in themeColorKeys()) {
        NSColor *light = [theme valueForKey:key];
        NSColor *dark = [theme valueForKey:[key stringByAppendingString:@"Dark"]] ?: light;
        if (light != dark && ![light isEqualToColor:dark])
            return NO;
    }
    for (GlkStyle *style in theme.allStyles) {
        NSColor *dark = style.darkColor ?: style.color;
        if (style.color != dark && ![style.color isEqualToColor:dark])
            return NO;
    }
    return YES;
}

- (void)themeColorsDidChange:(Theme *)theme {
    [Preferences rebuildTextAttributes];
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
    [self syncStylesEditor];
}

// Replacing the colors of a built-in theme makes a copy, and leaves the
// original alone. The user's own themes are changed in place, so ask first.
- (IBAction)copyThemeColors:(NSButton *)sender {
    BOOL toDark = sender.tag == 1;
    Theme *current = [Preferences currentTheme];
    if (!current.editable) {
        [self replaceThemeColorsToDark:toDark];
        return;
    }
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = [NSString stringWithFormat:toDark
                         ? NSLocalizedString(@"Replace the dark colors of \u201c%@\u201d with its light colors?", nil)
                         : NSLocalizedString(@"Replace the light colors of \u201c%@\u201d with its dark colors?", nil), current.name];
    alert.informativeText = toDark
        ? NSLocalizedString(@"Every dark color, including the colors of every Glk style, will be replaced. Games using this theme will look the same with Dark mode on or off. You can't undo this.", nil)
        : NSLocalizedString(@"Every light color, including the colors of every Glk style, will be replaced. Games using this theme will look the same with Dark mode on or off. You can't undo this.", nil);
    NSButton *replace = [alert addButtonWithTitle:NSLocalizedString(@"Replace", nil)];
    if (@available(macOS 11.0, *))
        replace.hasDestructiveAction = YES;
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    Preferences * __weak weakSelf = self;
    [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode) {
        if (returnCode == NSAlertFirstButtonReturn)
            [weakSelf replaceThemeColorsToDark:toDark];
    }];
}

- (void)replaceThemeColorsToDark:(BOOL)toDark {
    Theme *theme = self.cloneThemeIfNotEditable;
    for (NSString *key in themeColorKeys()) {
        NSString *darkKey = [key stringByAppendingString:@"Dark"];
        NSColor *light = [theme valueForKey:key];
        NSColor *dark = [theme valueForKey:darkKey] ?: light;
        if (toDark)
            [theme setValue:light forKey:darkKey];
        else
            [theme setValue:dark forKey:key];
    }
    for (GlkStyle *style in theme.allStyles) {
        NSColor *dark = style.darkColor ?: style.color;
        if (toDark)
            style.darkColor = style.color;
        else if (dark)
            style.color = dark;
    }
    [self themeColorsDidChange:theme];
}

#pragma mark Previews in the list of themes

- (void)tableView:(NSTableView *)tableView didAddRowView:(NSTableRowView *)rowView forRow:(NSInteger)row {
    if (tableView != self.themesTable)
        return;
    NSTableCellView *cell = [rowView viewAtColumn:0];
    if (![cell isKindOfClass:[NSTableCellView class]])
        return;
    ThemePreviewView *preview = nil;
    for (NSView *view in cell.subviews)
        if ([view isKindOfClass:[ThemePreviewView class]])
            preview = (ThemePreviewView *)view;
    NSSize size = [ThemePreviewView previewSize];
    if (!preview) {
        preview = [[ThemePreviewView alloc] initWithFrame:NSMakeRect(4, 0, size.width, size.height)];
        preview.autoresizingMask = NSViewMinYMargin | NSViewMaxYMargin;
        [cell addSubview:preview];
        NSTextField *name = cell.textField;
        NSRect frame = name.frame;
        CGFloat inset = NSMaxX(preview.frame) + 6 - NSMinX(frame);
        frame.origin.x += inset;
        frame.size.width -= inset;
        frame.origin.y = floor((NSHeight(cell.bounds) - NSHeight(frame)) / 2);
        name.frame = frame;
        name.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin | NSViewMaxYMargin;
    }
    [preview setFrameOrigin:NSMakePoint(4, floor((NSHeight(cell.bounds) - size.height) / 2))];
    preview.theme = cell.objectValue;
    preview.needsDisplay = YES;
    if (row == 0)
        [self placeThemePreviewLegendOver:preview];
}

// A sun over the tiles with Dark mode off, and a moon over the ones with it on.
- (void)placeThemePreviewLegendOver:(ThemePreviewView *)preview {
    if (@available(macOS 11.0, *)) {
        NSScrollView *scrollView = self.themesTable.enclosingScrollView;
        NSView *tab = scrollView.superview;
        if (!tab)
            return;
        NSMutableArray<NSImageView *> *legend = objc_getAssociatedObject(self, @selector(placeThemePreviewLegendOver:));
        if (!legend) {
            legend = [NSMutableArray new];
            NSArray *symbols = @[ @[ @"sun.max", NSLocalizedString(@"Dark mode off", nil) ],
                                  @[ @"moon", NSLocalizedString(@"Dark mode on", nil) ] ];
            for (NSArray *symbol in symbols) {
                NSImage *image = [NSImage imageWithSystemSymbolName:symbol[0] accessibilityDescription:symbol[1]];
                NSImageView *imageView = [NSImageView imageViewWithImage:image];
                imageView.contentTintColor = NSColor.secondaryLabelColor;
                imageView.symbolConfiguration = [NSImageSymbolConfiguration configurationWithPointSize:13 weight:NSFontWeightRegular];
                imageView.toolTip = symbol[1];
                imageView.autoresizingMask = NSViewMinYMargin;
                [tab addSubview:imageView];
                [legend addObject:imageView];
            }
            objc_setAssociatedObject(self, @selector(placeThemePreviewLegendOver:), legend, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        }
        NSRect previewInTab = [preview convertRect:preview.bounds toView:tab];
        CGFloat tileWidth = (NSWidth(previewInTab) - previewGap) / 2;
        for (NSUInteger i = 0; i < legend.count; i++) {
            CGFloat midX = NSMinX(previewInTab) + tileWidth / 2 + i * (tileWidth + previewGap);
            legend[i].frame = NSMakeRect(round(midX - 9), NSMaxY(scrollView.frame) + 2, 18, 18);
        }
    }
}

- (void)refreshThemePreviews {
    [self.themesTable enumerateAvailableRowViewsUsingBlock:^(NSTableRowView *rowView, NSInteger row) {
        NSTableCellView *cell = [rowView viewAtColumn:0];
        for (NSView *view in cell.subviews)
            if ([view isKindOfClass:[ThemePreviewView class]]) {
                ((ThemePreviewView *)view).theme = cell.objectValue;
                view.needsDisplay = YES;
            }
    }];
}

- (NSTextField *)formLabel:(NSString *)text {
    NSTextField *label = [NSTextField labelWithString:text];
    label.alignment = NSTextAlignmentRight;
    return label;
}

#pragma mark Layout

- (void)configureStylesEditor {
    NSDictionary<NSString *, NSView *> *controls = self.stylesTabControls;
    NSView *stylesView = self.stylesView;
    NSView *content = controls[@"gridFont"].superview;
    NSBox *box = (NSBox *)content.superview;
    NSView *themeHeader = self.stylesHeader.controlView;

    NSButton *marginGear = nil, *paragraphGear = nil;
    for (NSView *view in content.subviews) {
        if (![view isKindOfClass:[NSButton class]])
            continue;
        SEL action = ((NSButton *)view).action;
        if (action == @selector(showMarginPopover:))
            marginGear = (NSButton *)view;
        else if (action == @selector(showParagraphPopOver:))
            paragraphGear = (NSButton *)view;
    }
    NSPopUpButton *windowTypePopup = self.windowTypePopup;
    NSPopUpButton *styleNamePopup = self.styleNamePopup;
    for (NSView *view in [content.subviews copy])
        [view removeFromSuperview];

    NSButton *darkModeSwitch = self.darkModeSwitch;
    darkModeSwitch.title = NSLocalizedString(@"Dark mode", nil);
    darkModeSwitch.toolTip = NSLocalizedString(@"Show Spatterlight in dark mode. When this matches the system setting, Spatterlight follows the system.", nil);

    // Rows of label and control under the box: the design's rows, then the
    // ones from the xib.
    NSMutableArray<NSArray<NSView *> *> *themeRows = [[self colorsChoiceThemeRows] mutableCopy];
    NSView *hintsCheckbox = controls[@"enableStyles"];
    NSView *overridesButton = self.btnOverwriteStyles;
    NSView *hintsLabel = nil, *overridesLabel = nil;
    for (NSView *view in stylesView.subviews) {
        if (![view isKindOfClass:[NSTextField class]] || view == themeHeader)
            continue;
        if (fabs(NSMidY(view.frame) - NSMidY(hintsCheckbox.frame)) < 8)
            hintsLabel = view;
        else if (fabs(NSMidY(view.frame) - NSMidY(overridesButton.frame)) < 12)
            overridesLabel = view;
    }
    [themeRows addObject:@[ hintsLabel, hintsCheckbox ]];
    [themeRows addObject:@[ overridesLabel, overridesButton ]];
    NSArray<NSArray<NSView *> *> *appRows = [self colorsChoiceAppRows];
    NSArray<NSArray<NSView *> *> *headerRows = [self colorsChoiceThemeHeaderRows];

    const CGFloat boxHeight = 214, explanationHeight = 16, labelRight = 194, controlX = 198, bottomPad = 20;
    CGFloat appRowsHeight = 0;
    for (NSArray *row in appRows)
        appRowsHeight += NSHeight(((NSView *)row[1]).frame) + 8;
    CGFloat headerRowsHeight = 0;
    for (NSArray *row in headerRows)
        headerRowsHeight += NSHeight(((NSView *)row[1]).frame) + 8;
    CGFloat themeRowsHeight = 0;
    for (NSArray *row in themeRows)
        themeRowsHeight += MAX(NSHeight(((NSView *)row[1]).frame), 20) + 8;

    CGFloat headerTop = 38 + appRowsHeight + (appRows.count ? 10 : 0);
    CGFloat boxTop = headerTop + 26 + headerRowsHeight + (headerRows.count ? 4 : 0);
    CGFloat tabHeight = boxTop + boxHeight + 6 + explanationHeight + 8 + themeRowsHeight + bottomPad;
    NSRect tabFrame = stylesView.frame;
    tabFrame.size.height = tabHeight;
    stylesView.frame = tabFrame;
    CGFloat tabWidth = NSWidth(tabFrame);

    void (^placeInTab)(NSView *, CGFloat, CGFloat, CGFloat, CGFloat) = ^(NSView *view, CGFloat x, CGFloat top, CGFloat width, CGFloat viewHeight) {
        view.translatesAutoresizingMaskIntoConstraints = YES;
        view.autoresizingMask = NSViewNotSizable;
        view.frame = NSMakeRect(x, tabHeight - top - viewHeight, width, viewHeight);
        if (view.superview != stylesView)
            [stylesView addSubview:view];
    };
    void (^placeRows)(NSArray<NSArray<NSView *> *> *, CGFloat) = ^(NSArray<NSArray<NSView *> *> *rows, CGFloat top) {
        for (NSArray<NSView *> *row in rows) {
            NSView *label = row[0], *control = row[1];
            CGFloat rowHeight = MAX(NSHeight(control.frame), 20);
            [(NSTextField *)label sizeToFit];
            placeInTab(label, labelRight - NSWidth(label.frame), top + (rowHeight - NSHeight(label.frame)) / 2, NSWidth(label.frame), NSHeight(label.frame));
            if ([control isKindOfClass:[NSPopUpButton class]] || ([control isKindOfClass:[NSButton class]] && ((NSButton *)control).bezelStyle != NSBezelStyleRounded))
                [(NSControl *)control sizeToFit];
            placeInTab(control, controlX - ([control isKindOfClass:[NSPopUpButton class]] ? 3 : 0), top, NSWidth(control.frame), NSHeight(control.frame));
            top += rowHeight + 8;
        }
    };

    placeRows(appRows, 38);
    placeInTab(themeHeader, 0, headerTop, tabWidth, 17);
    placeRows(headerRows, headerTop + 26);
    box.frame = NSMakeRect(12, tabHeight - boxTop - boxHeight, tabWidth - 24, boxHeight);
    NSTextField *explanation = self.colorsExplanation;
    placeInTab(explanation, 24, boxTop + boxHeight + 6, tabWidth - 48, explanationHeight);
    placeRows(themeRows, boxTop + boxHeight + 6 + explanationHeight + 8);

    CGFloat height = NSHeight(content.bounds);
    void (^place)(NSView *, CGFloat, CGFloat, CGFloat, CGFloat) = ^(NSView *view, CGFloat x, CGFloat top, CGFloat width, CGFloat viewHeight) {
        view.translatesAutoresizingMaskIntoConstraints = YES;
        view.autoresizingMask = NSViewNotSizable;
        view.frame = NSMakeRect(x, height - top - viewHeight, width, viewHeight);
        [content addSubview:view];
    };

    const CGFloat labelX = 4, labelWidth = 80;
    const CGFloat fontX = 88, fontWidth = 140;
    const CGFloat wellWidth = 40, swapWidth = 12;
    const CGFloat lightX = 238, darkX = 364;
    const CGFloat groupWidth = wellWidth * 2 + swapWidth + 4;
    const CGFloat marginX = 468, gearX = 504;

    NSMutableArray<NSView *> *lightViews = self.lightColumnViews;
    NSMutableArray<NSView *> *darkViews = self.darkColumnViews;

    // Column headers
    for (NSNumber *dark in @[ @NO, @YES ]) {
        CGFloat x = dark.boolValue ? darkX : lightX;
        NSMutableArray *views = dark.boolValue ? darkViews : lightViews;
        NSTextField *title = smallLabel(dark.boolValue ? NSLocalizedString(@"Dark colors", nil) : NSLocalizedString(@"Light colors", nil), NSTextAlignmentCenter);
        title.font = [NSFont boldSystemFontOfSize:NSFont.smallSystemFontSize];
        place(title, x - 8, 10, groupWidth + 16, 16);
        NSBox *rule = [[NSBox alloc] init];
        rule.boxType = NSBoxSeparator;
        place(rule, x, 28, groupWidth, 1);
        NSButton *copyButton = [self copyColorsButtonToDark:dark.boolValue];
        [copyButton sizeToFit];
        place(copyButton, x + (groupWidth - NSWidth(copyButton.frame)) / 2, 33, NSWidth(copyButton.frame), NSHeight(copyButton.frame));
        NSTextField *ink = smallLabel(NSLocalizedString(@"Ink", nil), NSTextAlignmentCenter);
        NSTextField *paper = smallLabel(NSLocalizedString(@"Paper", nil), NSTextAlignmentCenter);
        place(ink, x - 4, 56, wellWidth + 8, 14);
        place(paper, x + wellWidth + swapWidth + 4 - 4, 56, wellWidth + 8, 14);
        [views addObjectsFromArray:@[ title, rule, copyButton, ink, paper ]];
    }
    place(smallLabel(NSLocalizedString(@"Font", nil), NSTextAlignmentCenter), fontX, 56, fontWidth, 14);
    place(smallLabel(NSLocalizedString(@"Margin", nil), NSTextAlignmentCenter), marginX - 8, 56, 50, 14);

    // Monospaced and proportional rows
    NSArray *rows = @[ @[ NSLocalizedString(@"Monospaced:", nil), controls[@"gridFont"], @"gridNormal", @"gridBackground", controls[@"gridMargin"], @0 ],
                       @[ NSLocalizedString(@"Proportional:", nil), controls[@"bufferFont"], @"bufferNormal", @"bufferBackground", controls[@"bufferMargin"], @1 ] ];
    CGFloat top = 76;
    for (NSArray *row in rows) {
        place(smallLabel(row[0], NSTextAlignmentRight), labelX, top + 5, labelWidth, 14);
        place(row[1], fontX, top, fontWidth, 24);
        for (NSNumber *dark in @[ @NO, @YES ]) {
            CGFloat x = dark.boolValue ? darkX : lightX;
            NSMutableArray *views = dark.boolValue ? darkViews : lightViews;
            NSView *inkWell = [self themeColorWellForKey:row[2] dark:dark.boolValue];
            NSView *swap = [self swapButtonWithTag:[row[5] integerValue] + (dark.boolValue ? 2 : 0)];
            NSView *paperWell = [self themeColorWellForKey:row[3] dark:dark.boolValue];
            place(inkWell, x, top + 1, wellWidth, 22);
            place(swap, x + wellWidth + 2, top + 5, swapWidth, 14);
            place(paperWell, x + wellWidth + swapWidth + 4, top + 1, wellWidth, 22);
            [views addObjectsFromArray:@[ inkWell, swap, paperWell ]];
        }
        place(row[4], marginX, top + 3, 34, 18);
        top += 30;
    }
    place(marginGear, gearX, 79, 27, 19);

    NSBox *separator = [[NSBox alloc] init];
    separator.boxType = NSBoxSeparator;
    place(separator, 0, top + 4, NSWidth(content.bounds), 5);

    // The style chosen in the popups
    top += 16;
    place(windowTypePopup, labelX + 6, top - 3, 76, 30);
    place(styleNamePopup, labelX + 84, top - 3, 110, 30);
    top += 28;
    place(controls[@"anyFont"], fontX, top, fontWidth, 24);
    NSView *lightStyleWell = [self themeColorWellForKey:@"selectedStyle" dark:NO];
    NSView *darkStyleWell = [self themeColorWellForKey:@"selectedStyle" dark:YES];
    place(lightStyleWell, lightX, top + 1, wellWidth, 22);
    place(darkStyleWell, darkX, top + 1, wellWidth, 22);
    [lightViews addObject:lightStyleWell];
    [darkViews addObject:darkStyleWell];
    place(paragraphGear, gearX, top + 2, 27, 19);

    [self configureColorsChoiceOnOtherTabs];
    [self syncStylesEditor];
}

- (void)syncStylesEditor {
    for (ThemeColorWell *well in self.themeColorWells) {
        NSColor *color = [self themeColorForKey:well.key dark:well.dark];
        if (color && !(well.color && [well.color isEqualToColor:color]))
            well.color = color;
    }
    BOOL colorsMatch = [self themeLightAndDarkColorsMatch:[Preferences currentTheme]];
    for (NSButton *button in self.copyColorsButtons)
        button.enabled = !colorsMatch;
    self.colorsExplanation.stringValue = colorsMatch
        ? NSLocalizedString(@"The light and dark colors are the same, so games look the same with Dark mode on or off.", nil)
        : NSLocalizedString(@"Games show the light colors when Dark mode is off, and the dark colors when it's on.", nil);
    [self refreshThemePreviews];
    BOOL showLight = [self stylesEditorShowsLightColors];
    BOOL showDark = [self stylesEditorShowsDarkColors];
    for (NSView *view in self.lightColumnViews)
        view.hidden = !showLight;
    // With only the dark colors, they take the place of the light colors
    NSNumber *darkHome = objc_getAssociatedObject(self, @selector(syncStylesEditor));
    if (!darkHome) {
        darkHome = @(NSMinX(self.darkColumnViews.firstObject.frame));
        objc_setAssociatedObject(self, @selector(syncStylesEditor), darkHome, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    CGFloat lightHome = NSMinX(self.lightColumnViews.firstObject.frame);
    CGFloat shift = (showDark && !showLight) ? lightHome - darkHome.doubleValue : 0;
    CGFloat delta = shift - (NSMinX(self.darkColumnViews.firstObject.frame) - darkHome.doubleValue);
    for (NSView *view in self.darkColumnViews) {
        view.hidden = !showDark;
        if (delta)
            [view setFrameOrigin:NSMakePoint(NSMinX(view.frame) + delta, NSMinY(view.frame))];
    }
    [self syncColorsChoice];
}

@end
