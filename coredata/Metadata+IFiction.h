//
//  Metadata+IFiction.h
//  Spatterlight
//
//  Copies parsed iFiction data into Core Data. The parser classes in
//  iFiction/ know nothing about Metadata, so they can be used without
//  Core Data.
//

#import "Metadata.h"

@class IFStory, IFBibliographic;

NS_ASSUME_NONNULL_BEGIN

@interface Metadata (IFiction)

//! Overwrites the fields the story has values for and adds its IFIDs
- (void)addInfoFromStory:(IFStory *)story;
- (void)addInfoFromBibliographic:(IFBibliographic *)bibliographic;

//! Adds an Ifid object for each IFID the metadata doesn't already have.
//! Skips the placeholder IFIDs used by the info panel.
- (void)addIfidStrings:(NSSet<NSString *> *)ifids;

@end

NS_ASSUME_NONNULL_END
