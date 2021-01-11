//
//  InputTextField.h
//  Spatterlight
//
//  Created by Administrator on 2020-10-24.
//

#import <Foundation/Foundation.h>

#import "GlkWindow.h"

NS_ASSUME_NONNULL_BEGIN

@interface MyFieldEditor : NSTextView

@end

@interface MyTextFormatter : NSFormatter

- (instancetype)initWithMaxLength:(NSUInteger)maxLength;

@property NSUInteger maxLength;

@end

@interface InputTextField: NSTextField <NSTextViewDelegate>

- (instancetype)initWithFrame:(NSRect)frame maxLength:(NSUInteger)maxLength;

@property (nullable, readonly) MyFieldEditor *fieldEditor;

@end

NS_ASSUME_NONNULL_END
