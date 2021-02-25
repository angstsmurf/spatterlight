#import "InfoController.h"

#import "Game.h"
#import "Metadata.h"
#import "LibController.h"
#import "CoreDataManager.h"
#import "Image.h"
#import "IFDBDownloader.h"
#import "main.h"

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
    fprintf(stderr, "%s\n",                                                    \
            [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);
#else
#define NSLog(...)
#endif

@interface InfoController () <NSWindowDelegate, NSTextFieldDelegate, NSTextViewDelegate>
{
    IBOutlet NSTextField *titleField;
    IBOutlet NSTextField *authorField;
    IBOutlet NSTextField *headlineField;
    IBOutlet NSTextField *ifidField;
    IBOutlet NSTextView *descriptionText;
    IBOutlet NSImageView *imageView;

    CoreDataManager *coreDataManager;
    NSManagedObjectContext *managedObjectContext;
}
@end

@implementation InfoController

- (instancetype)init {
    self = [super initWithWindowNibName:@"InfoPanel"];
    if (self) {
        coreDataManager = ((AppDelegate*)[NSApplication sharedApplication].delegate).coreDataManager;
        managedObjectContext = coreDataManager.mainManagedObjectContext;
    }

    return self;
}

- (instancetype)initWithGame:(Game *)game  {
    self = [self init];
    if (self) {
        _game = game;
        _path = [game urlForBookmark].path;
        if (!_path)
            _path = game.path;
        _meta = game.metadata;
    }
    return self;
}

- (instancetype)initWithpath:(NSString *)path {
    self = [self init];
    if (self) {
        _path = path;
        _game = [self fetchGameWithPath:path];
        if (_game)
            _meta = _game.metadata;
    }
    return self;
}

- (Game *)fetchGameWithPath:(NSString *)path {
    NSError *error = nil;
    NSArray *fetchedObjects;

    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];

    fetchRequest.entity = [NSEntityDescription entityForName:@"Game" inManagedObjectContext:managedObjectContext];
    fetchRequest.predicate = [NSPredicate predicateWithFormat:@"path like[c] %@",path];

    fetchedObjects = [managedObjectContext executeFetchRequest:fetchRequest error:&error];
    if (fetchedObjects == nil) {
        NSLog(@"Problem! %@",error);
    }

    if (fetchedObjects.count > 1)
    {
        NSLog(@"Found more than one entry with path %@",path);
    }
    else if (fetchedObjects.count == 0)
    {
        NSLog(@"fetchGameWithPath: Found no Game object with path %@", path);
        return nil;
    }

    return fetchedObjects[0];
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
    if (NSMaxY(frame) > NSMaxY(self.window.screen.frame))
        frame.origin.y = NSMaxY(self.window.screen.frame) - frame.size.height;
    [self.window setFrame:frame display:YES animate:animate];
}

- (void)noteManagedObjectContextDidChange:(NSNotification *)notification {
    NSArray *updatedObjects = (notification.userInfo)[NSUpdatedObjectsKey];
    NSArray *insertedObjects = (notification.userInfo)[NSInsertedObjectsKey];
    NSArray *refreshedObjects = (notification.userInfo)[NSRefreshedObjectsKey];

    if ([updatedObjects containsObject:_meta] || [updatedObjects containsObject:_game])
    {
        [self update];

        if (_meta.cover && ([insertedObjects containsObject:_meta.cover] || [refreshedObjects containsObject:_meta.cover] || [updatedObjects containsObject:_meta.cover])) {
            [self updateImage];
        }
    }
}

- (void)update {
    if (!_path)
        _path = _game.urlForBookmark.path;
    if (!_path)
        _path = _game.path;
    self.window.representedFilename = _path;
    self.window.title =
    [NSString stringWithFormat:@"%@ Info", _meta.title];

    if (_meta) {
        titleField.stringValue = _meta.title;
        if (_meta.author)
            authorField.stringValue = _meta.author;
        if (_meta.headline)
            headlineField.stringValue = _meta.headline;
        if (_meta.blurb)
            descriptionText.string = _meta.blurb;
        ifidField.stringValue = _game.ifid;
    }
}

- (void)updateImage {
    if (_meta.cover) {
        imageView.image = [[NSImage alloc] initWithData:(NSData *)_meta.cover.data];
        imageView.accessibilityLabel = _meta.coverArtDescription;
    }
    [self sizeToFitImageAnimate:NO];
}


- (void)windowDidLoad {
//    NSLog(@"infoctl: windowDidLoad");
     [[NSNotificationCenter defaultCenter]
     addObserver:self
     selector:@selector(noteManagedObjectContextDidChange:)
     name:NSManagedObjectContextObjectsDidChangeNotification
     object:managedObjectContext];

    descriptionText.drawsBackground = NO;
    ((NSScrollView *)descriptionText.superview).drawsBackground = NO;
    
    [self update];
    [self updateImage];

    titleField.editable = YES;
    titleField.delegate = self;

    authorField.editable = YES;
    authorField.delegate = self;
    
    headlineField.editable = YES;
    headlineField.delegate = self;

//    ifidField.editable = YES;
//    ifidField.delegate = self;

    descriptionText.editable = YES;
    descriptionText.delegate = self;

    [self.window makeFirstResponder:imageView];

    self.window.delegate = self;
    
}

+ (NSArray *)restorableStateKeyPaths {
    return @[
             @"path", @"titleField.stringValue", @"authorField.stringValue",
             @"headlineField.stringValue", @"descriptionText.string"
             ];
}

- (void)windowWillClose:(NSNotification *)notification {
    LibController *libcontroller = ((AppDelegate *)[NSApplication sharedApplication].delegate).libctl;
    // It seems we have to do it in this cumbersome way because the game.path used for key may have changed.
    // Probably a good reason to use something else as key.
    for (InfoController *controller in [libcontroller.infoWindows allValues])
        if (controller == self) {
            NSArray *temp = [libcontroller.infoWindows allKeysForObject:controller];
            NSString *key = [temp objectAtIndex:0];
            if (key) {
                [libcontroller.infoWindows removeObjectForKey:key];
                return;
            }
        }
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
              fileURLWithPath:[[dirURL.path stringByAppendingPathComponent:_game.ifid]
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
    if (imgdata) {
        [imgdata writeToURL:imgURL atomically:YES];
        _meta.coverArtURL = imgURL.path;
        IFDBDownloader *downloader = [[IFDBDownloader alloc] initWithContext:managedObjectContext];
        // Check if we already have created an image object for this game
        // with a file in Application Support as its originalURL
        Image *image = [downloader fetchImageForURL:imgURL.path];
        if (image) {
            _meta.cover = image;
            image.data = imgdata;
        } else {
            // If not, create a new one
            [downloader insertImage:imgdata inMetadata:_meta];
        }
        _meta.userEdited = @(YES);
        _meta.source = @(kUser);
    }

    [self sizeToFitImageAnimate:YES];
}

- (void)controlTextDidEndEditing:(NSNotification *)notification
{
	if ([notification.object isKindOfClass:[NSTextField class]])
	{
		NSTextField *textfield = notification.object;

		if (textfield == titleField)
		{
			_meta.title = titleField.stringValue;
		}
		else if (textfield == headlineField)
		{
			_meta.headline = headlineField.stringValue;
		}
		else if (textfield == authorField)
		{
			_meta.author = authorField.stringValue;
		}
//		else if (textfield == ifidField)
//		{
//			_game.ifid = [ifidField.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
//		}

        dispatch_async(dispatch_get_main_queue(), ^{[textfield.window makeFirstResponder:nil];});

	}
}

- (void)textDidEndEditing:(NSNotification *)notification {
    if (notification.object == descriptionText) {
        _meta.blurb = descriptionText.textStorage.string;
    }
}


- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window {
    return managedObjectContext.undoManager;
}

@end
