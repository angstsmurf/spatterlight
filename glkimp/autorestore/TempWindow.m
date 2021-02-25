/* TempWindow.m: Temporary Glk window objc class
 for serializing, adapted from IosGlk, the iOS
 implementation of the Glk API by Andrew Plotkin
 */

#import "TempLibrary.h"
#import "TempWindow.h"

@interface TempWindow () <NSSecureCoding> {

    TempLibrary *library;

    glui32 x0, y0, x1, y1;

    // For pair window struct:
    glui32 child1, child2;
    glui32 dir;            /* winmethod_Left, Right, Above, or Below */
    int vertical, backward;        /* flags */
    glui32 division;        /* winmethod_Fixed or winmethod_Proportional */
    glui32 key;            /* NULL or a leaf-descendant (not a Pair) */
    glui32 size;            /* size value */

    // For line input buffer struct:
    void *buf;
    int len;
    int cap;
    gidispatch_rock_t inarrayrock;

    glui32 background;    /* for graphics windows */

    int line_request;
    int line_request_uni;
    int char_request;
    int char_request_uni;
    int mouse_request;
    int hyper_request;

    int echo_line_input;

    glui32 style;

    int fgset;
    int bgset;
    int reverse;
    int attrstyle;
    int fgcolor;
    int bgcolor;
    int hyper;

    NSMutableArray *line_terminators;

    /* These values are only used in a temporary TempLibrary, while deserializing. */
    uint8_t *tempbufdata;
    NSUInteger tempbufdatalen;
    long tempbufkey;
}
@end

@implementation TempWindow

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (id) initWithCStruct:(window_t *)win {

    self = [super init];

	if (self) {

        _tag = win->tag;
		_type = win->type;
		_rock = win->rock;
        _peer = win->peer;

        x0 = win->bbox.x0;
        y0 = win->bbox.y0;
        x1 = win->bbox.x1;
        y1 = win->bbox.y1;

        //We use tags instead of pointers here

        _next = _prev = _parent = 0;

        if (win->next)
            _next = (win->next)->tag;
        if (win->prev)
            _prev = (win->prev)->tag;
        if (win->parent)
            _parent = (win->parent)->tag;

        child1 = child2 = key = 0;
        if (win->pair.child1)
            child1 = win->pair.child1->tag;
        if (win->pair.child2)
            child2 = win->pair.child2->tag;
        dir = win->pair.dir;
        vertical = win->pair.vertical;
        backward = win->pair.backward;
        division = win->pair.division;
        if (win->pair.key)
            key = win->pair.key->tag;
        size = win->pair.size;

		char_request = win->char_request;
		line_request = win->line_request;
        mouse_request = win->mouse_request;
        hyper_request = win->hyper_request;

		char_request_uni = win->char_request_uni;
		line_request_uni = win->line_request_uni;

		echo_line_input = win->echo_line_input;

        if (win->termct)
        {
            line_terminators = [NSMutableArray arrayWithCapacity:win->termct];
            for (NSInteger i=0; i<win->termct; i++)
                [line_terminators addObject:[NSNumber numberWithInt:win->line_terminators[i]]];
        }
        else line_terminators = nil;

		style = win->style;

        _streamtag = _echostreamtag = 0;

        if (win->str)
            _streamtag = win->str->tag;

        if (win->echostr)
            _echostreamtag = win->echostr->tag;

        buf = win->line.buf;
        len = win->line.len;
        cap = win->line.cap;

        inarrayrock = win->line.inarrayrock;
    }
    return self;
}


- (id) initWithCoder:(NSCoder *)decoder {

    _tag = [decoder decodeInt32ForKey:@"tag"];
	_type = [decoder decodeInt32ForKey:@"type"];
	_rock = [decoder decodeInt32ForKey:@"rock"];
    _peer = [decoder decodeInt32ForKey:@"peer"];

    _prev = [decoder decodeInt32ForKey:@"prev"];
    _next = [decoder decodeInt32ForKey:@"next"];

    x0 = [decoder decodeInt32ForKey:@"x0"];
    y0 = [decoder decodeInt32ForKey:@"y0"];
    x1 = [decoder decodeInt32ForKey:@"x1"];
    y1 = [decoder decodeInt32ForKey:@"y1"];

    child1 =  [decoder decodeInt32ForKey:@"child1"];
    child2 =  [decoder decodeInt32ForKey:@"child2"];

    dir = [decoder decodeInt32ForKey:@"dir"];
    vertical = [decoder decodeInt32ForKey:@"vertical"];
    backward = [decoder decodeInt32ForKey:@"backward"];
    division = [decoder decodeInt32ForKey:@"division"];
    size = [decoder decodeInt32ForKey:@"size"];
    key = [decoder decodeInt32ForKey:@"key"];

    style = [decoder decodeInt32ForKey:@"style"];
    fgset = [decoder decodeInt32ForKey:@"fgset"];
    bgset = [decoder decodeInt32ForKey:@"bgset"];
    reverse = [decoder decodeInt32ForKey:@"reverse"];
    attrstyle = [decoder decodeInt32ForKey:@"attrstyle"];
    fgcolor = [decoder decodeInt32ForKey:@"fgcolor"];
    bgcolor = [decoder decodeInt32ForKey:@"bgcolor"];
    hyper = [decoder decodeInt32ForKey:@"hyper"];

	_parent = [decoder decodeInt32ForKey:@"parent"];

	char_request = [decoder decodeInt32ForKey:@"char_request"];
	line_request = [decoder decodeInt32ForKey:@"line_request"];
	char_request_uni = [decoder decodeInt32ForKey:@"char_request_uni"];
	line_request_uni = [decoder decodeInt32ForKey:@"line_request_uni"];

    background = [decoder decodeInt32ForKey:@"background"];

    cap = [decoder decodeInt32ForKey:@"cap"];
	len = [decoder decodeInt32ForKey:@"len"];
	if (cap) {
		// the decoded "line_buffer" values are originally Glulx addresses (glui32), so stuffing them into a long is safe.
		if (!line_request_uni) {
			tempbufkey = (long)[decoder decodeInt64ForKey:@"line_buffer"];
			uint8_t *rawdata;
			NSUInteger rawdatalen;
			rawdata = (uint8_t *)[decoder decodeBytesForKey:@"line_buffer_data" returnedLength:&rawdatalen];
			if (rawdata && rawdatalen) {
				tempbufdatalen = rawdatalen;
				tempbufdata = malloc(rawdatalen);
				memcpy(tempbufdata, rawdata, rawdatalen);
			}
		}
		else {
			tempbufkey = (long)[decoder decodeInt64ForKey:@"line_buffer"];
			uint8_t *rawdata;
			NSUInteger rawdatalen;
			rawdata = (uint8_t *)[decoder decodeBytesForKey:@"line_buffer_data" returnedLength:&rawdatalen];
			if (rawdata && rawdatalen) {
				tempbufdatalen = rawdatalen;
				tempbufdata = malloc(rawdatalen);
				memcpy(tempbufdata, rawdata, rawdatalen);
			}
		}
	}

	echo_line_input = [decoder decodeIntForKey:@"echo_line_input"];
	style = [decoder decodeInt32ForKey:@"style"];
    line_terminators = [decoder decodeObjectOfClass:[NSMutableArray class] forKey:@"line_terminators"];

	_streamtag = [decoder decodeInt32ForKey:@"streamtag"];
	_echostreamtag = [decoder decodeInt32ForKey:@"echostreamtag"];

	return self;
}

- (void) encodeWithCoder:(NSCoder *)encoder {
    [encoder encodeInt32:_tag forKey:@"tag"];
    [encoder encodeInt32:_type forKey:@"type"];
    [encoder encodeInt32:_rock forKey:@"rock"];
    [encoder encodeInt32:_peer forKey:@"peer"];

    [encoder encodeInt32:_prev forKey:@"prev"];
    [encoder encodeInt32:_next forKey:@"next"];

    // disprock is handled by the app

    [encoder encodeInt32:_parent forKey:@"parent"];

    [encoder encodeInt32:char_request forKey:@"char_request"];
    [encoder encodeInt32:line_request forKey:@"line_request"];
    [encoder encodeInt32:char_request_uni forKey:@"char_request_uni"];
    [encoder encodeInt32:line_request_uni forKey:@"line_request_uni"];

    [encoder encodeInt32:background forKey:@"background"];

    if (buf && cap && gli_locate_arr) {
        long bufaddr;
        int elemsize;
        [encoder encodeInt32:cap forKey:@"cap"];
        [encoder encodeInt32:len forKey:@"len"];

        if (!line_request_uni) {
            bufaddr = (*gli_locate_arr)(buf, cap, "&+#!Cn", inarrayrock, &elemsize);
            [encoder encodeInt64:bufaddr forKey:@"line_buffer"];
            if (elemsize) {
                NSAssert(elemsize == 1, @"TempWindow encoding char array: wrong elemsize");
                // could trim trailing zeroes here
                [encoder encodeBytes:(uint8_t *)buf length:cap forKey:@"line_buffer_data"];
            }
        }
        else {
            bufaddr = (*gli_locate_arr)(buf, cap, "&+#!Iu", inarrayrock, &elemsize);
            [encoder encodeInt64:bufaddr forKey:@"line_buffer"];
            if (elemsize) {
                NSAssert(elemsize == 4, @"TempWindow encoding uni array: wrong elemsize");
                // could trim trailing zeroes here
                [encoder encodeBytes:(uint8_t *)buf length:sizeof(glui32)*cap forKey:@"line_buffer_data"];
            }
        }
    }

    //[encoder encodeInt:pending_echo_line_input forKey:@"pending_echo_line_input"];
    [encoder encodeInt32:echo_line_input forKey:@"echo_line_input"];
    if (line_terminators)
        [encoder encodeObject:line_terminators forKey:@"line_terminators"];
    [encoder encodeInt32:style forKey:@"style"];

    [encoder encodeInt32:_streamtag forKey:@"streamtag"];
    [encoder encodeInt32:_echostreamtag forKey:@"echostreamtag"];

    [encoder encodeInt32:x0 forKey:@"x0"];
    [encoder encodeInt32:y0 forKey:@"y0"];
    [encoder encodeInt32:x1 forKey:@"x1"];
    [encoder encodeInt32:y1 forKey:@"y1"];

    [encoder encodeInt32:child1 forKey:@"child1"];
    [encoder encodeInt32:child2 forKey:@"child2"];

    [encoder encodeInt32:dir forKey:@"dir"];
    [encoder encodeInt32:vertical forKey:@"vertical"];
    [encoder encodeInt32:backward forKey:@"backward"];
    [encoder encodeInt32:division forKey:@"division"];
    [encoder encodeInt32:size forKey:@"size"];
    [encoder encodeInt32:key forKey:@"key"];

    [encoder encodeInt32:fgset forKey:@"fgset"];
    [encoder encodeInt32:bgset forKey:@"bgset"];
    [encoder encodeInt32:reverse forKey:@"reverse"];
    [encoder encodeInt32:attrstyle forKey:@"attrstyle"];
    [encoder encodeInt32:fgcolor forKey:@"fgcolor"];
    [encoder encodeInt32:bgcolor forKey:@"bgcolor"];
    [encoder encodeInt32:hyper forKey:@"hyper"];
}

- (void) copyToCStruct:(window_t *)win {

    [self updateRegisterArray];

    win->magicnum = MAGIC_WINDOW_NUM;

    win->peer = _peer;
    win->tag = _tag;
    win->rock = _rock;
    win->type = _type;

    win->line.buf = buf;
    win->line.len = len;
    win->line.cap = cap;

    win->background = background;

    win->char_request = char_request;
    win->char_request_uni = char_request_uni;
    win->line_request = line_request;
    win->line_request_uni = line_request_uni;
    win->mouse_request = mouse_request;

    win->bbox.x0 = x0;
    win->bbox.y0 = y0;
    win->bbox.x1 = x1;
    win->bbox.y1 = y1;

    win->pair.dir = dir;
    win->pair.vertical = vertical;
    win->pair.backward = backward;
    win->pair.division = division;
    win->pair.size = size;

    win->line.len = len;
    win->line.cap = cap;
    win->termct = line_terminators.count;
    win->line.buf = buf;
    win->line.inarrayrock = inarrayrock;

    if (win->termct)
    {
        win->line_terminators = malloc((win->termct + 1) * sizeof(glui32));
        if (win->line_terminators)
        {
            for (int i=0; i<win->termct; i++)
                win->line_terminators[i]=(glui32)((NSNumber *)[line_terminators objectAtIndex:i]).intValue;
            win->line_terminators[win->termct] = 0;
        }
    }

    win->style = style;
    win->attr.fgset = fgset;
    win->attr.bgset = bgset;
    win->attr.reverse = reverse;
    win->attr.style = attrstyle;
    win->attr.fgcolor = fgcolor;
    win->attr.bgcolor = bgcolor;
    win->attr.hyper = hyper;
}

- (void) finalizeCStruct:(window_t *)win {

    win->parent = gli_window_for_tag(_parent);
    win->pair.child1 = gli_window_for_tag(child1);
    win->pair.child2 = gli_window_for_tag(child2);
    win->pair.key = gli_window_for_tag(key);

    win->str = gli_stream_for_tag(_streamtag);
    if (_streamtag && !win->str)
        NSLog(@"finalizeCStruct: Error! Could no find window stream %d for window %d",_echostreamtag, _tag);

    win->echostr = gli_stream_for_tag(_echostreamtag);
    if (_echostreamtag && !win->echostr)
        NSLog(@"finalizeCStruct: Error! Could no find echostream %d for window %d",_echostreamtag, _tag);

    if (win->str && win->tag != win->str->win->tag)
        NSLog(@"finalizeCStruct: Error! Window stream does not point back at window %d, but at %d",win->tag, win->str->win->tag);

    if (win->echostr && (!win->echostr->win || win->tag != win->echostr->win->tag)) {
        if (!win->echostr->win)
            NSLog(@"finalizeCStruct: Error! Window echo stream has no win at all!");
        else
            NSLog(@"finalizeCStruct: Error! Window echo stream does not point back at window %d, but at %d",win->tag, win->echostr->win->tag);
        NSLog(@"Trying to fix this.");
        win->echostr->win = win;
    }
}

- (void) updateRegisterArray {
    if (!gli_restore_arr)
        return;
    if (!cap)
        return;

    if (!line_request_uni) {
        void *voidbuf = nil;
        inarrayrock = (*gli_restore_arr)(tempbufkey, cap, "&+#!Cn", &voidbuf);
        if (voidbuf) {
            buf = voidbuf;
            if (tempbufdata) {
                if (tempbufdatalen > cap)
                    tempbufdatalen = cap;
                memcpy(buf, tempbufdata, tempbufdatalen);
                free(tempbufdata);
                tempbufdata = nil;
            }
        }
    }
    else {
        void *voidbuf = nil;
        inarrayrock = (*gli_restore_arr)(tempbufkey, cap, "&+#!Iu", &voidbuf);
        if (voidbuf) {
            buf = voidbuf;
            if (tempbufdata) {
                if (tempbufdatalen > sizeof(glui32)*cap)
                    tempbufdatalen = sizeof(glui32)*cap;
                memcpy(buf, tempbufdata, tempbufdatalen);
                free(tempbufdata);
                tempbufdata = nil;
            }
        }
    }
}

- (void) sanityCheckWithLibrary:(TempLibrary *)lib
{
    if (!_type)
        NSLog(@"SANITY: window lacks type");

    if (!_parent) {
        if (_tag != lib.rootwintag)
            NSLog(@"SANITY: window has no parent but is not rootwin");
    }
    else {
        if (_parent != gli_window_for_tag(_parent)->tag)
            NSLog(@"SANITY: window parent tag mismatch");
        if ([lib windowForTag:_parent].type != wintype_Pair)
            NSLog(@"SANITY: window parent is not pair");
    }
    if (![lib streamForTag:_streamtag])
        NSLog(@"SANITY: window lacks stream");
    if (_streamtag != gli_stream_for_tag(_streamtag)->tag)
        NSLog(@"SANITY: window stream tag mismatch");
    if ([lib streamForTag:_streamtag].type != strtype_Window)
        NSLog(@"SANITY: window stream is wrong type");
    if (_echostreamtag && _echostreamtag != gli_stream_for_tag(_echostreamtag)->tag)
        NSLog(@"SANITY: window echo stream tag mismatch");

    if (_type != wintype_Pair && !style)
        NSLog(@"SANITY: non-pair window lacks styleset");

    switch (_type) {
        case wintype_Pair: {
            if (!child1)
                NSLog(@"SANITY: pair win has no child1");
            if (!child2)
                NSLog(@"SANITY: pair win has no child2");
            if (child1 != gli_window_for_tag(child1)->tag)
                NSLog(@"SANITY: pair child1 tag mismatch");
            if (child2 != gli_window_for_tag(child2)->tag)
                NSLog(@"SANITY: pair child2 tag mismatch");
            if (style)
                NSLog(@"SANITY: pair window has styleset");
            if (key)
                NSLog(@"SANITY: pair window has leftover keydamage");
        }
            break;

        case wintype_TextBuffer: {
            if (!buf && (len || cap))
                NSLog(@"SANITY: text buffer window has len or cap but no buf");
        }
            break;

        case wintype_TextGrid: {
            if (!buf && (len || cap))
                NSLog(@"SANITY: text grid window has len or cap but no buf");
        }
            break;
    }
}

@end
