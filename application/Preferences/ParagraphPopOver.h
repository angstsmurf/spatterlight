//
//  ParagraphPopOver.h
//  Spatterlight
//
//  Created by Administrator on 2021-05-02.
//

#import <Cocoa/Cocoa.h>

@class GlkStyle;

NS_ASSUME_NONNULL_BEGIN

@interface ParagraphPopOver : NSViewController

- (void)refreshForStyle:(GlkStyle *)style;

@property (weak) IBOutlet NSButton *btnAlignmentLeft;
@property (weak) IBOutlet NSButton *btnAlignmentRight;
@property (weak) IBOutlet NSButton *btnAlignmentCenter;
@property (weak) IBOutlet NSButton *btnAlignmentJustified;

- (IBAction)changeAlignment:(id)sender;

@property (weak) IBOutlet NSTextField *lineSpacingTextField;
@property (weak) IBOutlet NSStepper *lineSpacingStepper;

@property (weak) IBOutlet NSTextField *characterSpacingTextField;
@property (weak) IBOutlet NSStepper *characterSpacingStepper;

@property (weak) IBOutlet NSTextField *spacingBeforeTextField;
@property (weak) IBOutlet NSStepper *spacingBeforeStepper;

@property (weak) IBOutlet NSTextField *spacingAfterTextField;
@property (weak) IBOutlet NSStepper *spacingAfterStepper;

@property (weak) IBOutlet NSTextField *indentLeadingTextField;
@property (weak) IBOutlet NSStepper *indentLeadingStepper;

@property (weak) IBOutlet NSTextField *indentTrailingTextField;
@property (weak) IBOutlet NSStepper *indentTrailingStepper;

@property (weak) IBOutlet NSTextField *indentFirstLineTextField;
@property (weak) IBOutlet NSStepper *indentFirstLineStepper;

@property (weak) IBOutlet NSTextField *lineMultipleTextField;
@property (weak) IBOutlet NSStepper *lineMultipleStepper;

@property (weak) IBOutlet NSTextField *baseLineOffsetTextField;
@property (weak) IBOutlet NSStepper *baseLineOffsetStepper;

@property (weak) IBOutlet NSTextField *maxLineHeightTextField;
@property (weak) IBOutlet NSStepper *maxLineHeightStepper;

@property (weak) IBOutlet NSTextField *minLineHeightTextField;
@property (weak) IBOutlet NSStepper *minLineHeightStepper;

@property (weak) IBOutlet NSTextField *hyphenationTextField;
@property (weak) IBOutlet NSStepper *hyphenationStepper;

@property (weak) GlkStyle *style;
@property NSMutableParagraphStyle *para;


@end

NS_ASSUME_NONNULL_END
