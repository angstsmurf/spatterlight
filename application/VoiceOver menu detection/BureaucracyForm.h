//
//  BureaucracyForm.h
//  Spatterlight
//
//  Created by Administrator on 2020-12-22.
//

#import <Foundation/Foundation.h>

@class GlkController;

NS_ASSUME_NONNULL_BEGIN

@interface BureaucracyForm : NSObject

@property(weak) GlkController *glkctl;
@property NSAttributedString *attrStr;
@property NSArray<NSValue *> *fields;
@property NSArray<NSString *> *fieldstrings;
@property NSString *titlestring;
@property NSUInteger selectedField;
@property NSUInteger lastField;
@property NSUInteger lastCharacterPos;
@property NSRange infoFieldRange;
@property NSString *lastInfoString;

@property BOOL haveSpokenForm;
@property BOOL dontSpeakField;
@property BOOL haveSpokenInstructions;
@property BOOL speakingError;
@property BOOL didNotMove;

- (instancetype)initWithGlkController:(GlkController *)glkctl;

@property (NS_NONATOMIC_IOSONLY, readonly) NSUInteger findCurrentField;

@property (NS_NONATOMIC_IOSONLY, getter=isForm, readonly) BOOL form;
- (void)movedFromField;
- (void)speakCurrentField;
- (void)deferredSpeakCurrentField:(id)sender;
- (void)speakError;
- (NSString *)fieldStringWithTitle:(BOOL)useTitle andIndex:(BOOL)useIndex andTotal:(BOOL)useTotal;

@end

NS_ASSUME_NONNULL_END
