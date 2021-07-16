//
//  IFZoomID.m
//  Spatterlight
//
//  Created by Administrator on 2021-07-16.
//

#import "IFZoomID.h"
#import "Metadata.h"
#import "Ifid.h"
#import "Game.h"

@interface IFZoomID () {
    NSString *serial;
    NSInteger release;
    NSString *checksum;
}
@end

@implementation IFZoomID

- (instancetype)initWithElements:(NSArray<NSXMLElement *> *)elements andContext:(NSManagedObjectContext *)context {
    self = [super init];
    if (self) {

        self.context = context;
        self.metadata = nil;
        NSMutableArray *ifidArray = [[NSMutableArray alloc] initWithCapacity:elements.count];
        for (NSXMLElement *element in elements) {
            NSString *ifidString = [self ifidStringFromElement:element];
            if (ifidString.length)
                [ifidArray addObject:ifidString];
        }
        self.ifids = ifidArray;


        self.metadata = [self metadataFromIfids:ifidArray];
        if (!self.metadata) {
            return nil;
        }
        if (!self.metadata.format)
            self.metadata.format = @"zcode";
    }
    return self;
}


// Look for a game in Core Data that matches the id tags. Return an ifid string.
- (NSString *)ifidStringFromElement:(NSXMLElement *)element {
    NSEnumerator *enumChildren = [element.children objectEnumerator];
    NSXMLNode *node;
    while ((node = [enumChildren nextObject])) {
        if ([node.name compare:@"format"] == 0) {
            self.format = node.stringValue;
            if (![self.format.lowercaseString isEqualToString:@"zcode"]) {
                NSLog(@"IFZoomID only supports Z-code format");
                return nil;
            }
        } else if ([node.name compare:@"zcode"] == 0) {
            NSEnumerator *subChildren = [node.children objectEnumerator];
            NSXMLNode *subnode;
            while ((subnode = [subChildren nextObject])) {
                if ([subnode.name compare:@"serial"] == 0) {
                    serial = subnode.stringValue;
                } else if ([subnode.name compare:@"release"] == 0) {
                    release = subnode.stringValue.integerValue;
                } else if ([subnode.name compare:@"checksum"] == 0) {
                    checksum = subnode.stringValue.uppercaseString;
                }
            }
        }
    }
    if (!serial.length)
        return nil;
    Game *game = [self findGame];
    
    if (game) {
        if (!game.ifid.length)
            NSLog(@"Error! Game object in store without ifid!");
        return (game.ifid);
    } else {
        // If no matching game is found in the Core Data store, we create a new Ifid,
        // the way the Babel tool does it.
        unichar firstChar = [serial characterAtIndex:0];
        if (![serial isEqualToString:@"000000"] && isdigit(firstChar) && firstChar != '8') {
            return [NSString stringWithFormat:@"ZCODE-%ld-%@-%@", (long)release, serial, checksum];
        } else {
            return [NSString stringWithFormat:@"ZCODE-%ld-%@", (long)release, serial];
        }
    }
}

- (Game *)findGame {
    NSError *error = nil;
    NSArray *fetchedObjects;
    NSPredicate *predicate;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:self.context];

    fetchRequest.entity = entity;

    predicate =  [NSPredicate predicateWithFormat:@"detectedFormat like[c] %@ AND serialString like[c] %@ AND releaseNumber == %d", @"zcode", serial, release];

    if (checksum.length) {
        unsigned checksumInt = 0;
        NSScanner *scanner = [NSScanner scannerWithString:checksum];
        [scanner scanHexInt:&checksumInt];

        predicate = [NSCompoundPredicate andPredicateWithSubpredicates:@[predicate, [NSPredicate predicateWithFormat:@"checksum = %d", checksumInt]]];
    }

    fetchRequest.predicate = predicate;

    fetchedObjects = [self.context executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"findGame: Problem! %@",error);
        return nil;
    }

    if (fetchedObjects.count > 1) {
        NSLog(@"Found more than one matching Game entity!?");
    } else if (fetchedObjects.count == 0) {
        return nil;
    }

    return fetchedObjects.firstObject;
}

@end
