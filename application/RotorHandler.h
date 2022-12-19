//
//  RotorHandler.h
//  Spatterlight
//
//  Created by Petter Sjölund on 2021-04-05.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

@class GlkController;

NS_ASSUME_NONNULL_BEGIN

@interface RotorHandler : NSObject <NSAccessibilityCustomRotorItemSearchDelegate> 

@property (weak) GlkController *glkctl;

- (nullable NSAccessibilityCustomRotorItemResult *)textSearchResultForString:(NSString *)searchString fromRange:(NSRange)fromRange direction:(NSAccessibilityCustomRotorSearchDirection)direction;

- (nullable NSAccessibilityCustomRotorItemResult *)linksRotor:(NSAccessibilityCustomRotor *)rotor
                           resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters;

- (nullable NSAccessibilityCustomRotorItemResult *)glkWindowRotor:(NSAccessibilityCustomRotor *)rotor
                               resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters;

- (nullable NSAccessibilityCustomRotorItemResult *)imagesRotor:(NSAccessibilityCustomRotor *)rotor
                            resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters;

- (nullable NSAccessibilityCustomRotorItemResult *)commandHistoryRotor:(NSAccessibilityCustomRotor *)rotor
                                    resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters;

@property (NS_NONATOMIC_IOSONLY, readonly, copy) NSArray * _Nonnull createCustomRotors;

@end

NS_ASSUME_NONNULL_END
