//
//  IFictionMetadata.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

@class IFStory;

NS_ASSUME_NONNULL_BEGIN

@interface IFictionMetadata : NSObject

//! Never empty
@property(readonly) NSArray<IFStory *> *stories;

//! The first story. Logs a warning if there is more than one, for callers
//! that expect exactly one.
@property(readonly) IFStory *firstStory;

//! Returns nil if the data is not iFiction XML or contains no usable story
- (nullable instancetype)initWithData:(NSData *)data;

//! Parses data with the options used for iFiction files throughout the app
+ (nullable NSXMLDocument *)XMLDocumentFromData:(NSData *)data;

@end

NS_ASSUME_NONNULL_END
