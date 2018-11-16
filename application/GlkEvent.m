#import "main.h"

#include <unistd.h>

@implementation GlkEvent

- (instancetype) init
{
    return [self initPrefsEvent];
}

unsigned chartokeycode(unsigned ch)
{
    switch (ch)
    {
        case '\n':
        case '\r':
            return keycode_Return;
        case '\t':
            return keycode_Tab;
        case 8:
        case 127:
        case NSDeleteFunctionKey:
            return keycode_Delete;
        case NSUpArrowFunctionKey:
            return keycode_Up;
        case NSDownArrowFunctionKey:
            return keycode_Down;
        case NSLeftArrowFunctionKey:
            return keycode_Left;
        case NSRightArrowFunctionKey:
            return keycode_Right;
        case NSPageDownFunctionKey:
            return keycode_PageDown;
        case NSPageUpFunctionKey:
            return keycode_PageUp;
        case NSHomeFunctionKey:
            return keycode_Home;
        case NSEndFunctionKey:
            return keycode_End;
        case NSF1FunctionKey: return keycode_Func1;
        case NSF2FunctionKey: return keycode_Func2;
        case NSF3FunctionKey: return keycode_Func3;
        case NSF4FunctionKey: return keycode_Func4;
        case NSF5FunctionKey: return keycode_Func5;
        case NSF6FunctionKey: return keycode_Func6;
        case NSF7FunctionKey: return keycode_Func7;
        case NSF8FunctionKey: return keycode_Func8;
        case NSF9FunctionKey: return keycode_Func9;
        case NSF10FunctionKey: return keycode_Func10;
        case NSF11FunctionKey: return keycode_Func11;
        case NSF12FunctionKey: return keycode_Func12;
        case '\e':
            return keycode_Escape;
    }

    if (ch < 0x200000)
        return ch;

    return keycode_Unknown;
}

- (instancetype) initPrefsEvent
{
    self = [super init];
    if (self)
    {
        type = EVTPREFS;
        val1 = [Preferences graphicsEnabled];
        val2 = [Preferences soundEnabled];
    }
    return self;
}

- (instancetype) initCharEvent: (unsigned)v forWindow: (NSInteger)name
{
    self = [super init];
    if (self)
    {
        type = EVTKEY;
        win = name;
        val1 = v;
    }
    return self;
}

- (instancetype) initMouseEvent: (NSPoint)v forWindow: (NSInteger)name
{
    self = [super init];
    if (self)
    {
        type = EVTMOUSE;
        win = name;
        val1 = v.x;
        val2 = v.y;
    }
    return self;
}

- (instancetype) initLineEvent: (NSString*)v forWindow: (NSInteger)name
{
    self = [super init];
    if (self)
    {
        type = EVTLINE;
        ln = [v copy];
        win = name;
        val1 = (unsigned int)[ln length];
    }
    return self;
}

- (instancetype) initTimerEvent
{
    self = [super init];
    if (self)
        type = EVTTIMER;
    return self;
}

- (instancetype) initArrangeWidth: (NSInteger)aw height: (NSInteger)ah;
{
    self = [super init];
    if (self)
    {
        type = EVTARRANGE;
        val1 = aw;
        val2 = ah;
    }
    return self;
}

- (instancetype) initSoundNotify: (NSInteger)notify withSound:(NSInteger)sound
{
	self = [super init];
	if (self)
	{
		type = EVTSOUND;
		val1 = sound;
		val2 = notify;
	}
	return self;
}

- (instancetype) initVolumeNotify: (NSInteger)notify
{
	self = [super init];
	if (self)
	{
		type = EVTVOLUME;
		val2 = notify;
	}
	return self;
}

- (instancetype) initLinkEvent: (NSUInteger)linkid forWindow: (NSInteger)name
{
	self = [super init];
	if (self)
	{
		type = EVTHYPER;
		win = name;
		val1 = linkid;
	}
	return self;
}

- (void) writeEvent: (NSInteger)fd
{
    struct message reply;
    char buf[4096];

    if (ln)
    {
        reply.len = (int)([ln length] * 2);
        if (reply.len > sizeof buf)
            reply.len = sizeof buf;
        [ln getCharacters: (unsigned short*)buf range: NSMakeRange(0, reply.len/2)];
    }
    else
    {
        reply.len = 0;
    }

    reply.cmd = (int)type;
    reply.a1 = (int)win;
    reply.a2 = (int)val1;
    reply.a3 = (int)val2;

    if (type == EVTARRANGE || type == EVTPREFS)
    {
        reply.a1 = (int)val1;
        reply.a2 = (int)val2;
        reply.a3 = (int)[Preferences bufferMargins];
        reply.a4 = (int)[Preferences gridMargins];
        reply.a5 = [Preferences charWidth] * 256.0;
        reply.a6 = [Preferences lineHeight] * 256.0;
    }

    write((int)fd, &reply, sizeof(struct message));
    if (reply.len)
        write((int)fd, buf, reply.len);
}

- (NSInteger) type
{
    return self->type;
}

@end
