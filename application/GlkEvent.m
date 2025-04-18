#import "GlkEvent.h"
#import "Theme.h"
#import "GlkStyle.h"
#import "NSColor+integer.h"

#include "glk.h"
#include "protocol.h"

#include <unistd.h>

@interface GlkEvent () <NSSecureCoding> {
    NSInteger win;
    NSString *ln;
    Theme *theme;
}
@end

@implementation GlkEvent

+ (BOOL) supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)decoder {
    self = [super init];
    if (self) {
    _type = [decoder decodeIntegerForKey:@"type"];
    win = [decoder decodeIntegerForKey:@"win"];
    _val1 = [decoder decodeIntegerForKey:@"val1"];
    _val2 = [decoder decodeIntegerForKey:@"val2"];
    ln = [decoder decodeObjectOfClass:[NSString class] forKey:@"ln"];
    _forced = [decoder decodeBoolForKey:@"forced"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInteger:_type forKey:@"type"];
    [encoder encodeInteger:win forKey:@"win"];
    [encoder encodeInteger:_val1 forKey:@"val1"];
    [encoder encodeInteger:_val2 forKey:@"val2"];
    [encoder encodeObject:ln forKey:@"ln"];
    [encoder encodeBool:_forced forKey:@"forced"];
}

unsigned chartokeycode(unsigned ch) {
    switch (ch) {
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
    case NSF1FunctionKey:
        return keycode_Func1;
    case NSF2FunctionKey:
        return keycode_Func2;
    case NSF3FunctionKey:
        return keycode_Func3;
    case NSF4FunctionKey:
        return keycode_Func4;
    case NSF5FunctionKey:
        return keycode_Func5;
    case NSF6FunctionKey:
        return keycode_Func6;
    case NSF7FunctionKey:
        return keycode_Func7;
    case NSF8FunctionKey:
        return keycode_Func8;
    case NSF9FunctionKey:
        return keycode_Func9;
    case NSF10FunctionKey:
        return keycode_Func10;
    case NSF11FunctionKey:
        return keycode_Func11;
    case NSF12FunctionKey:
        return keycode_Func12;
    case '\e':
        return keycode_Escape;
    }

    if (ch < 0x200000)
        return ch;

    return keycode_Unknown;
}

- (instancetype)initPrefsEventForTheme:(Theme *)aTheme {
    self = [super init];
    if (self) {
        _type = EVTPREFS;
        _val1 = aTheme.doGraphics;
        _val2 = aTheme.doSound;
        theme = aTheme;
    }
    return self;
}

- (instancetype)initCharEvent:(unsigned)v forWindow:(NSInteger)name {
    self = [super init];
    if (self) {
        _type = EVTKEY;
        win = name;
        _val1 = v;
    }
    return self;
}

- (instancetype)initMouseEvent:(NSPoint)v forWindow:(NSInteger)name {
    self = [super init];
    if (self) {
        _type = EVTMOUSE;
        win = name;
        _val1 = (NSInteger)v.x;
        _val2 = (NSInteger)v.y;
    }
    return self;
}

- (instancetype)initLineEvent:(NSString *)v forWindow:(NSInteger)name terminator:(NSInteger)terminator {
    self = [super init];
    if (self) {
        _type = EVTLINE;
        ln = [v copy];
        win = name;
        _val1 = (unsigned int)ln.length;
        _val2 = terminator;
    }
    return self;
}

- (instancetype)initTimerEvent {
    self = [super init];
    if (self)
        _type = EVTTIMER;
    return self;
}

- (instancetype)initArrangeWidth:(NSInteger)aw height:(NSInteger)ah theme:(Theme *)aTheme force:(BOOL)forceFlag
{
    self = [super init];
    if (self) {
        _type = EVTARRANGE;
        _val1 = aw;
        _val2 = ah;
        theme = aTheme;
      _forced = forceFlag;
    }
    return self;
}

- (instancetype)initRedrawEvent {
    self = [super init];
    if (self) {
        _type = EVTREDRAW;
    }
    return self;
}

- (instancetype)initQuitEvent {
    self = [super init];
    if (self) {
        _type = EVTQUIT;
    }
    return self;
}

- (instancetype)initSoundNotify:(NSInteger)notify withSound:(NSInteger)sound {
    self = [super init];
    if (self) {
        _type = EVTSOUND;
        _val1 = sound;
        _val2 = notify;
    }
    return self;
}

- (instancetype)initVolumeNotify:(NSInteger)notify {
    self = [super init];
    if (self) {
        _type = EVTVOLUME;
        _val2 = notify;
    }
    return self;
}

- (instancetype)initLinkEvent:(NSUInteger)linkid forWindow:(NSInteger)name {
    self = [super init];
    if (self) {
        _type = EVTHYPER;
        win = name;
        _val1 = (NSInteger)linkid;
    }
    return self;
}

- (void)writeEvent:(int)fd {
    struct message reply;
    char buf[4096];
    struct settings_struct *settings = (void*)buf;

    reply.len = 0;

    if (ln) {
        reply.len = ln.length * 2;
        if (reply.len > sizeof(buf))
            reply.len = sizeof(buf);
        [ln getCharacters:(unsigned short *)buf
                    range:NSMakeRange(0, (NSUInteger)reply.len / 2)];
    }

    reply.cmd = (int)_type;
    reply.a1 = (int)win;
    reply.a2 = (int)_val1;
    reply.a3 = (int)_val2;

    if (_type == EVTPREFS) {
        reply.a4 = (int)theme.autosave;
        reply.a5 = (int)theme.autosaveOnTimer;
    }

    if (_type == EVTARRANGE) {

        settings->screen_width = (int)_val1;
        settings->screen_height = (int)_val2;

        settings->buffer_margin_x = (int)theme.bufferMarginX;
        settings->buffer_margin_y = (int)theme.bufferMarginY;
        settings->grid_margin_x = (int)theme.gridMarginX;
        settings->grid_margin_y =(int)theme.gridMarginY;
        settings->cell_width = (float)theme.cellWidth;
        settings->cell_height = (float)theme.cellHeight;
        settings->buffer_cell_width = (float)theme.bufferCellWidth;
        settings->buffer_cell_height = (float)theme.bufferCellHeight;
        settings->buffer_foreground = (int)theme.bufferNormal.color.integerColor;
        settings->buffer_background = (int)theme.bufferBackground.integerColor;
        settings->grid_foreground = (int)theme.gridNormal.color.integerColor;
        settings->grid_background = (int)theme.gridBackground.integerColor;
        settings->do_styles = (int)theme.doStyles;
        settings->quote_boxes = (int)theme.quoteBox;
        settings->determinism = (int)theme.determinism;
        settings->error_handling = (int)theme.errorHandling;
        settings->sa_delays = (int)theme.sADelays;
        settings->sa_display_style = (int)theme.sADisplayStyle;
        settings->sa_inventory = (int)theme.sAInventory;
        settings->sa_palette = (int)theme.sAPalette;
        settings->slowdraw = (int)theme.slowDrawing;
        settings->flicker = (int)theme.flicker;
        settings->zmachine_terp = (int)theme.zMachineTerp;
        settings->z6_graphics = (int)theme.z6GraphicsType;
        settings->z6_colorize = (int)theme.z6Colorize1Bit;
        settings->zmachine_no_err_win = (int)theme.zMachineNoErrWin;
        settings->voiceover_on = (int)[NSWorkspace sharedWorkspace].voiceOverEnabled;

        settings->force_arrange = _forced;

        reply.len = sizeof(struct settings_struct);
    }

    write(fd, &reply, sizeof(struct message));
    if (reply.len)
        write(fd, buf, (size_t)reply.len);
}

@end
