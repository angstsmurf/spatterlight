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

@end

@implementation ParagraphPopOver

- (void)refreshForStyle:(GlkStyle *)style {
    _style = style;
    _para = [style.attributeDict[NSParagraphStyleAttributeName] mutableCopy];

    [self refreshAlignmentButtons];

    _lineSpacingTextField.doubleValue = _para.lineSpacing;
    _lineSpacingStepper.doubleValue = _para.lineSpacing;

    CGFloat characterSpacing = [style.attributeDict[NSKernAttributeName] doubleValue];
    _characterSpacingTextField.doubleValue = characterSpacing;
    _characterSpacingStepper.doubleValue = characterSpacing;

    _spacingBeforeTextField.doubleValue = _para.paragraphSpacingBefore;
    _spacingBeforeStepper.doubleValue = _para.paragraphSpacingBefore;

    _spacingAfterTextField.doubleValue = _para.paragraphSpacing;
    _spacingAfterStepper.doubleValue = _para.paragraphSpacing;

    _indentLeadingTextField.doubleValue = _para.headIndent;
    _indentLeadingStepper.doubleValue = _para.headIndent;

    // To avoid -0
    if (_para.tailIndent == 0)
        _indentTrailingTextField.doubleValue = 0;
    else
        _indentTrailingTextField.doubleValue = -_para.tailIndent;

    _indentTrailingStepper.doubleValue = -_para.tailIndent;

    _indentFirstLineTextField.doubleValue = _para.firstLineHeadIndent;
    _indentFirstLineStepper.doubleValue = _para.firstLineHeadIndent;
}

- (void)refreshAlignmentButtons {
    _btnAlignmentLeft.state = NSOffState;
    _btnAlignmentRight.state = NSOffState;
    _btnAlignmentCenter.state = NSOffState;
    _btnAlignmentJustified.state = NSOffState;

    switch (_para.alignment) {
        case NSTextAlignmentRight:
            _btnAlignmentRight.state = NSOnState;
            break;
        case NSTextAlignmentCenter:
            _btnAlignmentCenter.state = NSOnState;
            break;
        case NSTextAlignmentJustified:
            _btnAlignmentJustified.state = NSOnState;
            break;
        default:
            _btnAlignmentLeft.state = NSOnState;
            break;
    }
}

- (void)saveChange {
    [self saveNonParagraphChange:_style.attributeDict.mutableCopy];
}

// Baseline offset and character spacing are not NSParagraghStyle properties,
// so the methods that change those pass their modified attribute dictionary here
- (void)saveNonParagraphChange:(NSMutableDictionary *)attDict {
    Theme *theme = [[Preferences instance] cloneThemeIfNotEditable];
    NSUInteger stylevalue = (NSUInteger)((NSNumber *)(_style.attributeDict)[@"GlkStyle"]).integerValue;
    NSString *styleName = [_style testGridStyle] ? gGridStyleNames[stylevalue] : gBufferStyleNames[stylevalue];
    _style = [theme valueForKey:styleName];
    attDict[NSParagraphStyleAttributeName] = _para.copy;
    _style.attributeDict = attDict;
    _style.autogenerated = NO;
    [[NSNotificationCenter defaultCenter]
     postNotification:[NSNotification notificationWithName:@"PreferencesChanged" object:theme]];
}

- (IBAction)changeAlignment:(id)sender {
    if ([sender tag] == _para.alignment)
        return;
    _para.alignment = [sender tag];
    [self saveChange];
    [self refreshAlignmentButtons];
}

- (IBAction)changeCharacterSpacing:(id)sender {
    NSMutableDictionary *attDict =_style.attributeDict.mutableCopy;
    if ([attDict[NSKernAttributeName] doubleValue] == [sender doubleValue]) {
        return;
    }

    attDict[NSKernAttributeName] = @([sender doubleValue]);
    _characterSpacingTextField.doubleValue = [sender doubleValue];
    _characterSpacingStepper.doubleValue = [sender doubleValue];

    [self saveNonParagraphChange:attDict];
}

- (IBAction)changeLineSpacing:(id)sender {
    if (_para.lineSpacing == [sender doubleValue]) {
        return;
    }
    _para.lineSpacing = [sender doubleValue];
    _lineSpacingTextField.doubleValue = _para.lineSpacing;
    _lineSpacingStepper.doubleValue = _para.lineSpacing;
    [self saveChange];
}

- (IBAction)changeParagraphSpacingBefore:(id)sender {
    if (_para.paragraphSpacingBefore == [sender doubleValue]) {
        return;
    }
    _para.paragraphSpacingBefore = [sender doubleValue];
    _spacingBeforeTextField.doubleValue = _para.paragraphSpacingBefore;
    _spacingBeforeStepper.doubleValue = _para.paragraphSpacingBefore;
    [self saveChange];
}

- (IBAction)changeParagraphSpacingAfter:(id)sender {
    if (_para.paragraphSpacing == [sender doubleValue]) {
        return;
    }
    _para.paragraphSpacing = [sender doubleValue];
    _spacingAfterTextField.doubleValue = _para.paragraphSpacing;
    _spacingAfterStepper.doubleValue = _para.paragraphSpacing;
    [self saveChange];
}

- (IBAction)changeLeadingIndent:(id)sender {
    if (_para.headIndent == [sender doubleValue]) {
        return;
    }
    _para.headIndent = [sender doubleValue];
    _indentLeadingTextField.doubleValue = _para.headIndent;
    _indentLeadingStepper.doubleValue = _para.headIndent;
    [self saveChange];
}

- (IBAction)changeTrailingIndent:(id)sender {
    if (_para.tailIndent == -[sender doubleValue]) {
        return;
    }
    _para.tailIndent = -[sender doubleValue];
    _indentTrailingTextField.doubleValue = -_para.tailIndent;
    _indentTrailingStepper.doubleValue = -_para.tailIndent;
    [self saveChange];
}

- (IBAction)changeFirstLineIndent:(id)sender {
    if (_para.firstLineHeadIndent == [sender doubleValue]) {
        return;
    }
    _para.firstLineHeadIndent = [sender doubleValue];
    _indentFirstLineTextField.doubleValue = _para.firstLineHeadIndent;
    _indentFirstLineStepper.doubleValue = _para.firstLineHeadIndent;
    [self saveChange];
}

- (IBAction)changeLineMultiple:(id)sender {
    if (_para.lineHeightMultiple == [sender doubleValue]) {
        return;
    }
    _para.lineHeightMultiple = [sender doubleValue];
    _lineMultipleTextField.doubleValue = _para.lineHeightMultiple;
    _lineMultipleStepper.doubleValue = _para.lineHeightMultiple;
    [self saveChange];
}

- (IBAction)changeBaselineOffset:(id)sender {
    NSMutableDictionary *attDict =_style.attributeDict.mutableCopy;
    if ([attDict[NSBaselineOffsetAttributeName] doubleValue] == [sender doubleValue]) {
        return;
    }

    attDict[NSBaselineOffsetAttributeName] = @([sender doubleValue]);
    _baseLineOffsetTextField.doubleValue = [sender doubleValue];
    _baseLineOffsetStepper.doubleValue = [sender doubleValue];

    [self saveNonParagraphChange:attDict];
}

- (IBAction)changeMaxLineHeight:(id)sender {
    if (_para.maximumLineHeight == [sender doubleValue]) {
        return;
    }
    _para.maximumLineHeight = [sender doubleValue];
    _maxLineHeightTextField.doubleValue = _para.maximumLineHeight;
    _maxLineHeightStepper.doubleValue = _para.maximumLineHeight;
    [self saveChange];
}

- (IBAction)changeMinLineHeight:(id)sender {
    if (_para.minimumLineHeight == [sender doubleValue]) {
        return;
    }
    _para.minimumLineHeight = [sender doubleValue];
    _minLineHeightTextField.doubleValue = _para.minimumLineHeight;
    _minLineHeightStepper.doubleValue = _para.minimumLineHeight;
    [self saveChange];
}

- (IBAction)changeHyphenation:(id)sender {
    if (_para.hyphenationFactor == [sender floatValue]) {
        return;
    }
    _para.hyphenationFactor = [sender floatValue];
    _hyphenationTextField.floatValue = _para.hyphenationFactor;
    _hyphenationStepper.floatValue = _para.hyphenationFactor;
    [self saveChange];
}


@end
