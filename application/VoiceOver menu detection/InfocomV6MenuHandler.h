//
//  InfocomV6MenuHandler.h
//  Spatterlight
//
//  Created by Administrator on 2025-01-06.
//

#import <Foundation/Foundation.h>

#include "glkimp.h"

@class GlkController;

NS_ASSUME_NONNULL_BEGIN

@interface InfocomV6MenuHandler : NSObject

- (instancetype)initWithDelegate:(GlkController *)delegate;

- (void)handleMenuItemOfType:(InfocomV6MenuType)type index:(NSUInteger)index total:(NSUInteger)total text:(char *)buf length:(NSUInteger)len;
//- (void)describeMenu;

- (NSString *)menuLineStringWithTitle:(BOOL)useTitle index:(BOOL)useIndex total:(BOOL)useTotal instructions:(BOOL)useInstructions;
- (NSString *)constructMenuInstructionString;
- (void)updateMoveRanges:(GlkTextBufferWindow *)window;

@property (weak) GlkController *delegate;

@property NSMutableArray<NSString *> *menuItems;
@property NSUInteger numberOfItems;
@property NSString *title;
@property NSUInteger selectedLine;
@property BOOL hasDescribedMenu;

@end

NS_ASSUME_NONNULL_END
