//
//  MIDISequenceUtils.m
//  Spatterlight
//

#import "MIDISequenceUtils.h"

void MIDISequenceForEachTrack(MusicSequence sequence, BOOL includeTempoTrack, void (NS_NOESCAPE ^visit)(MusicTrack track)) {
    UInt32 numberOfTracks = 0;
    if (MusicSequenceGetTrackCount(sequence, &numberOfTracks) != noErr)
        numberOfTracks = 0;
    for (UInt32 i = 0; i < numberOfTracks; i++) {
        MusicTrack track = NULL;
        if (MusicSequenceGetIndTrack(sequence, i, &track) == noErr && track)
            visit(track);
    }
    if (includeTempoTrack) {
        MusicTrack tempoTrack = NULL;
        if (MusicSequenceGetTempoTrack(sequence, &tempoTrack) == noErr && tempoTrack)
            visit(tempoTrack);
    }
}

void MIDITrackForEachEvent(MusicTrack track, BOOL (NS_NOESCAPE ^visit)(MusicTimeStamp stamp, MusicEventType type, const void *data, UInt32 size)) {
    MusicEventIterator iterator = NULL;
    if (NewMusicEventIterator(track, &iterator) != noErr)
        return;

    Boolean hasEvent = false;
    MusicEventIteratorHasCurrentEvent(iterator, &hasEvent);
    while (hasEvent) {
        MusicTimeStamp stamp = 0;
        MusicEventType type = 0;
        const void *data = NULL;
        UInt32 size = 0;
        if (MusicEventIteratorGetEventInfo(iterator, &stamp, &type, &data, &size) != noErr)
            break;
        if (!visit(stamp, type, data, size))
            break;
        if (MusicEventIteratorNextEvent(iterator) != noErr)
            break;
        MusicEventIteratorHasCurrentEvent(iterator, &hasEvent);
    }
    DisposeMusicEventIterator(iterator);
}

MusicTimeStamp MIDISequenceLength(MusicSequence sequence) {
    __block MusicTimeStamp longest = 0;
    MIDISequenceForEachTrack(sequence, NO, ^(MusicTrack track) {
        MusicTimeStamp trackLen = 0;
        UInt32 trackLenLen = sizeof(trackLen);
        if (MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &trackLen, &trackLenLen) == noErr
            && trackLen > longest)
            longest = trackLen;
    });
    return longest;
}

/* Shortest leading rest worth removing, in seconds. */
static const Float64 kMinimumTrimmedSilence = 0.25;

/* Setup events -- program changes, controllers, a General MIDI reset -- sit at
   time zero and stay there when the music slides back, so leave the first note a
   sliver of room to land after them instead of exactly on top. A hundredth of a
   beat is a few milliseconds. */
static const MusicTimeStamp kTrimHeadroom = 0.01;

static BOOL EventMakesSound(MusicEventType type, const void *data) {
    switch (type) {
        case kMusicEventType_MIDINoteMessage:
            return ((const MIDINoteMessage *)data)->velocity > 0;
        case kMusicEventType_MIDIChannelMessage: {
            const MIDIChannelMessage *message = data;
            return (message->status & 0xf0) == 0x90 && message->data2 > 0;
        }
        case kMusicEventType_ExtendedNote:
            return YES;
        default:
            return NO;
    }
}

/// How far into `track` the music can be moved up, given that it cannot pass
/// `limit`. Returns `limit` if the track asks for nothing tighter.
static MusicTimeStamp FirstTrimBarrier(MusicTrack track, MusicTimeStamp limit) {
    __block MusicTimeStamp barrier = limit;
    MIDITrackForEachEvent(track, ^BOOL(MusicTimeStamp stamp, MusicEventType type, const void *data, UInt32 size) {
        /* Events arrive in time order, so nothing further on can beat the limit. */
        if (stamp >= barrier)
            return NO;
        /* Anything after time zero stops us: only the events sitting at zero are
           left behind by the trim, and leaving a program change or a sysex reset
           stranded in the middle of the music would be worse than the rest we are
           trying to remove. Sound at time zero means there is no rest at all. */
        if (stamp > 0 || EventMakesSound(type, data)) {
            barrier = stamp;
            return NO;
        }
        return YES;
    });
    return barrier;
}

/// Move everything in `track` from `offset` onward back to the top of the track.
static void ShiftTrackBack(MusicTrack track, MusicTimeStamp offset) {
    MusicTimeStamp length = 0;
    UInt32 lengthSize = sizeof(length);
    BOOL hasLength = (MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &length, &lengthSize) == noErr);

    if (MusicTrackMoveEvents(track, offset, kMusicTimeStamp_EndOfTrack, -offset) != noErr)
        return;

    /* Track length can be set independently of the events it holds, so it does not
       necessarily follow them back. Shorten it by hand, or -loop: and -addCallback:
       will put the loop point and the end-of-track event past the real end. */
    if (hasLength) {
        length = (length > offset) ? length - offset : 0;
        MusicTrackSetProperty(track, kSequenceTrackProperty_TrackLength, &length, sizeof(length));
    }
}

/* Some MIDI files open with seconds of rest before the first note: the music in
   Hugo's Scourge of Merovia starts almost seven seconds in. Nothing is audible
   during that time, so slide the whole sequence back to the top.

   Seeking the player past the rest would be simpler, but the setup events all sit
   at time zero and would be skipped, leaving every voice on the wrong instrument. */
void MIDISequenceTrimLeadingSilence(MusicSequence sequence) {
    __block MusicTimeStamp offset = kMusicTimeStamp_EndOfTrack;
    MIDISequenceForEachTrack(sequence, YES, ^(MusicTrack track) {
        offset = FirstTrimBarrier(track, offset);
    });

    if (offset == kMusicTimeStamp_EndOfTrack || offset <= kTrimHeadroom)
        return;
    offset -= kTrimHeadroom;

    Float64 silence = 0;
    if (MusicSequenceGetSecondsForBeats(sequence, offset, &silence) != noErr
        || silence < kMinimumTrimmedSilence)
        return;

    MIDISequenceForEachTrack(sequence, YES, ^(MusicTrack track) {
        ShiftTrackBack(track, offset);
    });
}

/* Starting in the middle of a sequence means the synth never saw the events that
   set it up. Within a session that does not matter: pausing leaves the graph and
   its DLS synth alive, still holding every program change and controller the
   sequence has sent. A sequence revived from an autosave gets a fresh graph
   instead, and would resume on channel defaults -- grand piano, centre pan, no
   bank select -- so replay that state by hand before playing.

   Sysex is sent as it is found, which puts all of it ahead of the values below:
   a General MIDI reset wipes controller and program state, and we want ours to be
   what survives. Then come controllers in ascending order (a bank select has to
   arrive before the program change that consumes it), then programs, then pitch
   bends. Note-ons are skipped, or every note sounding at the resume point would
   come back stuck on. Replaying values cannot reproduce an RPN/NRPN entry
   sequence, which is rare enough in game music to live with. */
void MIDISequenceChaseSynthState(MusicSequence sequence, MusicTimeStamp beats, void (NS_NOESCAPE ^send)(const UInt8 *bytes, UInt32 length)) {
    if (beats <= 0)
        return;

    struct {
        UInt8 program[16];
        UInt8 controller[16][128];
        UInt8 bend[16][2];
        BOOL hasProgram[16], hasController[16][128], hasBend[16];
    } chased;
    memset(&chased, 0, sizeof(chased));
    typeof(chased) *state = &chased;

    MIDISequenceForEachTrack(sequence, NO, ^(MusicTrack track) {
        MIDITrackForEachEvent(track, ^BOOL(MusicTimeStamp stamp, MusicEventType type, const void *data, UInt32 size) {
            /* Events arrive in time order, so the rest of the track is in the future. */
            if (stamp >= beats)
                return NO;

            if (type == kMusicEventType_MIDIChannelMessage) {
                const MIDIChannelMessage *message = data;
                UInt8 channel = message->status & 0x0f;
                switch (message->status & 0xf0) {
                    case 0xb0:
                        if (message->data1 < 128) {
                            state->controller[channel][message->data1] = message->data2;
                            state->hasController[channel][message->data1] = YES;
                        }
                        break;
                    case 0xc0:
                        state->program[channel] = message->data1;
                        state->hasProgram[channel] = YES;
                        break;
                    case 0xe0:
                        state->bend[channel][0] = message->data1;
                        state->bend[channel][1] = message->data2;
                        state->hasBend[channel] = YES;
                        break;
                    default:
                        break;
                }
            } else if (type == kMusicEventType_MIDIRawData) {
                const MIDIRawData *raw = data;
                if (raw->length > 0 && raw->data[0] == 0xf0)
                    send(raw->data, raw->length);
            }
            return YES;
        });
    });

    for (UInt8 channel = 0; channel < 16; channel++) {
        for (UInt8 number = 0; number < 128; number++) {
            if (state->hasController[channel][number]) {
                const UInt8 message[3] = { (UInt8)(0xb0 | channel), number, state->controller[channel][number] };
                send(message, 3);
            }
        }
        if (state->hasProgram[channel]) {
            const UInt8 message[2] = { (UInt8)(0xc0 | channel), state->program[channel] };
            send(message, 2);
        }
        if (state->hasBend[channel]) {
            const UInt8 message[3] = { (UInt8)(0xe0 | channel), state->bend[channel][0], state->bend[channel][1] };
            send(message, 3);
        }
    }
}
