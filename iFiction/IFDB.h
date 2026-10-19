//
//  IFDB.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

//! The namespace of the <ifdb> element in IFDB's iFiction feed
extern NSString * const kIFDBXMLNamespace;

@interface IFDB : NSObject

//! The IFDB page for a game, by TUID if there is one, otherwise by IFID.
//! With iFiction set, the game's iFiction record instead of the web page.
//! Nil if both are empty.
+ (nullable NSURL *)viewGameURLForTUID:(nullable NSString *)tuid IFID:(nullable NSString *)ifid iFiction:(BOOL)iFiction;

- (instancetype)initWithXMLElement:(NSXMLElement *)element;

@property (nullable) NSString *tuid;
@property (nullable) NSString *coverArtURL;
@property (nullable) NSString *starRating;

@end

NS_ASSUME_NONNULL_END
