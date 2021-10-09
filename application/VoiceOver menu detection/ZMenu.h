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

@property(weak) GlkController *glkctl;
@property NSMutableArray<NSValue *> *lines;
@property NSMutableArray<NSString *> *viewStrings;
@property NSAttributedString *attrStr;
@property NSDictionary<NSString *, NSString *> *menuCommands;
@property NSArray<NSString *> *menuKeys;
@property NSUInteger selectedLine;
@property BOOL haveSpokenMenu;
@property BOOL recheckNeeded;
@property BOOL isTads3;
@property NSString *lastSpokenString;
@property NSUInteger lastNumberOfItems;

- (instancetype)initWithGlkController:(GlkController *)glkctl;
- (BOOL)isMenu;
- (void)speakSelectedLine;
- (void)deferredSpeakSelectedLine:(id)sender;
- (NSUInteger)findSelectedLine;

@end

NS_ASSUME_NONNULL_END
