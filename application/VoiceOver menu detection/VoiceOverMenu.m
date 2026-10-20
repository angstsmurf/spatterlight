//
//  VoiceOverMenu.m
//  Spatterlight
//

#import "VoiceOverMenu.h"

void VoiceOverHelperDetach(id<VoiceOverHelper> helper) {
    [helper cancelPendingSpeech];
    helper.glkctl = nil;
}

NSString *VoiceOverMenuIndexString(NSString *noun, NSUInteger index, NSUInteger total, BOOL useTotal) {
    NSString *string = [NSString stringWithFormat:@".\n%@ %lu", noun, index + 1];
    if (useTotal)
        string = [string stringByAppendingFormat:@" of %lu", total];
    return [string stringByAppendingString:@".\n"];
}

NSString *VoiceOverMenuTitleString(NSString *intro, NSString *title) {
    if (title.length)
        return [NSString stringWithFormat:@"%@, \"%@.\"\n", intro, title];
    return @"We are in a menu.\n";
}
