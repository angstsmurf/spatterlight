/*  TempSChannel.h: Temporary sound channel objc class
for serializing, inspired by IosGlk, the iOS
implementation of the Glk API by Andrew Plotkin
*/

#include "glk.h"
#include "gi_dispa.h"
#include "glkimp.h"

#import <Foundation/Foundation.h>

//struct glk_schannel_struct
//{
//    glui32 rock;
//
//    void *sample; /* Mix_Chunk (or FMOD Sound) */
//    void *music; /* Mix_Music (or FMOD Music) */
//    void *decode; /* Sound_Sample */
//
//    void *sdl_rwops; /* SDL_RWops */
//    unsigned char *sdl_memory;
//    int sdl_channel;
//
//    int resid; /* for notifies */
//    int status;
//    int channel;
//    int volume;
//    glui32 loop;
//    int notify;
//    int buffered;
//    int paused;
//
//    /* for volume fades */
//    int volume_notify;
//    int volume_timeout;
//    int target_volume;
//    double float_volume;
//    double volume_delta;
//    int timer;
//
//    gidispatch_rock_t disprock;
//    channel_t *chain_next, *chain_prev;
//};

@interface TempSChannel : NSObject <NSSecureCoding> {

    int sdl_channel;

    int resid; /* for notifies */
    int status;
    int channel;
    int volume;
    glui32 loop;
    int notify;
    int buffered;
    int paused;

    /* for volume fades */
    int volume_notify;
    int volume_timeout;
    int target_volume;
    CGFloat float_volume;
    CGFloat volume_delta;

}

@property glui32 tag;
@property glui32 rock;
@property glui32 prev;
@property glui32 next;

- (id) initWithCStruct:(channel_t *)chan;
- (void) copyToCStruct:(channel_t *)chan;
- (void) restartInternal;

@end
