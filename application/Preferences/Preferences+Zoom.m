#import "PreferencesPrivate.h"

#import "Constants.h"
#import "GlkStyle.h"
#import "Theme.h"

@implementation Preferences (Zoom)

#pragma mark Zoom

+ (void)zoomIn {
    self.zoomDirection = ZOOMRESET;
    NSFont *gridroman = self.theme.gridNormal.font;
    if (gridroman.pointSize < 200) {
        self.zoomDirection = ZOOMIN;
        [self scale:(gridroman.pointSize + 1) / gridroman.pointSize];
    }
}

+ (void)zoomOut {
    self.zoomDirection = ZOOMRESET;
    NSFont *gridroman = self.theme.gridNormal.font;
    if (gridroman.pointSize > 6) {
        self.zoomDirection = ZOOMOUT;
        [self scale:(gridroman.pointSize - 1) / gridroman.pointSize];
    }
}

+ (void)zoomToActualSize {
    self.zoomDirection = ZOOMRESET;

    CGFloat scale = 12;
    Theme *parent = self.theme.defaultParent;
    while (parent.defaultParent)
        parent = parent.defaultParent;

    if (parent)
        scale = parent.gridNormal.font.pointSize;

    if (scale < 6)
        scale = 12;

    [self scale:scale / self.theme.gridNormal.font.pointSize];
}

+ (void)scale:(CGFloat)scalefactor {
    if (scalefactor < 0)
        scalefactor = fabs(scalefactor);

    [Preferences.instance cloneThemeIfNotEditable];

    for (GlkStyle *style in self.theme.allStyles) {
        [style updateAttributes:^(NSMutableDictionary *dict) {
            NSFont *font = dict[NSFontAttributeName];
            CGFloat fontSize = font.pointSize * scalefactor;
            if (fontSize > 0) {
                NSFont *scaled = [NSFont fontWithDescriptor:font.fontDescriptor
                                                       size:fontSize];
                if (scaled)
                    dict[NSFontAttributeName] = scaled;
            }
            if (dict[NSKernAttributeName]) {
                CGFloat newValue = ((NSNumber *)dict[NSKernAttributeName]).doubleValue * scalefactor;
                dict[NSKernAttributeName] = @(newValue);
            }
            if (dict[NSBaselineOffsetAttributeName]) {
                CGFloat newValue = ((NSNumber *)dict[NSBaselineOffsetAttributeName]).doubleValue * scalefactor;
                dict[NSBaselineOffsetAttributeName] = @(newValue);
            }
            NSMutableParagraphStyle *para = ((NSParagraphStyle *)dict[NSParagraphStyleAttributeName]).mutableCopy;
            if (!para)
                return;
            para.lineSpacing = para.lineSpacing * scalefactor;
            para.paragraphSpacing = para.paragraphSpacing * scalefactor;
            para.paragraphSpacingBefore = para.paragraphSpacingBefore * scalefactor;
            para.headIndent = para.headIndent * scalefactor;
            para.tailIndent = para.tailIndent * scalefactor;
            para.firstLineHeadIndent = para.firstLineHeadIndent * scalefactor;
            para.maximumLineHeight = para.maximumLineHeight * scalefactor;
            para.minimumLineHeight = para.minimumLineHeight * scalefactor;

            dict[NSParagraphStyleAttributeName] = para;
        }];
    }

    [Preferences rebuildTextAttributes];

    /* send notification that default size has changed -- resize all windows */
    [[NSNotificationCenter defaultCenter]
     postNotificationName:kDefaultSizeChangedNotification
     object:self.theme];
}

- (void)updatePanelAfterZoom {
    btnGridFont.title = fontToString(self.theme.gridNormal.font);
    btnBufferFont.title = fontToString(self.theme.bufferNormal.font);
    btnAnyFont.title = fontToString([self selectedStyle].font);
}

@end
