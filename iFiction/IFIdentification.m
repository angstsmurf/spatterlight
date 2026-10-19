//
//  IFIdentification.m
//  Adapted from Yazmin by David Schweinsberg
//  See https://github.com/dcsch/yazmin
//

#import <Foundation/Foundation.h>

#import "DebugLog.h"

#import "IFIdentification.h"

@implementation IFIdentification

- (instancetype)initWithXMLElement:(NSXMLElement *)element {
    self = [super init];
    if (self) {
        NSMutableSet<NSString *> *ifids = [[NSMutableSet alloc] init];

        _format = @"";

        for (NSXMLNode *node in element.children) {
            NSString *name = node.name;
            if ([name isEqualToString:@"ifid"]) {
                [ifids addObject:node.stringValue];
            } else if ([name isEqualToString:@"format"]) {
                _format = node.stringValue;
            } else if ([name isEqualToString:@"bafn"]) {
                _bafn = node.stringValue;
            }
        }
        if (ifids.count == 0) {
            NSLog(@"IFIdentification: no Ifids found in document! Bailing!");
            return nil;
        }
        _ifids = ifids;
    }
    return self;
}

- (instancetype)initWithZoomIDElements:(NSArray<NSXMLElement *> *)elements {
    self = [super init];
    if (self) {
        _format = @"zcode";
        NSMutableSet<NSString *> *ifidSet = [[NSMutableSet alloc] initWithCapacity:elements.count];
        for (NSXMLElement *element in elements) {
            NSString *ifidString = [IFIdentification ifidStringFromZoomIDElement:element];
            if (ifidString.length)
                [ifidSet addObject:ifidString];
        }
        _ifids = ifidSet;
    }
    return self;
}

+ (NSString *)ifidStringFromZoomIDElement:(NSXMLElement *)element {
    NSString *serial = nil;
    NSString *checksum = nil;
    NSInteger release = 0;

    for (NSXMLNode *node in element.children) {
        if ([node.name isEqualToString:@"format"]) {
            if (![node.stringValue.lowercaseString isEqualToString:@"zcode"]) {
                NSLog(@"IFIdentification: legacy Zoom IDs are only supported for Z-code");
                return nil;
            }
        } else if ([node.name isEqualToString:@"zcode"]) {
            for (NSXMLNode *subnode in node.children) {
                if ([subnode.name isEqualToString:@"serial"]) {
                    serial = subnode.stringValue;
                } else if ([subnode.name isEqualToString:@"release"]) {
                    release = subnode.stringValue.integerValue;
                } else if ([subnode.name isEqualToString:@"checksum"]) {
                    checksum = subnode.stringValue;
                }
            }
        }
    }
    if (!serial.length)
        return nil;

    // Same rules as babel/zcode.c: non-alphanumeric serial characters
    // become dashes, and the checksum is only included for serials that
    // look like dates.
    NSMutableString *cleanSerial = [serial mutableCopy];
    for (NSUInteger i = 0; i < cleanSerial.length; i++) {
        unichar c = [cleanSerial characterAtIndex:i];
        if (c > 127 || !isalnum(c))
            [cleanSerial replaceCharactersInRange:NSMakeRange(i, 1) withString:@"-"];
    }

    unichar firstChar = [cleanSerial characterAtIndex:0];
    unsigned checksumValue = 0;
    if (![cleanSerial isEqualToString:@"000000"] && firstChar < 128 && isdigit(firstChar) && firstChar != '8' &&
        checksum.length && [[NSScanner scannerWithString:checksum] scanHexInt:&checksumValue]) {
        return [NSString stringWithFormat:@"ZCODE-%ld-%@-%04X", (long)release, cleanSerial, checksumValue];
    }
    return [NSString stringWithFormat:@"ZCODE-%ld-%@", (long)release, cleanSerial];
}

@end
