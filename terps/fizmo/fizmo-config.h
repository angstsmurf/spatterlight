/* fizmo-config.h: Header file for Fizmo configuration when building the IosFizmo project.
 for IosFizmo, an IosGlk port of the Fizmo Z-machine interpreter.
 Designed by Andrew Plotkin <erkyrath@eblong.com>
 http://eblong.com/zarf/glk/
 */

/* In the Unix Fizmo distribution, build configuration comes from Makefiles all down the build tree, which are in turn controlled by a top-level "config.mk" file. The iOS project (Xcode) does not use Makefiles. Instead, this header file supplies all the necessary #defines. (This header is included from the project's IosFizmo-Prefix.pch file.)
 */

#define DISABLE_BABEL 1
#define DISABLE_COMMAND_HISTORY 1
#define DISABLE_CONFIGFILES 1
#define DISABLE_FILELIST 1
#define DISABLE_OUTPUT_HISTORY 1
#define DISABLE_PREFIX_COMMANDS 1
#define DISABLE_SIGNAL_HANDLERS 1
#define DISABLE_BLOCKBUFFER 1

/* This sends TRACE_LOG output to the Xcode standard console. (Fizmo is very verbose, so we leave it off.) */
//#define ENABLE_IOS_TRACING 1
