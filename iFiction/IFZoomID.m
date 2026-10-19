//
//  IFZoomID.m
//  Spatterlight
//
//  Created by Administrator on 2021-07-16.
//

#import <Foundation/Foundation.h>

#import "DebugLog.h"

#import "IFZoomID.h"

@implementation IFZoomID

- (instancetype)initWithElements:(NSArray<NSXMLElement *> *)elements {
    self = [super init];
    if (self) {
        self.format = @"zcode";
        NSMutableSet<NSString *> *ifidSet = [[NSMutableSet alloc] initWithCapacity:elements.count];
        for (NSXMLElement *element in elements) {
            NSString *ifidString = [IFZoomID ifidStringFromElement:element];
            if (ifidString.length)
                [ifidSet addObject:ifidString];
        }
        self.ifids = ifidSet;
    }
    return self;
}

+ (NSString *)ifidStringFromElement:(NSXMLElement *)element {
    NSString *serial = nil;
    NSString *checksum = nil;
    NSInteger release = 0;

    for (NSXMLNode *node in element.children) {
        if ([node.name isEqualToString:@"format"]) {
            if (![node.stringValue.lowercaseString isEqualToString:@"zcode"]) {
                NSLog(@"IFZoomID only supports Z-code format");
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
