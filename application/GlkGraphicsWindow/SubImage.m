//
//  SubImage.m
//  Spatterlight
//
//  Created by Administrator on 2021-04-07.
//

#import "SubImage.h"

@implementation SubImage

+ (BOOL) supportsSecureCoding {
    return YES;
}

+ (instancetype)imageWithLabel:(NSString *)label parent:(id)parent frame:(NSRect)frameRect {
    SubImage *image = [SubImage new];
    image.accessibilityLabel = label;
    image.accessibilityParent = parent;
    image.frameRect = frameRect;
    return image;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _uuid = [[NSUUID UUID] UUIDString];
        self.accessibilityRole = NSAccessibilityImageRole;
    }
    return self;
}

// The parent is not archived; the window that decodes us sets it.
- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [self init];
    if (self) {
        self.accessibilityLabel = [decoder decodeObjectOfClass:[NSString class] forKey:@"accessibilityLabel"];
        _frameRect = [decoder decodeRectForKey:@"frameRect"];
        // The rotor finds images by uuid, so keep the fresh one if none was saved.
        NSString *uuid = [decoder decodeObjectOfClass:[NSString class] forKey:@"UUID"];
        if (uuid.length)
            _uuid = uuid;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeObject:self.accessibilityLabel forKey:@"accessibilityLabel"];
    [encoder encodeObject:_uuid forKey:@"UUID"];
    [encoder encodeRect:_frameRect forKey:@"frameRect"];
}

- (void)setAccessibilityFocused:(BOOL)accessibilityFocused {
    [super setAccessibilityFocused:accessibilityFocused];
    if (!accessibilityFocused)
        return;
    NSView *parent = self.accessibilityParent;
    if (!parent)
        return;
    self.accessibilityFrame = NSAccessibilityFrameInView(parent, _frameRect);
    [parent becomeFirstResponder];
}

@end
