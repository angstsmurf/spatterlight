#import "main.h"

#include <signal.h>

NSString *stringWithLatin1String(const char *cString, int len)
{
    return [[[NSString alloc]
		initWithData: [NSData dataWithBytes: cString length: len]
		    encoding: NSISOLatin1StringEncoding] autorelease];
}

const char *latin1String(NSString *self)
{
    int i, len, ch;
    char *rv;
    
    len = [self length];
    rv = malloc(len + 1);
    for (i = 0; i < len; i++)
    {
	ch = [self characterAtIndex: i];
	if (ch < 256)
	    rv[i] = ch;
	else
	    rv[i] = '?';
    }
    rv[i] = '\0';
    
    // trick to "autorelease" returnValue...
    // I think that's the method used by Apple for the
    // "autoreleased" result of -cString (see NSString doc)
    [NSData dataWithBytesNoCopy: rv length: len+1];
    
    return rv;
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN); /* please don't die because a terp closed */
    return NSApplicationMain(argc,  (const char **) argv);
}
