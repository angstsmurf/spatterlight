#import "main.h"

#include <unistd.h>

@implementation GlkEvent

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
    
    if (ch < 256)
	return ch;
    
    return keycode_Unknown;
}

- (id) initPrefsEvent
{
    [super init];
    type = EVTPREFS;
    val1 = [Preferences graphicsEnabled];
    val2 = [Preferences soundEnabled];
    return self;
}

- (id) initCharEvent: (unsigned)v forWindow: (int)name
{
    [super init];
    type = EVTKEY;
    win = name;
    val1 = v;
    return self;
}

- (id) initMouseEvent: (NSPoint)v forWindow: (int)name
{
    [super init];
    type = EVTMOUSE;
    win = name;
    val1 = v.x;
    val2 = v.y;
    return self;
}

- (id) initLineEvent: (NSString*)v forWindow: (int)name
{
    [super init];
    type = EVTLINE;
    ln = [v copy];
    win = name;
    val1 = [ln length];
    return self;
}

- (id) initTimerEvent
{
    [super init];
    type = EVTTIMER;
    return self;
}

- (id) initArrangeWidth: (int)aw height: (int)ah;
{
    [super init];
    type = EVTARRANGE;
    val1 = aw;
    val2 = ah;
    return self;
}

- (void) dealloc
{
    [ln release];
    [super dealloc];
}

- (void) writeEvent: (int)fd
{
    struct message reply;
    char buf[4096];
   
    if (ln)
    {
	reply.len = [ln length] * 2;
	if (reply.len > sizeof buf)
	    reply.len = sizeof buf;
	[ln getCharacters: (unsigned short*)buf range: NSMakeRange(0, reply.len/2)];
    }
    else
    {
	reply.len = 0;
    }
    
    reply.cmd = type;
    reply.a1 = win;
    reply.a2 = val1;
    reply.a3 = val2;
    
    if (type == EVTARRANGE || type == EVTPREFS)
    {
	reply.a1 = val1;
	reply.a2 = val2;
	reply.a3 = [Preferences bufferMargins];
	reply.a4 = [Preferences gridMargins];
	reply.a5 = [Preferences charWidth] * 256.0;
	reply.a6 = [Preferences lineHeight] * 256.0;
    }
    
    write(fd, &reply, sizeof(struct message));
    if (reply.len)
	write(fd, buf, reply.len);
}

@end
