//
//  RotorsHandler.h
//  Spatterlight
//
//  Created by Administrator on 2021-04-05.
//

#import <Foundation/Foundation.h>

@class GlkController;

NS_ASSUME_NONNULL_BEGIN

@interface RotorHandler : NSObject <NSAccessibilityCustomRotorItemSearchDelegate> 

@property (weak) GlkController *glkctl;

- (NSAccessibilityCustomRotorItemResult *)textSearchResultForString:(NSString *)searchString fromRange:(NSRange)fromRange direction:(NSAccessibilityCustomRotorSearchDirection)direction  API_AVAILABLE(macos(10.13));

- (NSAccessibilityCustomRotorItemResult *)linksRotor:(NSAccessibilityCustomRotor *)rotor
                           resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters  API_AVAILABLE(macos(10.13));

- (NSAccessibilityCustomRotorItemResult *)glkWindowRotor:(NSAccessibilityCustomRotor *)rotor
                               resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters  API_AVAILABLE(macos(10.13));

- (NSAccessibilityCustomRotorItemResult *)imagesRotor:(NSAccessibilityCustomRotor *)rotor
                            resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters  API_AVAILABLE(macos(10.13));

- (NSAccessibilityCustomRotorItemResult *)commandHistoryRotor:(NSAccessibilityCustomRotor *)rotor
                                    resultForSearchParameters:(NSAccessibilityCustomRotorSearchParameters *)searchParameters  API_AVAILABLE(macos(10.13));

- (NSArray *)createCustomRotors;

@end

NS_ASSUME_NONNULL_END
