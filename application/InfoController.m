#import "main.h"

#include "treaty.h"
#include "babel_handler.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...) fprintf(stderr,"%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@implementation InfoController

- (void) sizeToFitImageAnimate: (BOOL)animate
{
    NSRect frame;
    NSSize wellsize;
    NSSize imgsize;
    NSSize cursize;
    NSSize setsize;
    NSSize maxsize;
    float scale;
    
    maxsize = self.window.screen.frame.size;
    wellsize = imageView.frame.size;
    cursize = self.window.frame.size;
    
    maxsize.width = maxsize.width * 0.75 - (cursize.width - wellsize.width);
    maxsize.height = maxsize.height * 0.75 - (cursize.height - wellsize.height);
    
    NSArray * imageReps = imageView.image.representations;
    
    NSInteger width = 0;
    NSInteger height = 0;
    
    for (NSImageRep * imageRep in imageReps) {
        if (imageRep.pixelsWide > width) width = imageRep.pixelsWide;
        if (imageRep.pixelsHigh > height) height = imageRep.pixelsHigh;
    }
    
    imgsize.width = width;
    imgsize.height = height;
    
    imageView.image.size = imgsize; /* no steenkin' dpi here */
    
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
    
    frame = self.window.frame;
    frame.origin.y += frame.size.height;
    frame.size.width = setsize.width;
    frame.size.height = setsize.height;
    frame.origin.y -= setsize.height;
    [self.window setFrame: frame display: YES animate: animate];
}

- (void) windowDidLoad
{
    NSString *dirpath, *imgpath;
    NSImage *img;
    NSData *imgdata;
    const char *format;
    
    NSLog(@"infoctl: windowDidLoad");
    
    NSString *path = [_game urlForBookmark].path;
	if (path)
    	self.window.representedFilename = path;

    Metadata *meta = _game.metadata;
    self.window.title = [NSString stringWithFormat: @"%@ Info", meta.title];
    
    [descriptionText setDrawsBackground: NO];
    [(NSScrollView *)descriptionText.superview setDrawsBackground:NO];
    
    titleField.stringValue = meta.title;
    if (meta.author)
        authorField.stringValue = meta.author;
    if (meta.headline)
        headlineField.stringValue = meta.headline;
    if (meta.blurb)
        descriptionText.string = meta.blurb;

	dispatch_async(dispatch_get_main_queue(), ^{[self->ifidField
.window makeFirstResponder:nil];});

    format = babel_init((char*)path.UTF8String);
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
        ifid = @(buf);

		if (!ifid || [ifid isEqualToString:@""])
			ifid = _game.metadata.ifid;

        ifidField.stringValue = ifid;
        
        dirpath = (@"~/Library/Application Support/Spatterlight/Cover Art").stringByStandardizingPath;
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
                
//                rv =
				babel_treaty(GET_STORY_FILE_COVER_SEL, imgbuf, imglen);
                imgdata = [[NSData alloc] initWithBytesNoCopy: imgbuf length: imglen freeWhenDone: YES];
                img = [[NSImage alloc] initWithData: imgdata];
            }
            else if (meta.cover)
            {
                img = [[NSImage alloc] initWithData: (NSData *)meta.cover.data];
            }
        }

        if (img)
        {
            imageView.image = img;
        }
        
        [self sizeToFitImageAnimate: NO];
        
    finish:
        babel_release();
    }
	if ([ifidField.stringValue isEqualToString:@""])
		ifidField.stringValue = _game.metadata.ifid;
}

- (void) saveImage: sender
{
    NSURL *dirURL, *imgURL;
    NSData *imgdata;
    
    dirURL = [NSURL fileURLWithPath:(@"~/Library/Application Support/Spatterlight/Cover Art").stringByExpandingTildeInPath isDirectory:YES];
    imgURL = [NSURL fileURLWithPath: [ [dirURL.path stringByAppendingPathComponent: ifid] stringByAppendingPathExtension: @"tiff"] isDirectory:NO];
    
    [[NSFileManager defaultManager] createDirectoryAtURL:dirURL withIntermediateDirectories:YES attributes:nil error:NULL];
    
    NSLog(@"infoctl: save image %@", imgURL);
    
    imgdata = [imageView.image TIFFRepresentationUsingCompression: NSTIFFCompressionLZW factor: 0];
    [imgdata writeToURL: imgURL atomically: YES];
    
    [self sizeToFitImageAnimate: YES];
}

- (void)controlTextDidEndEditing:(NSNotification *)notification
{

	NSLog(@"InfoController: controlTextDidEndEditing: %@", notification.object);

	if ([notification.object isKindOfClass:[NSTextField class]])
	{
		NSTextField *textfield = notification.object;
		NSString *target = nil;

		if (textfield == titleField)
		{
			target = @"title";
			NSLog(@"InfoController: controlTextDidEndEditing: titleField");
		}
		else if (textfield == headlineField)
		{
			target = @"headline";
			NSLog(@"InfoController: controlTextDidEndEditing: headlineField");
		}
		else if (textfield == authorField)
		{
			target = @"author";
			NSLog(@"InfoController: controlTextDidEndEditing: authorField");
		}
		else if (textfield == ifidField)
		{
			NSLog(@"InfoController: controlTextDidEndEditing: ifidField");
			ifidField.stringValue = [ifidField.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
			if ([ifidField.stringValue isEqualToString:@""])
				_game.metadata.ifid = nil;
			else _game.metadata.ifid = ifidField.stringValue;
		}

		if (target)
		{
			if ([[textfield.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] isEqualToString:@""])
			{
				textfield.stringValue = @"";
				[_game.metadata setValue:nil forKey:target];
			}
			else [_game.metadata setValue:textfield.stringValue forKey:target];

			[_libctl updateSideView];
			[_libctl updateTableViews];

		}
		dispatch_async(dispatch_get_main_queue(), ^{[textfield.window makeFirstResponder:nil];});
	}
}

- (void)textDidEndEditing:(NSNotification *)notification
{
	NSLog (@"Game description did end editing!");
	if (notification.object == descriptionText)
		[self updateBlurb];
}

- (void)updateBlurb
{
	_game.metadata.blurb = descriptionText.textStorage.string;

	[_libctl updateSideView];
	[_libctl updateTableViews];
}

- (void) showWindow: sender
{
    [super showWindow: sender];
}

@end
