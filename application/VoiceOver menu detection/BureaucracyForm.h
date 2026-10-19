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
@property NSUInteger selectedField;

@property BOOL haveSpokenForm;
@property BOOL dontSpeakField;
@property BOOL haveSpokenInstructions;

- (instancetype)initWithGlkController:(GlkController *)glkctl;

// Looks for one of the Bureaucracy forms in the grid window. If one is
// found, stores its fields, finds the current field and adds it to the
// window's moves. Returns NO if no form is showing.
- (BOOL)detectForm;
- (void)movedFromField;
- (void)speakCurrentField;
- (void)speakError;
- (NSString *)fieldStringWithTitle:(BOOL)useTitle andIndex:(BOOL)useIndex andTotal:(BOOL)useTotal;

@end

NS_ASSUME_NONNULL_END
