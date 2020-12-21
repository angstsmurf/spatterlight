//
//  ZMenu.h
//  Spatterlight
//
//  Created by Administrator on 2020-11-15.
//

#import <Foundation/Foundation.h>

@class GlkController;

NS_ASSUME_NONNULL_BEGIN

@interface ZMenu : NSObject

@property GlkController *glkctl;
@property NSMutableArray *lines;
@property NSMutableArray *viewStrings;
@property NSAttributedString *attrStr;
@property NSDictionary *menuCommands;
@property NSArray *menuKeys;
@property NSUInteger selectedLine;
@property BOOL haveSpokenMenu;

- (instancetype)initWithGlkController:(GlkController *)glkctl;
- (BOOL)isMenu;
- (void)speakSelectedLine;
- (NSUInteger)findSelectedLine;

@end

NS_ASSUME_NONNULL_END
