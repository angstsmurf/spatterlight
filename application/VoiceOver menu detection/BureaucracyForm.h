//
//  BureaucracyForm.h
//  Spatterlight
//
//  Created by Administrator on 2020-12-22.
//

#import <Foundation/Foundation.h>

#import "VoiceOverMenu.h"

@class GlkController;

NS_ASSUME_NONNULL_BEGIN

@interface BureaucracyForm : NSObject <VoiceOverHelper>

@property(weak, nullable) GlkController *glkctl;
@property NSUInteger selectedField;

@property BOOL haveSpokenForm;
@property BOOL dontSpeakField;
@property BOOL haveSpokenInstructions;

- (instancetype)initWithGlkController:(GlkController *)glkctl;

// Looks for one of the Bureaucracy forms in the grid window. If one is
// found, stores its fields and finds the current field. Returns NO if no
// form is showing.
- (BOOL)detectForm;
// Adds the current field to the grid window's moves, or moves it last if
// it is already there, so that stepping through moves reviews the form.
- (void)addCurrentFieldToMoves;
- (void)movedFromField;
- (void)speakCurrentField;
- (void)speakError;
- (NSString *)fieldStringWithTitle:(BOOL)useTitle andIndex:(BOOL)useIndex andTotal:(BOOL)useTotal;

@end

NS_ASSUME_NONNULL_END
