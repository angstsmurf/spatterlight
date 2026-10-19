# Sounds — remaining items

Review of 2026-10-19; the fixes went in the same day (A1–A9, A12–A20, B1,
B2, B4–B7, B9, C1–C8, D1–D4, most of D5, most of E). What is left needs a
behaviour decision or a runtime check, or is not worth doing on its own.
Tests: `SoundHandlerTests` and `MIDISequenceUtilsTests`, run one class at a
time through the isolation rig, never the whole bundle.

## Settled without a change

- **A10.** Timed playback in `testMIDIRepeatsArePasses` and
  `testMIDIFinishesWithLongestTrack` shows that `loopCount = N` plays N
  passes, and that the SMF loader gives every track the sequence's length.
  So looping tracks stay in step, and the end event on track 0 fires at the
  real end.
- **A11.** The two repeat models remain, but each lives inside its backend
  and is reported as a resume pair (`resumePosition` + `resumeRepeats`), so
  the channel no longer has to know which one it holds.

## Needs a runtime check

- **Now Playing after D1.** Nobody has driven the app through the
  `media-control` + Soundtest.gblorb harness since the channel/backend
  change: play/pause from Control Center, a second game taking the session
  over, music restored from an autosave. The MIDI path has never been run
  against Now Playing.
- **A21 [U].** The fade `NSTimer` is scheduled on the caller's run loop and
  retains the channel until it ends. Glk requests arrive on the main thread
  today, so it fires; a main-queue `dispatch_source`, cancelled in `dealloc`,
  would not depend on that.

## Needs a decision

- **B3.** `GlkSoundBlorbFormatAIFF` is never produced: sniffing reports AIFF
  as `FORM`. `GlkSoundBlorbFormatMod` only maps to `audio/mod`. Either make
  them real formats or remove them, keeping the raw enum values stable for
  archived `SoundResource`s.
- **B13 [U].** `SoundResource.data` stays loaded once read. A large Blorb
  with many sounds stays resident; evicting after a sound finishes would
  need a reload path that does not stall the main thread.
- **D5 [U] graph reuse.** `MIDIPlayer` builds a new DLS synth + mixer +
  output `AUGraph` for every play. One graph per backend, reused, would cut
  start latency, but the synth then keeps the previous sound's programs and
  controllers between sounds.

## Small cleanup, low value alone

- **B8.** `glui32`/`glsi32` typedefs are repeated in `GlkSoundChannel.h`,
  `SoundHandler.h` and `GlkSoundBackend.h`. `MAXSND`, `FADE_GRANULARITY` and
  `GLK_MAXVOLUME` are macros in public headers. One `SoundConstants.h`.
- **B10.** `GlkSoundChannelStatus` has two cases and is only compared with
  `Idle`: effectively an `isPlaying` BOOL. It is archived as an integer, so
  keep the key.
- **B11.** Snake case in `GlkSoundChannel`: `init_fade:duration:notify:`,
  `volume_notify`, `volume_timeout`, `volume_tick`, `target_volume`,
  `volume_delta`. Tests reach `volume`, `target_volume` and `volume_timeout`
  by KVC, so rename those together.
- **B12.** `MIDIPlayer.m` ignores the `OSStatus` of every `AUGraph*` and
  `MusicPlayer*` call; a failed `AUGraphStart` goes unnoticed. `AUGraph` is
  deprecated in favour of `AVAudioEngine` + `AVAudioUnitSampler`.
- **C9.** The coders mix `encodeInt:`/`encodeInteger:`/`encodeInt32:` for
  the same kinds of value (`volume_notify` is written with `encodeInt:` and
  read with `decodeInt32ForKey:`). Harmless in practice, but untidy; any
  change must still read old autosaves.

## Tests not written

- `CFDataInputSource` read/seek/EOF edge cases (A19 is fixed). The class
  is file-private in `SFBSoundBackend.mm`; it would need a header to be
  tested directly.
- `NowPlayingCoordinator` with fake handlers: claim/release between two
  handlers, and the looping-or-over-5-s threshold. The coordinator is a
  singleton that talks to `MPNowPlayingInfoCenter`, so this needs a seam
  first.
