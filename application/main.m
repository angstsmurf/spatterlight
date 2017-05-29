#import "main.h"

#include <signal.h>

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN); /* please don't die because a terp closed */
    return NSApplicationMain(argc,  (const char **) argv);
}
