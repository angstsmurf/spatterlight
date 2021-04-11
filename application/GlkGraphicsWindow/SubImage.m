//
//  SubImage.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-07.
//

#import "SubImage.h"
#import "GlkGraphicsWindow.h"

@implementation SubImage

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self.accessibilityLabel = [decoder decodeObjectOfClass:[NSString class] forKey:@"accessibilityLabel"];
    _frameRect = [decoder decodeRectForKey:@"frameRect"];

    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:self.accessibilityLabel forKey:@"accessibilityLabel"];
    [encoder encodeRect:_frameRect forKey:@"frameRect"];
}

- (void)setAccessibilityFocused:(BOOL)accessibilityFocused {
    if (accessibilityFocused) {
        NSRect bounds = NSAccessibilityFrameInView(self.accessibilityParent, _frameRect);
        self.accessibilityFrame = bounds;
        GlkGraphicsWindow *parent = (GlkGraphicsWindow *)self.accessibilityParent;
        if (parent) {
            [parent becomeFirstResponder];
        }
    }
}

@end
