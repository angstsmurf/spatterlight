#import "main.h"

#include "treaty.h"
#include "babel_handler.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation InfoController

void showInfoForFile(NSString *path, NSDictionary *info)
{
    NSArray *windows = [NSApp windows];
    NSWindow *window;
    NSWindowController *winctl;
    InfoController *infoctl;
    int count;
    int i;
    
    count = [windows count];
    for (i = 0; i < count; i++)
    {
	window = (NSWindow*) [windows objectAtIndex: i];
	winctl = (NSWindowController*) [window delegate];
	if (winctl && [winctl isKindOfClass: [InfoController class]])
	{
	    infoctl = (InfoController*) winctl;
	    if ([infoctl->path isEqualToString: path])
	    {
		[infoctl showWindow: nil];
		return;
	    }
	}
    }
    
    infoctl = [[InfoController alloc] initWithWindowNibName: @"InfoPanel"];
    [infoctl showForFile: path info: info];
    [infoctl showWindow: nil];
    
    // infoctl releases itself when it closes
}

- (void) sizeToFitImageAnimate: (BOOL)animate
{
    NSRect frame;
    NSSize wellsize;
    NSSize imgsize;
    NSSize cursize;
    NSSize setsize;
    NSSize maxsize;
    float scale;
    
    maxsize = [[[self window] screen] frame].size;
    wellsize = [imageView frame].size;
    cursize = [[self window] frame].size;

    maxsize.width = maxsize.width * 0.75 - (cursize.width - wellsize.width);
    maxsize.height = maxsize.height * 0.75 - (cursize.height - wellsize.height);
    
    imgsize.width = [[[imageView image] bestRepresentationForDevice:nil] pixelsWide];
    imgsize.height = [[[imageView image] bestRepresentationForDevice:nil] pixelsHigh];
    [[imageView image] setSize: imgsize]; /* no steenkin' dpi here */
    
    if (imgsize.width > maxsize.width)
    {
	scale = maxsize.width / imgsize.width;
	imgsize.width *= scale;
	imgsize.height *= scale;
    }
    
    if (imgsize.height > maxsize.height)
    {
	scale = maxsize.height / imgsize.height;
	imgsize.width *= scale;
	imgsize.height *= scale;
    }
    
    if (imgsize.width < 100) imgsize.width = 100;
    if (imgsize.height < 150) imgsize.height = 150;
    
    setsize.width = cursize.width - wellsize.width + imgsize.width;
    setsize.height = cursize.height - wellsize.height + imgsize.height;
    
    frame = [[self window] frame];
    frame.origin.y += frame.size.height;
    frame.size.width = setsize.width;
    frame.size.height = setsize.height;
    frame.origin.y -= setsize.height;
    [[self window] setFrame: frame display: YES animate: animate];    
}

- (void) windowDidLoad
{
    NSString *dirpath, *imgpath;
    NSImage *img;
    NSData *imgdata;
    const char *format;

    NSLog(@"infoctl: windowDidLoad");

    [[self window] setRepresentedFilename: path];
    [[self window] setTitle: [NSString stringWithFormat: @"%@ Info", [path lastPathComponent]]];
   
    [descriptionText setDrawsBackground: NO];
    [(NSScrollView *)[descriptionText superview] setDrawsBackground:NO];

    [titleField setStringValue: [meta objectForKey: @"title"]];
    if ([meta objectForKey: @"author"])
       [authorField setStringValue: [meta objectForKey: @"author"]];
    if ([meta objectForKey: @"headline"])
	[headlineField setStringValue: [meta objectForKey: @"headline"]];
    if ([meta objectForKey: @"description"])
	[descriptionText setString: [meta objectForKey: @"description"]];
    
    format = babel_init((char*)[path UTF8String]);
    if (format)
    {
	char buf[TREATY_MINIMUM_EXTENT];
	char *s;
	int imglen;
	int rv;
	
	rv = babel_treaty(GET_STORY_FILE_IFID_SEL, buf, sizeof buf);
	if (rv <= 0)
	    goto finish;
	s = strchr(buf, ',');
	if (s) *s = 0;
	ifid = [NSString stringWithUTF8String: buf];
	
	[ifidField setStringValue: ifid];
	
	dirpath = [@"~/Library/Application Support/Spatterlight/Cover Art" stringByStandardizingPath];
	imgpath = [[dirpath stringByAppendingPathComponent: ifid] stringByAppendingPathExtension: @"tiff"];
	img = [[NSImage alloc] initWithContentsOfFile: imgpath];
	if (!img)
	{
	    imglen = babel_treaty(GET_STORY_FILE_COVER_EXTENT_SEL, NULL, 0);
	    if (imglen > 0)
	    {
		char *imgbuf = malloc(imglen);
		if (!imgbuf)
		    goto finish;
		
		rv = babel_treaty(GET_STORY_FILE_COVER_SEL, imgbuf, imglen);
		imgdata = [[NSData alloc] initWithBytesNoCopy: imgbuf length: imglen freeWhenDone: YES];
		img = [[NSImage alloc] initWithData: imgdata];
		[imgdata release];
	    }
	}
	
	if (img)
	{
	    [imageView setImage: img];
	    [img release];
	}

	[self sizeToFitImageAnimate: NO];

finish:
	babel_release();
    }
}

- (void) saveImage: sender
{
    NSString *dirpath, *imgpath;
    NSData *imgdata;
    
    dirpath = [@"~/Library/Application Support/Spatterlight/Cover Art" stringByStandardizingPath];
    imgpath = [[dirpath stringByAppendingPathComponent: ifid] stringByAppendingPathExtension: @"tiff"];
    
    [[NSFileManager defaultManager] createDirectoryAtPath: dirpath attributes: nil];
    
    NSLog(@"infoctl: save image %@", imgpath);
    
    imgdata = [[imageView image] TIFFRepresentationUsingCompression: NSTIFFCompressionLZW factor: 0];
    [imgdata writeToFile: imgpath atomically: YES];

    [self sizeToFitImageAnimate: YES];
}

- (void) showForFile: (NSString*)path_ info: (NSDictionary*)meta_
{
    path = [path_ retain];
    meta = [meta_ retain];
}

- (void) showWindow: sender
{
    [super showWindow: sender];
}

- (void) windowWillClose: sender
{
    [self autorelease];
}

- (void) dealloc
{
    NSLog(@"infoctl: dealloc");
    [path release];
    [meta release];
    [super dealloc];
}

@end
