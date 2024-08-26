//
//  JourneyMenuHandler.h
//  Spatterlight
//
//  Created by Administrator on 2024-01-28.
//

#import <Foundation/Foundation.h>

#include "glkimp.h"

@class GlkController, JourneyMenuItem, GlkTextGridWindow;

NS_ASSUME_NONNULL_BEGIN

@interface JourneyMenuHandler : NSObject

- (instancetype)initWithDelegate:(GlkController *)delegate gridWindow:(GlkTextGridWindow *)gridwindow;
- (void)handleMenuItemOfType:(JourneyMenuType)type column:(NSUInteger)column line:(NSUInteger)line stopflag:(BOOL)stopflag text:(char *_Nullable)buf length:(NSUInteger)len;

- (void)sendCorrespondingMouseEventForMenuItem:(JourneyMenuItem *)item;

- (void)journeyPartyAction:(id)sender;
- (void)journeyMemberVerbAction:(id)sender;
- (void)deleteAllJourneyMenus;

@property BOOL showsJourneyMenus;
@property (weak) GlkController *delegate;
@property (weak) GlkTextGridWindow *textGridWindow;

@property NSPopUpButton *journeyPopupButton;
@property NSTextField *journeyTextField;

@property NSMutableArray<JourneyMenuItem *> *journeyDialogMenuItems;

@end

NS_ASSUME_NONNULL_END
