//
//  CommandScriptHandler.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-11.
//

#import <Foundation/Foundation.h>

@class GlkController, GlkWindow;

typedef NS_ENUM(NSUInteger, kLastCommandType) {
    kCommandTypeLine,
    kCommandTypeChar,
};

NS_ASSUME_NONNULL_BEGIN

@interface CommandScriptHandler : NSObject

@property (weak) GlkController *glkctl;

@property (nullable) NSString *commandString;
@property (nullable) NSString *untypedCharacters;
@property (nullable) NSArray <NSValue *> *commandArray;
@property NSUInteger commandIndex;
@property NSInteger lastCommandWindow;
@property kLastCommandType lastCommandType;

- (void)startCommandScript:(NSString *)string inWindow:(GlkWindow *)win;
- (BOOL)commandScriptInPasteboard:(NSPasteboard *)pboard fromWindow:(nullable GlkWindow *)gwin;
- (void)runCommandsFromFile:(NSString *)filename;
- (void)restoreFromSaveFile:(NSString *)filename;
- (void)copyPropertiesFrom:(CommandScriptHandler *)handler;
- (nullable GlkWindow *)findGlkWindowWithInput;

- (void)sendCommandLineToWindow:(GlkWindow *)win;
- (void)sendCommandKeyPressToWindow:(GlkWindow *)win;


@end

NS_ASSUME_NONNULL_END
