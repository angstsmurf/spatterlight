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
@property NSArray *fields;
@property NSArray *fieldstrings;
@property NSUInteger selectedField;
@property NSUInteger lastField;
@property NSUInteger lastCharacterPos;
@property NSRange infoFieldRange;
@property NSString *lastInfoString;

@property BOOL haveSpokenForm;
@property BOOL dontSpeakField;

- (instancetype)initWithGlkController:(GlkController *)glkctl;

- (NSUInteger)findCurrentField;

- (BOOL)isForm;
- (void)movedFromField;
- (void)speakCurrentField;
- (void)deferredSpeakCurrentField:(id)sender;
- (void)speakError;

@end

NS_ASSUME_NONNULL_END
