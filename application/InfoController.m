#import "InfoController.h"

#import "Game.h"
#import "Metadata.h"
#import "Image.h"
#import "main.h"

#include "babel_handler.h"
#include "treaty.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation InfoController

- (instancetype)initWithGame:(Game *)game  {
    self = [super initWithWindowNibName:@"InfoPanel"];
    if (self) {
        _path = [game urlForBookmark].path;
        if (!_path)
            _path = game.path;
        _meta = game.metadata;
    }
    return self;
}

- (instancetype)initWithpath:(NSString *)path {
    self = [super initWithWindowNibName:@"InfoPanel"];
    if (self) {
        _path = path;
        _meta = nil;
    }
    return self;
}

- (void)sizeToFitImageAnimate:(BOOL)animate {
    NSRect frame;
    NSSize wellsize;
    NSSize imgsize;
    NSSize cursize;
    NSSize setsize;
    NSSize maxsize;
    double scale;

    maxsize = self.window.screen.frame.size;
    wellsize = imageView.frame.size;
    cursize = self.window.frame.size;

    maxsize.width = maxsize.width * 0.75 - (cursize.width - wellsize.width);
    maxsize.height = maxsize.height * 0.75 - (cursize.height - wellsize.height);

    NSArray *imageReps = imageView.image.representations;

    NSInteger width = 0;
    NSInteger height = 0;

    for (NSImageRep *imageRep in imageReps) {
        if (imageRep.pixelsWide > width)
            width = imageRep.pixelsWide;
        if (imageRep.pixelsHigh > height)
            height = imageRep.pixelsHigh;
    }

    imgsize.width = width;
    imgsize.height = height;

    imageView.image.size = imgsize; /* no steenkin' dpi here */

    if (imgsize.width > maxsize.width) {
        scale = maxsize.width / imgsize.width;
        imgsize.width *= scale;
        imgsize.height *= scale;
    }

    if (imgsize.height > maxsize.height) {
        scale = maxsize.height / imgsize.height;
        imgsize.width *= scale;
        imgsize.height *= scale;
    }

    if (imgsize.width < 100)
        imgsize.width = 100;
    if (imgsize.height < 150)
        imgsize.height = 150;

    setsize.width = cursize.width - wellsize.width + imgsize.width;
    setsize.height = cursize.height - wellsize.height + imgsize.height;

    frame = self.window.frame;
    frame.origin.y += frame.size.height;
    frame.size.width = setsize.width;
    frame.size.height = setsize.height;
    frame.origin.y -= setsize.height;
    [self.window setFrame:frame display:YES animate:animate];
}

- (void)windowDidLoad {
    NSURL *imgpath;
    NSString *pathstring;
    NSImage *img;
    NSData *imgdata;
    const char *format;

    // Get Application Support Directory URL
    NSError *error;
    imgpath = [[NSFileManager defaultManager]
               URLForDirectory:NSApplicationSupportDirectory
               inDomain:NSUserDomainMask
               appropriateForURL:nil
               create:YES
               error:&error];

    NSLog(@"infoctl: windowDidLoad");

    if (_path) {
        self.window.representedFilename = _path;
        self.window.title =
        [NSString stringWithFormat:@"%@ Info", _path.lastPathComponent];
    }

    [descriptionText setDrawsBackground:NO];
    [(NSScrollView *)descriptionText.superview setDrawsBackground:NO];

    if (_meta) {
        titleField.stringValue = _meta.title;
        if (_meta.author)
            authorField.stringValue = _meta.author;
        if (_meta.headline)
            headlineField.stringValue = _meta.headline;
        if (_meta.blurb)
            descriptionText.string = _meta.blurb;
        ifid = ((Game *)_meta.games.anyObject).ifid;
        if (ifid)
            ifidField.stringValue = ifid;
    }

    format = babel_init((char *)_path.UTF8String);
    if (format) {
        char buf[TREATY_MINIMUM_EXTENT];
        char *s;
        size_t imglen;
        int rv;

        rv = babel_treaty(GET_STORY_FILE_IFID_SEL, buf, sizeof buf);
        if (rv <= 0)
            goto finish;
        s = strchr(buf, ',');
        if (s)
            *s = 0;
        ifid = @(buf);

        ifidField.stringValue = ifid;
        pathstring =
        [[@"Spatterlight/Cover%20Art" stringByAppendingPathComponent:ifid]
         stringByAppendingPathExtension:@"tiff"];
        imgpath = [NSURL URLWithString:pathstring relativeToURL:imgpath];
        img = [[NSImage alloc] initWithContentsOfURL:imgpath];
        if (!img) {
            imglen = (size_t)babel_treaty(GET_STORY_FILE_COVER_EXTENT_SEL, NULL, 0);
            if (imglen > 0) {
                char *imgbuf = malloc(imglen);
                if (!imgbuf)
                    goto finish;

                //                rv =
                babel_treaty(GET_STORY_FILE_COVER_SEL, imgbuf, imglen);
                imgdata = [[NSData alloc] initWithBytesNoCopy:imgbuf
                                                       length:imglen
                                                 freeWhenDone:YES];
                img = [[NSImage alloc] initWithData:imgdata];
            } else {
                img = [[NSImage alloc] initWithData:(NSData *)_meta.cover.data];
            }
        }

        if (img) {
            imageView.image = img;
        }

        [self sizeToFitImageAnimate:NO];

    finish:
        babel_release();
    }

    self.window.delegate = self;
}

+ (NSArray *)restorableStateKeyPaths {
    return @[
             @"path", @"titleField.stringValue", @"authorField.stringValue",
             @"headlineField.stringValue", @"descriptionText.string"
             ];
}

- (void)saveImage:sender {
    NSURL *dirURL, *imgURL;
    NSData *imgdata;

    NSError *error;
    dirURL = [[NSFileManager defaultManager]
              URLForDirectory:NSApplicationSupportDirectory
              inDomain:NSUserDomainMask
              appropriateForURL:nil
              create:YES
              error:&error];

    dirURL = [NSURL URLWithString:@"Spatterlight/Cover%20Art"
                    relativeToURL:dirURL];

    imgURL = [NSURL
              fileURLWithPath:[[dirURL.path stringByAppendingPathComponent:ifid]
                               stringByAppendingPathExtension:@"tiff"]
              isDirectory:NO];

    [[NSFileManager defaultManager] createDirectoryAtURL:dirURL
                             withIntermediateDirectories:YES
                                              attributes:nil
                                                   error:NULL];

    NSLog(@"infoctl: save image %@", imgURL);

    imgdata =
    [imageView.image TIFFRepresentationUsingCompression:NSTIFFCompressionLZW
                                                 factor:0];
    [imgdata writeToURL:imgURL atomically:YES];

    [self sizeToFitImageAnimate:YES];
}

- (void)updateBlurb
{
	_game.metadata.blurb = descriptionText.textStorage.string;

//	[((AppDelegate *)[NSApplication sharedApplication].delegate)
//     .libctl updateSideViewForce:YES];
}

@end
