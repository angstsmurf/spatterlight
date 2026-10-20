//
//  InputTextField.h
//  Spatterlight
//
//  Created by Administrator on 2020-10-24.
//

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface MyFieldEditor : NSTextView

@end

// Limits the input to maxLength UTF-16 units (0 allows nothing), truncating
// pasted text. Archived with the field, as a subview of the grid window's
// text view.
@interface MyTextFormatter : NSFormatter

- (instancetype)initWithMaxLength:(NSUInteger)maxLength NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;

@property NSUInteger maxLength;

@end

@interface InputTextField: NSTextField <NSTextViewDelegate>

- (instancetype)initWithFrame:(NSRect)frame maxLength:(NSUInteger)maxLength;

@property (nullable, readonly) MyFieldEditor *fieldEditor;

@end

NS_ASSUME_NONNULL_END
