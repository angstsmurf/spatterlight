//
//  InputHistory.h
//  Spatterlight
//
//  Created by Administrator on 2020-11-25.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// The command history of a window: the last HISTORYLEN - 1 lines entered,
// plus the line being edited when the player started to travel back.
// A pure model: the window puts the returned lines into its input and
// speaks them.
@interface InputHistory : NSObject <NSSecureCoding>

// Move to the present, i.e. the line being edited.
- (void)reset;
- (void)saveHistory:(NSString *)line;
// The previous line, or nil at the oldest one. Leaving the present stores
// currentInput, so that travelForward can bring it back.
- (nullable NSString *)travelBackward:(nullable NSString *)currentInput;
// The next line, or nil at the present.
- (nullable NSString *)travelForward;

@property (readonly, getter=isEmpty) BOOL empty;
@property (readonly) NSUInteger count;

@end

NS_ASSUME_NONNULL_END
