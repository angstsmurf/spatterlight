//
//  DummyController.h
//  Spatterlight
//
//  This subclass of GlkController is used for the text style preview in the preferences panel.
//  We simply redefine all public methods to do nothing, to prevent any calls meant for running games
//  from having any effect.

#import "GlkController.h"

NS_ASSUME_NONNULL_BEGIN

@interface DummyController : GlkController

@end

NS_ASSUME_NONNULL_END
