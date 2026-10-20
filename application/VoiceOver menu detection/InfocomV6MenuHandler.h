//
//  InfocomV6MenuHandler.h
//  Spatterlight
//
//  Created by Administrator on 2025-01-06.
//

#import <Foundation/Foundation.h>

#import "VoiceOverMenu.h"

#include "glkimp.h"

@class GlkController, GlkTextBufferWindow;

NS_ASSUME_NONNULL_BEGIN

@interface InfocomV6MenuHandler : NSObject <VoiceOverMenu, VoiceOverHelper>

- (instancetype)initWithGlkController:(GlkController *)glkctl;

- (void)handleMenuItemOfType:(InfocomV6MenuType)type index:(NSUInteger)index total:(NSUInteger)total text:(char *)buf length:(NSUInteger)len;

- (void)updateMoveRanges:(GlkTextBufferWindow *)window;

@property (weak, nullable) GlkController *glkctl;

@property (readonly) NSUInteger selectedLine;
@property BOOL haveSpokenMenu;

@end

NS_ASSUME_NONNULL_END
