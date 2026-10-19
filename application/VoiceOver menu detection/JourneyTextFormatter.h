//
//  JourneyTextFormatter.h
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import "InputTextField.h"

NS_ASSUME_NONNULL_BEGIN

// Formatter for the text field of Journey's text entry dialogs. Only ASCII
// letters are allowed. A name (not elvish) is capitalised: first letter
// upper case, the rest lower case. Elvish ("elvish" stands for all text
// entry except naming the main character) also allows space and - ' . , ?
@interface JourneyTextFormatter : MyTextFormatter

- (instancetype)initWithMaxLength:(NSUInteger)maxLength elvish:(BOOL)elvish;

@property BOOL elvish;

@end

NS_ASSUME_NONNULL_END
