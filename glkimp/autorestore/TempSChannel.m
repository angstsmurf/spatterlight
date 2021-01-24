/*  TempSChannel.m: Temporary sound channel objc class
 for serializing, inspired by IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import "TempSChannel.h"

@implementation TempSChannel

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype) initWithCStruct:(channel_t *)chan {

    self = [super init];

    if (self) {
        _rock = chan->rock;
        _tag = chan->tag;
        _peer = chan->peer;
        _next = _prev = 0;

        if (chan->next)
            _next = (chan->next)->tag;
        if (chan->prev)
            _prev = (chan->prev)->tag;
    }
    return self;
}

- (id) initWithCoder:(NSCoder *)decoder {

    _rock = [decoder decodeInt32ForKey:@"rock"];
    _tag = [decoder decodeInt32ForKey:@"tag"];
    _peer = [decoder decodeInt32ForKey:@"peer"];

    _prev = [decoder decodeInt32ForKey:@"prev"];
    _next = [decoder decodeInt32ForKey:@"next"];

    return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInt32:_rock forKey:@"rock"];
    [encoder encodeInt32:_tag forKey:@"tag"];
    [encoder encodeInt32:_peer forKey:@"peer"];

    [encoder encodeInt32:_prev forKey:@"prev"];
    [encoder encodeInt32:_next forKey:@"next"];

}

- (void) copyToCStruct:(channel_t *)chan {
    chan->tag = _tag;
    chan->rock = _rock;
    chan->peer = _peer;
}

@end
