//
//  MIDISequenceUtils.h
//  Spatterlight
//
//  What MIDIPlayer does to a MusicSequence before and while playing it. These
//  are pure functions of the sequence and need no audio graph, so they can be
//  tested on sequences built by hand.
//

#import <AudioToolbox/AudioToolbox.h>

NS_ASSUME_NONNULL_BEGIN

/// Call `visit` for every track of `sequence`, then for its tempo track if
/// `includeTempoTrack`.
void MIDISequenceForEachTrack(MusicSequence sequence, BOOL includeTempoTrack, void (NS_NOESCAPE ^visit)(MusicTrack track));

/// Call `visit` for every event of `track` in time order, until it returns NO.
void MIDITrackForEachEvent(MusicTrack track, BOOL (NS_NOESCAPE ^visit)(MusicTimeStamp stamp, MusicEventType type, const void *data, UInt32 size));

/// Length of the longest track in the sequence, in beats, or 0 if unknown.
MusicTimeStamp MIDISequenceLength(MusicSequence sequence);

/// Slide the sequence back over a leading rest of a quarter second or more,
/// leaving the setup events at time zero.
void MIDISequenceTrimLeadingSilence(MusicSequence sequence);

/// Pass `send` the messages that bring a synth to the state the sequence has
/// left it in by `beats`: sysex first, then controllers, programs and pitch
/// bends, channel by channel.
void MIDISequenceChaseSynthState(MusicSequence sequence, MusicTimeStamp beats, void (NS_NOESCAPE ^send)(const UInt8 *bytes, UInt32 length));

NS_ASSUME_NONNULL_END
