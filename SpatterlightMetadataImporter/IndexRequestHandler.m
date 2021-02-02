//
//  IndexRequestHandler.m
//  SpatterlightMetadataImporter
//
//  Created by Administrator on 2020-12-29.
//

#import "IndexRequestHandler.h"

@implementation IndexRequestHandler

- (void)searchableIndex:(CSSearchableIndex *)searchableIndex reindexAllSearchableItemsWithAcknowledgementHandler:(void (^)(void))acknowledgementHandler  API_AVAILABLE(macos(10.11)){
    // Reindex all data with the provided index

    acknowledgementHandler();
}

- (void)searchableIndex:(CSSearchableIndex *)searchableIndex reindexSearchableItemsWithIdentifiers:(NSArray <NSString *> *)identifiers acknowledgementHandler:(void (^)(void))acknowledgementHandler  API_AVAILABLE(macos(10.11)){
    // Reindex any items with the given identifiers and the provided index

    acknowledgementHandler();
}

- (NSData *)dataForSearchableIndex:(CSSearchableIndex *)searchableIndex itemIdentifier:(NSString*)itemIdentifier typeIdentifier:(NSString*)typeIdentifier error:(out NSError **)outError  API_AVAILABLE(macos(10.11)){
    // Replace with to return data representation of requested type from item identifier

    NSData *data = nil;
    return data;
}


- (NSURL *)fileURLForSearchableIndex:(CSSearchableIndex *)searchableIndex itemIdentifier:(NSString *)itemIdentifier typeIdentifier:(NSString *)typeIdentifier inPlace:(BOOL)inPlace error:(out NSError ** __nullable)outError  API_AVAILABLE(macos(10.11)){
    // Replace with to return file url based on requested type from item identifier

    NSURL *url = nil;
    return url;
}

@end
