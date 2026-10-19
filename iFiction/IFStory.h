//
//  IFStory.h
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

@class IFIdentification, IFBibliographic, IFCoverDescription, IFDB;

NS_ASSUME_NONNULL_BEGIN

@interface IFStory : NSObject

@property(readonly, nullable) IFIdentification *identification;
@property(readonly, nullable) IFBibliographic *bibliographic;
@property(readonly, nullable) IFCoverDescription *coverDescription;
@property(readonly, nullable) IFDB *ifdb;

//! Returns nil if the story has neither an <identification> nor a
//! legacy Zoom <id> element
- (nullable instancetype)initWithXMLElement:(NSXMLElement *)element;

@end

NS_ASSUME_NONNULL_END
