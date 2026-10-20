//
//  ParagraphPopOver.m
//  Spatterlight
//
//  Created by Administrator on 2021-05-02.
//

#import "ParagraphPopOver.h"
#import "Preferences.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "Constants.h"

@interface ParagraphPopOver ()

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

@implementation ParagraphPopOver

- (void)refreshForStyle:(GlkStyle *)style {
    _style = style;
    _para = [style.attributeDict[NSParagraphStyleAttributeName] mutableCopy];

    [self refreshAlignmentButtons];

    [self showValue:_para.lineSpacing textField:_lineSpacingTextField stepper:_lineSpacingStepper];
    [self showValue:[style.attributeDict[NSKernAttributeName] doubleValue]
          textField:_characterSpacingTextField stepper:_characterSpacingStepper];
    [self showValue:_para.paragraphSpacingBefore textField:_spacingBeforeTextField stepper:_spacingBeforeStepper];
    [self showValue:_para.paragraphSpacing textField:_spacingAfterTextField stepper:_spacingAfterStepper];
    [self showValue:_para.headIndent textField:_indentLeadingTextField stepper:_indentLeadingStepper];
    // tailIndent is stored negative (measured from the trailing margin)
    [self showValue:-_para.tailIndent textField:_indentTrailingTextField stepper:_indentTrailingStepper];
    [self showValue:_para.firstLineHeadIndent textField:_indentFirstLineTextField stepper:_indentFirstLineStepper];
    [self showValue:_para.lineHeightMultiple textField:_lineMultipleTextField stepper:_lineMultipleStepper];
    [self showValue:[style.attributeDict[NSBaselineOffsetAttributeName] doubleValue]
          textField:_baseLineOffsetTextField stepper:_baseLineOffsetStepper];
    [self showValue:_para.maximumLineHeight textField:_maxLineHeightTextField stepper:_maxLineHeightStepper];
    [self showValue:_para.minimumLineHeight textField:_minLineHeightTextField stepper:_minLineHeightStepper];
    [self showValue:_para.hyphenationFactor textField:_hyphenationTextField stepper:_hyphenationStepper];
}

- (void)refreshAlignmentButtons {
    _btnAlignmentLeft.state = NSControlStateValueOff;
    _btnAlignmentRight.state = NSControlStateValueOff;
    _btnAlignmentCenter.state = NSControlStateValueOff;
    _btnAlignmentJustified.state = NSControlStateValueOff;

    switch (_para.alignment) {
        case NSTextAlignmentRight:
            _btnAlignmentRight.state = NSControlStateValueOn;
            break;
        case NSTextAlignmentCenter:
            _btnAlignmentCenter.state = NSControlStateValueOn;
            break;
        case NSTextAlignmentJustified:
            _btnAlignmentJustified.state = NSControlStateValueOn;
            break;
        default:
            _btnAlignmentLeft.state = NSControlStateValueOn;
            break;
    }
}

// Shows value in a text field + stepper pair, without a "-0"
- (void)showValue:(CGFloat)value textField:(NSTextField *)textField stepper:(NSStepper *)stepper {
    if (value == 0)
        value = 0;
    textField.doubleValue = value;
    stepper.doubleValue = value;
}

// Sets an NSParagraphStyle property of _para by key, shows it and saves
- (void)setParagraphValue:(CGFloat)value forKey:(NSString *)key
                textField:(NSTextField *)textField stepper:(NSStepper *)stepper {
    if ([[_para valueForKey:key] doubleValue] == value)
        return;
    [_para setValue:@(value) forKey:key];
    [self showValue:value textField:textField stepper:stepper];
    [self saveChange];
}

// Sets a non-paragraph attribute (kerning, baseline offset), shows it and saves
- (void)setAttributeValue:(CGFloat)value forKey:(NSAttributedStringKey)key
                textField:(NSTextField *)textField stepper:(NSStepper *)stepper {
    NSMutableDictionary *attDict = _style.attributeDict.mutableCopy;
    if ([attDict[key] doubleValue] == value)
        return;
    attDict[key] = @(value);
    [self showValue:value textField:textField stepper:stepper];
    [self saveNonParagraphChange:attDict];
}

- (void)saveChange {
    [self saveNonParagraphChange:_style.attributeDict.mutableCopy];
}

// Baseline offset and character spacing are not NSParagraphStyle properties,
// so the methods that change those pass their modified attribute dictionary here
- (void)saveNonParagraphChange:(NSMutableDictionary *)attDict {
    GlkStyle *style = _style;
    // Without a valid GlkStyle index we can't find the matching style in the
    // cloned theme, and index 0 would silently edit the wrong style
    NSNumber *styleNumber = style.attributeDict[@"GlkStyle"];
    NSArray *styleNames = [style testGridStyle] ? gGridStyleNames : gBufferStyleNames;
    if (!styleNumber || styleNumber.integerValue < 0 || (NSUInteger)styleNumber.integerValue >= styleNames.count) {
        NSLog(@"ParagraphPopOver: style has no valid GlkStyle index: %@", styleNumber);
        return;
    }
    NSString *styleName = styleNames[(NSUInteger)styleNumber.integerValue];
    Theme *theme = [[Preferences instance] cloneThemeIfNotEditable];
    _style = [theme valueForKey:styleName];
    style = _style;
    attDict[NSParagraphStyleAttributeName] = _para.copy;
    style.attributeDict = attDict;
    style.autogenerated = NO;
    if (style == theme.bufferNormal || style == theme.gridNormal)
        [Preferences rebuildTextAttributes];
    else
        [[NSNotificationCenter defaultCenter]
         postNotification:[NSNotification notificationWithName:kPreferencesChangedNotification object:theme]];
}

- (IBAction)changeAlignment:(id)sender {
    // Map by button rather than by tag: the NSTextAlignment values of Right
    // and Center differ between x86_64 and arm64
    NSTextAlignment alignment;
    if (sender == _btnAlignmentLeft)
        alignment = NSTextAlignmentLeft;
    else if (sender == _btnAlignmentRight)
        alignment = NSTextAlignmentRight;
    else if (sender == _btnAlignmentCenter)
        alignment = NSTextAlignmentCenter;
    else if (sender == _btnAlignmentJustified)
        alignment = NSTextAlignmentJustified;
    else
        return;
    if (alignment == _para.alignment)
        return;
    _para.alignment = alignment;
    [self saveChange];
    [self refreshAlignmentButtons];
}

- (IBAction)changeCharacterSpacing:(id)sender {
    [self setAttributeValue:[sender doubleValue] forKey:NSKernAttributeName
                  textField:_characterSpacingTextField stepper:_characterSpacingStepper];
}

- (IBAction)changeLineSpacing:(id)sender {
    [self setParagraphValue:[sender doubleValue] forKey:@"lineSpacing"
                  textField:_lineSpacingTextField stepper:_lineSpacingStepper];
}

- (IBAction)changeParagraphSpacingBefore:(id)sender {
    [self setParagraphValue:[sender doubleValue] forKey:@"paragraphSpacingBefore"
                  textField:_spacingBeforeTextField stepper:_spacingBeforeStepper];
}

- (IBAction)changeParagraphSpacingAfter:(id)sender {
    [self setParagraphValue:[sender doubleValue] forKey:@"paragraphSpacing"
                  textField:_spacingAfterTextField stepper:_spacingAfterStepper];
}

- (IBAction)changeLeadingIndent:(id)sender {
    [self setParagraphValue:[sender doubleValue] forKey:@"headIndent"
                  textField:_indentLeadingTextField stepper:_indentLeadingStepper];
}

- (IBAction)changeTrailingIndent:(id)sender {
    // Shown positive, stored negative
    CGFloat value = [sender doubleValue];
    if (_para.tailIndent == -value)
        return;
    _para.tailIndent = -value;
    [self showValue:value textField:_indentTrailingTextField stepper:_indentTrailingStepper];
    [self saveChange];
}

- (IBAction)changeFirstLineIndent:(id)sender {
    [self setParagraphValue:[sender doubleValue] forKey:@"firstLineHeadIndent"
                  textField:_indentFirstLineTextField stepper:_indentFirstLineStepper];
}

- (IBAction)changeLineMultiple:(id)sender {
    [self setParagraphValue:[sender doubleValue] forKey:@"lineHeightMultiple"
                  textField:_lineMultipleTextField stepper:_lineMultipleStepper];
}

- (IBAction)changeBaselineOffset:(id)sender {
    [self setAttributeValue:[sender doubleValue] forKey:NSBaselineOffsetAttributeName
                  textField:_baseLineOffsetTextField stepper:_baseLineOffsetStepper];
}

- (IBAction)changeMaxLineHeight:(id)sender {
    [self setParagraphValue:[sender doubleValue] forKey:@"maximumLineHeight"
                  textField:_maxLineHeightTextField stepper:_maxLineHeightStepper];
}

- (IBAction)changeMinLineHeight:(id)sender {
    [self setParagraphValue:[sender doubleValue] forKey:@"minimumLineHeight"
                  textField:_minLineHeightTextField stepper:_minLineHeightStepper];
}

- (IBAction)changeHyphenation:(id)sender {
    [self setParagraphValue:[sender floatValue] forKey:@"hyphenationFactor"
                  textField:_hyphenationTextField stepper:_hyphenationStepper];
}

@end
