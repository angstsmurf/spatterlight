//
//  BocfelSoundStateTests.mm
//  SpatterlightTests
//
//  Regression test for issue #166: with "Enable sound" unchecked in the
//  theme, gestalt_Sound2 reports no sound support, so bocfel never creates
//  any sound channels. The Spatterlight-only autosave helpers
//  stash_library_sound_state / recover_library_sound_state then used to call
//  channels.at(Channels::Effects) on the empty channel map, throwing
//  std::out_of_range into bocfel's terminate handler (SIGABRT at the first
//  prompt, or instantly at launch when an autosave existed).
//
//  Like QuestionRegressionTests, this unity-includes the terp source directly,
//  with just enough Glk stubbed out to drive both the empty-channels and the
//  loaded-channels paths.
//

#import <XCTest/XCTest.h>

#include <cstring>
#include <initializer_list>
#include <exception>

// Compile sound.cpp the way the bocfel target does, minus Blorb support
// (ZTERP_GLK_BLORB), which only matters for looping-sound tables.
#define SPATTERLIGHT
#define ZTERP_GLK

#include "../terps/bocfel/sound.cpp"

// --- Glk stubs ---------------------------------------------------------------
// Just enough to let init_sound() either fail (sound disabled) or build its
// effects + music channels (sound enabled), and to resolve link references
// from the parts of sound.cpp the test never runs.

static bool stub_sound_supported = false;
static int stub_channels_created = 0;
static struct glk_schannel_struct stub_channel_pool[16];

// The Glk channel list, as gli_schan_for_tag and glk_schannel_iterate see
// it. Autorestore replaces it wholesale; stub_restore_list models that.
static schanid_t stub_list[16];
static int stub_list_count = 0;

static schanid_t stub_new_channel(int tag)
{
    schanid_t chan = &stub_channel_pool[stub_channels_created++];
    chan->tag = tag;
    stub_list[stub_list_count++] = chan;
    return chan;
}

// Replace the channel list with channels carrying these tags.
static void stub_restore_list(std::initializer_list<int> tags)
{
    stub_list_count = 0;
    for (int tag : tags)
        stub_new_channel(tag);
}

extern "C" {

glui32 glk_gestalt(glui32 sel, glui32 val)
{
    return sel == gestalt_Sound2 && stub_sound_supported;
}

schanid_t glk_schannel_create(glui32 rock)
{
    if (!stub_sound_supported)
        return NULL;
    return stub_new_channel(4711 + stub_channels_created + 1); // 4712 = effects, 4713 = music
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rockptr)
{
    int i = 0;
    if (chan) {
        while (i < stub_list_count && stub_list[i] != chan)
            i++;
        i++;
    }
    return i < stub_list_count ? stub_list[i] : NULL;
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol) {}
glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify) { return 0; }
void glk_schannel_stop(schanid_t chan) {}
void glk_sound_load_hint(glui32 snd, glui32 flag) {}
void win_beep(int type) {}

channel_t *gli_schan_for_tag(int tag)
{
    for (int i = 0; i < stub_list_count; i++)
        if (stub_list[i]->tag == tag)
            return stub_list[i];
    return NULL;
}

} // extern "C"

// bocfel globals referenced by zsound_effect() (never executed here).
std::array<uint16_t, 8> zargs;
int znargs;
int zversion;
uint16_t zarg_or(int n, uint16_t def) { return def; }
bool is_game(Game game) { return false; }

@interface BocfelSoundStateTests : XCTestCase
@end

@implementation BocfelSoundStateTests

// One method for the whole scenario: `channels` in sound.cpp is file-static
// and cannot be reset, so the empty-channels checks must run before the
// channels are created.
- (void)testStashAndRecoverSoundState {
    // --- Sound disabled in settings: init_sound() leaves the map empty.
    stub_sound_supported = false;
    init_sound();
    XCTAssertFalse(sound_loaded(), @"channels created despite gestalt_Sound2 = 0");

    library_state_data dat;
    memset(&dat, 0x5A, sizeof dat);
    library_state_data before = dat;

    // Issue #166: these threw std::out_of_range from channels.at() when no
    // channels existed. They must be silent no-ops instead.
    try {
        stash_library_sound_state(&dat);
        recover_library_sound_state(&dat);
        stash_library_sound_state(NULL);
        recover_library_sound_state(NULL);
    } catch (const std::exception &e) {
        XCTFail(@"stash/recover with no sound channels threw: %s", e.what());
    }
    XCTAssertEqual(memcmp(&dat, &before, sizeof dat), 0,
                   @"stash/recover touched the state despite having no channels");

    // --- Sound enabled: the new guard must not break real stash/recover.
    stub_sound_supported = true;
    init_sound();
    XCTAssertTrue(sound_loaded(), @"channels not created with sound enabled");

    memset(&dat, 0, sizeof dat);
    stash_library_sound_state(&dat);
    XCTAssertEqual(dat.autosave_version, 1);
    XCTAssertEqual(dat.sound_channel_tag, 4712, @"stash did not record the effects channel's tag");

    XCTAssertEqual(dat.music_channel_tag, 4713, @"stash did not record the music channel's tag");

    // Round-trip: recover autosaved state, then stash it back out again.
    // Autorestore has replaced the channel list with the saved channels.
    stub_restore_list({31337, 31338});
    dat.routine = 7;
    dat.queued_sound = 3;
    dat.queued_volume = 5;
    dat.sound_channel_tag = 31337;
    dat.music_channel_tag = 31338;
    recover_library_sound_state(&dat);

    library_state_data out;
    memset(&out, 0, sizeof out);
    stash_library_sound_state(&out);
    XCTAssertEqual(out.routine, (uint16_t)7);
    XCTAssertEqual(out.queued_sound, 3);
    XCTAssertEqual(out.queued_volume, 5);
    XCTAssertEqual(out.sound_channel_tag, 31337,
                   @"recover did not reattach the channel found by gli_schan_for_tag");
    XCTAssertEqual(out.music_channel_tag, 31338,
                   @"recover did not reattach the music channel");

    // Saved tags missing from the restored list (an old autosave without the
    // music tag, or a mismatched one): the effects channel used to become
    // NULL and crash the next stash; music was left pointing at a freed
    // channel. Effects falls back to the restored channel, music to a new one.
    stub_restore_list({500});
    dat.sound_channel_tag = 999;
    dat.music_channel_tag = 0;
    recover_library_sound_state(&dat);
    XCTAssertEqual(channels.at(Channels::Effects)->channel->tag, 500,
                   @"effects did not fall back to the restored channel");
    schanid_t music = channels.at(Channels::Music)->channel;
    XCTAssertTrue(music != NULL && music != channels.at(Channels::Effects)->channel,
                  @"music was not given a channel of its own");

    memset(&out, 0, sizeof out);
    stash_library_sound_state(&out);
    XCTAssertEqual(out.sound_channel_tag, 500);
    XCTAssertEqual(out.music_channel_tag, music->tag);

    // No channels restored at all: both get fresh, distinct channels.
    stub_restore_list({});
    recover_library_sound_state(&dat);
    XCTAssertTrue(channels.at(Channels::Effects)->channel != NULL);
    XCTAssertTrue(channels.at(Channels::Music)->channel != NULL);
    XCTAssertTrue(channels.at(Channels::Effects)->channel != channels.at(Channels::Music)->channel);
}

@end
