//
//  MNGConverter.m
//  Spatterlight
//
//  Follows the MNG 1.0 specification. Of the HTML TADS corpus, Macrocosm has
//  four MNGs of full RGBA frames, each after a FRAM with its delay, played
//  once by a TERM. Antiquest has one of twelve paletted frames, each placed
//  by a DEFI, followed by a LOOP whose iteration count is 0, which the
//  specification says is done zero times.
//

#import "MNGConverter.h"
#import <ImageIO/ImageIO.h>

static const uint8_t kMNGSignature[8] = { 0x8a, 'M', 'N', 'G', '\r', '\n', 0x1a, '\n' };
static const uint8_t kPNGSignature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };

// A bound on the frames a LOOP can repeat into, as an iteration count can
// be as high as 2^31 - 1.
static const NSUInteger kMaxFrames = 2000;
// MNG's "infinite" iteration count.
static const uint32_t kMNGInfinity = 0x7fffffff;

typedef struct {
    uint32_t type;
    const uint8_t *body;
    uint32_t length;
    // Offset of the chunk's length field, and of the next chunk.
    NSUInteger start, end;
} MNGChunk;

typedef struct {
    NSUInteger first;     // chunk index after the LOOP
    uint32_t remaining;   // iterations still to do after this one
    uint8_t nest;
} MNGLoop;

#define FOURCC(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))

static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint16_t read16(const uint8_t *p) {
    return (uint16_t)(p[0] << 8 | p[1]);
}

BOOL MNGIsMNGData(NSData *data) {
    return data.length >= sizeof kMNGSignature
        && memcmp(data.bytes, kMNGSignature, sizeof kMNGSignature) == 0;
}

// The chunks up to MEND, or the end of the data, or the first chunk that
// does not fit in it.
static NSArray<NSValue *> *readChunks(NSData *mng) {
    NSMutableArray<NSValue *> *chunks = [NSMutableArray new];
    const uint8_t *bytes = mng.bytes;
    NSUInteger pos = sizeof kMNGSignature;
    while (pos + 12 <= mng.length) {
        MNGChunk chunk;
        chunk.length = read32(bytes + pos);
        if (chunk.length > mng.length - pos - 12)
            break;
        chunk.type = read32(bytes + pos + 4);
        chunk.body = bytes + pos + 8;
        chunk.start = pos;
        chunk.end = pos + 12 + chunk.length;
        [chunks addObject:[NSValue valueWithBytes:&chunk objCType:@encode(MNGChunk)]];
        pos = chunk.end;
        if (chunk.type == FOURCC('M', 'E', 'N', 'D'))
            break;
    }
    return chunks;
}

static MNGChunk chunkAt(NSArray<NSValue *> *chunks, NSUInteger i) {
    MNGChunk chunk;
    [chunks[i] getValue:&chunk size:sizeof chunk];
    return chunk;
}

// An embedded IHDR…IEND run as a standalone PNG.
static CGImageRef createPNGImage(NSData *mng, NSUInteger start, NSUInteger end) {
    NSMutableData *png = [NSMutableData dataWithBytes:kPNGSignature length:sizeof kPNGSignature];
    [png appendBytes:(const uint8_t *)mng.bytes + start length:end - start];
    CGImageSourceRef source = CGImageSourceCreateWithData((__bridge CFDataRef)png, NULL);
    if (!source)
        return NULL;
    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
    CFRelease(source);
    return image;
}

NSData * _Nullable MNGConvertToGIF(NSData *mng) {
    if (!MNGIsMNGData(mng))
        return nil;
    NSArray<NSValue *> *chunks = readChunks(mng);
    if (chunks.count == 0)
        return nil;
    MNGChunk mhdr = chunkAt(chunks, 0);
    if (mhdr.type != FOURCC('M', 'H', 'D', 'R') || mhdr.length < 12)
        return nil;
    size_t width = read32(mhdr.body), height = read32(mhdr.body + 4);
    uint32_t ticksPerSecond = read32(mhdr.body + 8);
    if (width == 0 || height == 0 || width > 0x4000 || height > 0x4000)
        return nil;
    // With no ticks per second, frame timing is up to the viewer.
    double secondsPerTick = ticksPerSecond ? 1.0 / ticksPerSecond : 0.1;

    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef canvas = CGBitmapContextCreate(NULL, width, height, 8, 0, space,
                                                (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(space);
    if (!canvas)
        return nil;
    CGRect canvasRect = CGRectMake(0, 0, width, height);

    NSMutableArray *frames = [NSMutableArray new];
    NSMutableArray<NSNumber *> *delays = [NSMutableArray new];

    // The current state, with the defaults of the specification.
    uint8_t framingMode = 1;
    uint32_t defaultDelay = 1;
    __block uint32_t subframeDelay = 1;
    __block BOOL haveBackground = NO;
    __block CGFloat red = 0, green = 0, blue = 0;
    BOOL doNotShow = NO;
    int32_t x = 0, y = 0;
    // Whether the canvas has had no layer yet, has had none in this
    // subframe, and has layers not yet put out as a frame.
    BOOL firstLayer = YES, subframeEmpty = YES, pending = NO;
    uint32_t loopCount = 1;
    NSMutableArray<NSValue *> *loops = [NSMutableArray new];

    void (^clearCanvas)(void) = ^{
        CGContextClearRect(canvas, canvasRect);
        if (haveBackground) {
            CGContextSetRGBFillColor(canvas, red, green, blue, 1);
            CGContextFillRect(canvas, canvasRect);
        }
    };
    void (^emitFrame)(void) = ^{
        CGImageRef frame = CGBitmapContextCreateImage(canvas);
        if (!frame)
            return;
        [frames addObject:CFBridgingRelease(frame)];
        [delays addObject:@(subframeDelay * secondsPerTick)];
    };

    NSUInteger i = 1;
    while (i < chunks.count && frames.count < kMaxFrames) {
        MNGChunk chunk = chunkAt(chunks, i++);
        const uint8_t *b = chunk.body;
        uint32_t len = chunk.length;

        switch (chunk.type) {
            case FOURCC('M', 'E', 'N', 'D'):
                i = chunks.count;
                break;

            case FOURCC('B', 'A', 'C', 'K'):
                if (len >= 6) {
                    haveBackground = YES;
                    red = read16(b) / 65535.0;
                    green = read16(b + 2) / 65535.0;
                    blue = read16(b + 4) / 65535.0;
                }
                break;

            case FOURCC('T', 'E', 'R', 'M'):
                // Only "repeat" has a count; showing the last frame, the
                // first one or none afterwards is all "stop" to a GIF.
                if (len >= 10 && b[0] == 3) {
                    uint32_t max = read32(b + 6);
                    loopCount = max >= kMNGInfinity ? 0 : MAX(max, 1u);
                }
                break;

            case FOURCC('D', 'E', 'F', 'I'):
                doNotShow = len >= 3 && b[2] == 1;
                x = y = 0;
                if (len >= 12) {
                    x = (int32_t)read32(b + 4);
                    y = (int32_t)read32(b + 8);
                }
                break;

            case FOURCC('F', 'R', 'A', 'M'): {
                // Modes 2 and 4 put a subframe's layers out as one frame,
                // ended by the next FRAM.
                if (pending)
                    emitFrame();
                pending = NO;
                subframeEmpty = YES;
                subframeDelay = defaultDelay;
                if (len == 0)
                    break;
                if (b[0] != 0)
                    framingMode = b[0];
                // The subframe name, ended by a NUL, then the change flags.
                uint32_t p = 1;
                while (p < len && b[p] != 0)
                    p++;
                p++;
                if (p + 4 > len)
                    break;
                uint8_t changeDelay = b[p];
                p += 4;
                if (changeDelay && p + 4 <= len) {
                    subframeDelay = read32(b + p);
                    if (changeDelay == 2)
                        defaultDelay = subframeDelay;
                }
                break;
            }

            case FOURCC('L', 'O', 'O', 'P'): {
                if (len < 5)
                    break;
                uint8_t nest = b[0];
                uint32_t count = read32(b + 1);
                if (count == 0) {
                    // Done zero times: on past the matching ENDL.
                    while (i < chunks.count) {
                        MNGChunk endl = chunkAt(chunks, i++);
                        if (endl.type == FOURCC('E', 'N', 'D', 'L') && endl.length >= 1 && endl.body[0] == nest)
                            break;
                    }
                    break;
                }
                MNGLoop loop = { i, MIN(count, kMNGInfinity) - 1, nest };
                [loops addObject:[NSValue valueWithBytes:&loop objCType:@encode(MNGLoop)]];
                break;
            }

            case FOURCC('E', 'N', 'D', 'L'): {
                if (len < 1)
                    break;
                // An ENDL closes its own LOOP and any left open inside it.
                while (loops.count) {
                    MNGLoop loop;
                    [loops.lastObject getValue:&loop size:sizeof loop];
                    if (loop.nest < b[0])
                        break;
                    [loops removeLastObject];
                    if (loop.nest == b[0]) {
                        if (loop.remaining) {
                            loop.remaining--;
                            [loops addObject:[NSValue valueWithBytes:&loop objCType:@encode(MNGLoop)]];
                            i = loop.first;
                        }
                        break;
                    }
                }
                break;
            }

            case FOURCC('I', 'H', 'D', 'R'): {
                NSUInteger end = chunk.end;
                while (i < chunks.count) {
                    MNGChunk part = chunkAt(chunks, i++);
                    end = part.end;
                    if (part.type == FOURCC('I', 'E', 'N', 'D'))
                        break;
                }
                if (doNotShow)
                    break;
                CGImageRef image = createPNGImage(mng, chunk.start, end);
                if (!image) {
                    i = chunks.count;
                    break;
                }
                // Mode 3 puts the background behind every layer, mode 4
                // behind the first of each subframe, and modes 1 and 2
                // only behind the first of all.
                if (firstLayer || framingMode == 3 || (framingMode == 4 && subframeEmpty))
                    clearCanvas();
                size_t w = CGImageGetWidth(image), h = CGImageGetHeight(image);
                // DEFI places the image from the top left.
                CGContextDrawImage(canvas, CGRectMake(x, (CGFloat)height - y - h, w, h), image);
                CGImageRelease(image);
                firstLayer = subframeEmpty = NO;
                if (framingMode == 1 || framingMode == 3) {
                    emitFrame();
                    subframeDelay = defaultDelay;
                } else {
                    pending = YES;
                }
                break;
            }

            default:
                // Chunks that only describe the file. Any other critical
                // one (an uppercase first letter), such as SHOW or a delta
                // image, needs more than this reads, so the animation stops.
                if ((chunk.type >> 24 & 0x20) == 0
                    && chunk.type != FOURCC('S', 'A', 'V', 'E')
                    && chunk.type != FOURCC('S', 'E', 'E', 'K')
                    && chunk.type != FOURCC('n', 'E', 'E', 'D'))
                    i = chunks.count;
                break;
        }
    }
    if (pending && frames.count < kMaxFrames)
        emitFrame();
    CGContextRelease(canvas);

    if (frames.count == 0)
        return nil;

    NSMutableData *gif = [NSMutableData new];
    CGImageDestinationRef destination = CGImageDestinationCreateWithData((__bridge CFMutableDataRef)gif, CFSTR("com.compuserve.gif"), frames.count, NULL);
    if (!destination)
        return nil;
    // A GIF with no loop count plays once.
    if (loopCount != 1)
        CGImageDestinationSetProperties(destination, (__bridge CFDictionaryRef)@{
            (NSString *)kCGImagePropertyGIFDictionary: @{ (NSString *)kCGImagePropertyGIFLoopCount: @(loopCount) } });
    for (NSUInteger f = 0; f < frames.count; f++) {
        NSDictionary *properties = @{ (NSString *)kCGImagePropertyGIFDictionary: @{
            (NSString *)kCGImagePropertyGIFDelayTime: delays[f],
            (NSString *)kCGImagePropertyGIFUnclampedDelayTime: delays[f] } };
        CGImageDestinationAddImage(destination, (__bridge CGImageRef)frames[f], (__bridge CFDictionaryRef)properties);
    }
    BOOL finished = CGImageDestinationFinalize(destination);
    CFRelease(destination);
    return finished ? gif : nil;
}
