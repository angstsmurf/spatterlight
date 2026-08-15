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
//  Like GeasRegressionTests, this unity-includes the terp source directly,
//  with just enough Glk stubbed out to drive both the empty-channels and the
//  loaded-channels paths.
//

#import <XCTest/XCTest.h>

#include <cstring>
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
static struct glk_schannel_struct stub_channel_pool[4];
static struct glk_schannel_struct stub_recovered_channel;

extern "C" {

glui32 glk_gestalt(glui32 sel, glui32 val)
{
    return sel == gestalt_Sound2 && stub_sound_supported;
}

schanid_t glk_schannel_create(glui32 rock)
{
    if (!stub_sound_supported)
        return NULL;
    schanid_t chan = &stub_channel_pool[stub_channels_created++];
    chan->tag = 4711 + stub_channels_created; // 4712 = effects, 4713 = music
    return chan;
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol) {}
glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify) { return 0; }
void glk_schannel_stop(schanid_t chan) {}
void glk_sound_load_hint(glui32 snd, glui32 flag) {}
void win_beep(int type) {}

channel_t *gli_schan_for_tag(int tag)
{
    stub_recovered_channel.tag = tag;
    return &stub_recovered_channel;
}

} // extern "C"

// bocfel globals referenced by zsound_effect() (never executed here).
std::array<uint16_t, 8> zargs;
int znargs;
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

    // Round-trip: recover autosaved state, then stash it back out again.
    dat.routine = 7;
    dat.queued_sound = 3;
    dat.queued_volume = 5;
    dat.sound_channel_tag = 31337;
    recover_library_sound_state(&dat);

    library_state_data out;
    memset(&out, 0, sizeof out);
    stash_library_sound_state(&out);
    XCTAssertEqual(out.routine, (uint16_t)7);
    XCTAssertEqual(out.queued_sound, 3);
    XCTAssertEqual(out.queued_volume, 5);
    XCTAssertEqual(out.sound_channel_tag, 31337,
                   @"recover did not reattach the channel found by gli_schan_for_tag");
}

@end
