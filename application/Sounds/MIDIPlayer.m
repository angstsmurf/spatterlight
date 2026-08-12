//
//  MIDIPlayer.m
//  MIDIPlayer
//
//  Based on code by 谢小进.
//

#import <AudioToolbox/AudioToolbox.h>

#import "MIDIPlayer.h"
#import "SoundHandler.h"


@interface MIDIPlayer() {
    AUGraph graph;
    AUNode sampleNode;
    AUNode ioNode;
    AUNode mixerNode;

    AudioUnit sampleUnit;
    AudioUnit ioUnit;
    AudioUnit mixerUnit;

    MusicPlayer player;
    MusicSequence sequence;
    MusicTimeStamp pausedTime;
    BOOL hasPausedTime;
}

@end

@implementation MIDIPlayer

- (instancetype)initWithData:(NSData *)data {
    if (self = [super init]) {
        hasPausedTime = NO;
        pausedTime = 0;
        [self createGraph];
        [self startGraph];
        [self loadData:data];
    }
    return self;
}

- (void)createGraph {
    NewAUGraph(&graph);

    AudioComponentDescription sampleDesc = {};
    sampleDesc.componentType            = kAudioUnitType_MusicDevice;
    sampleDesc.componentSubType         = kAudioUnitSubType_DLSSynth;
    sampleDesc.componentManufacturer    = kAudioUnitManufacturer_Apple;
    sampleDesc.componentFlags           = 0;
    sampleDesc.componentFlagsMask       = 0;
    AUGraphAddNode(graph, &sampleDesc, &sampleNode);

    AudioComponentDescription ioUnitDesc;
    ioUnitDesc.componentType            = kAudioUnitType_Output;
    ioUnitDesc.componentSubType         = kAudioUnitSubType_DefaultOutput;
    ioUnitDesc.componentManufacturer    = kAudioUnitManufacturer_Apple;
    ioUnitDesc.componentFlags           = 0;
    ioUnitDesc.componentFlagsMask       = 0;
    AUGraphAddNode(graph, &ioUnitDesc, &ioNode);

    // A description of the mixer unit
    AudioComponentDescription mixerDescription;
    mixerDescription.componentType = kAudioUnitType_Mixer;
    mixerDescription.componentSubType = kAudioUnitSubType_MultiChannelMixer;
    mixerDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    mixerDescription.componentFlags = 0;
    mixerDescription.componentFlagsMask = 0;

    AUGraphAddNode(graph, &mixerDescription, &mixerNode);
    AUGraphOpen(graph);
    AUGraphNodeInfo(graph, sampleNode, NULL, &sampleUnit);
    AUGraphNodeInfo(graph, ioNode, NULL, &ioUnit);
    AUGraphNodeInfo(graph, mixerNode, NULL, &mixerUnit);

    AUGraphConnectNodeInput(graph, sampleNode, 0, mixerNode, 0);
    AUGraphConnectNodeInput(graph, mixerNode, 0, ioNode, 0);

    [self setVolume:1.0];

    AUGraphInitialize(graph);
}

- (void)setVolume:(CGFloat)volume {
    if (mixerUnit) {
        AudioUnitSetParameter(mixerUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Output, 0, (AudioUnitParameterValue)volume, 0);
        AudioUnitSetParameter(mixerUnit, kMultiChannelMixerParam_Volume, kAudioUnitScope_Input, 0, (AudioUnitParameterValue)volume, 0);
    }
}

- (void)startGraph {
    if (graph) {
        Boolean isRunning;
        AUGraphIsRunning(graph, &isRunning);
        if (!isRunning) {
            AUGraphStart(graph);
        }
    }
}

- (void)stopGraph {
    if (graph) {
        Boolean isRunning;
        AUGraphIsRunning (graph, &isRunning);
        if (isRunning) {
            AUGraphStop(graph);
        }
    }
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
    MusicEventIterator iterator = NULL;
    if (NewMusicEventIterator(track, &iterator) != noErr)
        return limit;

    Boolean hasEvent = false;
    MusicEventIteratorHasCurrentEvent(iterator, &hasEvent);
    while (hasEvent) {
        MusicTimeStamp stamp = 0;
        MusicEventType type = 0;
        const void *data = NULL;
        UInt32 size = 0;
        if (MusicEventIteratorGetEventInfo(iterator, &stamp, &type, &data, &size) != noErr)
            break;
        /* Events arrive in time order, so nothing further on can beat the limit. */
        if (stamp >= limit)
            break;
        /* Anything after time zero stops us: only the events sitting at zero are
           left behind by the trim, and leaving a program change or a sysex reset
           stranded in the middle of the music would be worse than the rest we are
           trying to remove. Sound at time zero means there is no rest at all. */
        if (stamp > 0 || EventMakesSound(type, data)) {
            limit = stamp;
            break;
        }
        if (MusicEventIteratorNextEvent(iterator) != noErr)
            break;
        MusicEventIteratorHasCurrentEvent(iterator, &hasEvent);
    }
    DisposeMusicEventIterator(iterator);
    return limit;
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
static void TrimLeadingSilence(MusicSequence sequence) {
    UInt32 numberOfTracks = 0;
    if (MusicSequenceGetTrackCount(sequence, &numberOfTracks) != noErr)
        return;

    MusicTrack tempoTrack = NULL;
    if (MusicSequenceGetTempoTrack(sequence, &tempoTrack) != noErr)
        tempoTrack = NULL;

    MusicTimeStamp offset = kMusicTimeStamp_EndOfTrack;
    for (UInt32 i = 0; i < numberOfTracks; i++) {
        MusicTrack track = NULL;
        if (MusicSequenceGetIndTrack(sequence, i, &track) == noErr)
            offset = FirstTrimBarrier(track, offset);
    }
    if (tempoTrack)
        offset = FirstTrimBarrier(tempoTrack, offset);

    if (offset == kMusicTimeStamp_EndOfTrack || offset <= kTrimHeadroom)
        return;
    offset -= kTrimHeadroom;

    Float64 silence = 0;
    if (MusicSequenceGetSecondsForBeats(sequence, offset, &silence) != noErr
        || silence < kMinimumTrimmedSilence)
        return;

    for (UInt32 i = 0; i < numberOfTracks; i++) {
        MusicTrack track = NULL;
        if (MusicSequenceGetIndTrack(sequence, i, &track) == noErr)
            ShiftTrackBack(track, offset);
    }
    if (tempoTrack)
        ShiftTrackBack(tempoTrack, offset);
}

/// Length of the longest track in the sequence, in beats, or 0 if unknown.
static MusicTimeStamp SequenceLength(MusicSequence sequence) {
    UInt32 numberOfTracks = 0;
    if (MusicSequenceGetTrackCount(sequence, &numberOfTracks) != noErr)
        return 0;

    MusicTimeStamp longest = 0;
    for (UInt32 i = 0; i < numberOfTracks; i++) {
        MusicTrack track = NULL;
        MusicTimeStamp trackLen = 0;
        UInt32 trackLenLen = sizeof(trackLen);
        if (MusicSequenceGetIndTrack(sequence, i, &track) != noErr)
            continue;
        if (MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &trackLen, &trackLenLen) != noErr)
            continue;
        if (trackLen > longest)
            longest = trackLen;
    }
    return longest;
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
static void ChaseSynthState(MusicSequence sequence, MusicTimeStamp beats, AudioUnit synth) {
    if (beats <= 0 || !synth)
        return;

    UInt32 numberOfTracks = 0;
    if (MusicSequenceGetTrackCount(sequence, &numberOfTracks) != noErr)
        return;

    UInt8 program[16];
    UInt8 controller[16][128];
    UInt8 bend[16][2];
    BOOL hasProgram[16], hasController[16][128], hasBend[16];
    memset(program, 0, sizeof(program));
    memset(controller, 0, sizeof(controller));
    memset(bend, 0, sizeof(bend));
    memset(hasProgram, 0, sizeof(hasProgram));
    memset(hasController, 0, sizeof(hasController));
    memset(hasBend, 0, sizeof(hasBend));

    for (UInt32 i = 0; i < numberOfTracks; i++) {
        MusicTrack track = NULL;
        if (MusicSequenceGetIndTrack(sequence, i, &track) != noErr)
            continue;

        MusicEventIterator iterator = NULL;
        if (NewMusicEventIterator(track, &iterator) != noErr)
            continue;

        Boolean hasEvent = false;
        MusicEventIteratorHasCurrentEvent(iterator, &hasEvent);
        while (hasEvent) {
            MusicTimeStamp stamp = 0;
            MusicEventType type = 0;
            const void *data = NULL;
            UInt32 size = 0;
            if (MusicEventIteratorGetEventInfo(iterator, &stamp, &type, &data, &size) != noErr)
                break;
            /* Events arrive in time order, so the rest of the track is in the future. */
            if (stamp >= beats)
                break;

            if (type == kMusicEventType_MIDIChannelMessage) {
                const MIDIChannelMessage *message = data;
                UInt8 channel = message->status & 0x0f;
                switch (message->status & 0xf0) {
                    case 0xb0:
                        if (message->data1 < 128) {
                            controller[channel][message->data1] = message->data2;
                            hasController[channel][message->data1] = YES;
                        }
                        break;
                    case 0xc0:
                        program[channel] = message->data1;
                        hasProgram[channel] = YES;
                        break;
                    case 0xe0:
                        bend[channel][0] = message->data1;
                        bend[channel][1] = message->data2;
                        hasBend[channel] = YES;
                        break;
                    default:
                        break;
                }
            } else if (type == kMusicEventType_MIDIRawData) {
                const MIDIRawData *raw = data;
                if (raw->length > 0 && raw->data[0] == 0xf0)
                    MusicDeviceSysEx(synth, raw->data, raw->length);
            }

            if (MusicEventIteratorNextEvent(iterator) != noErr)
                break;
            MusicEventIteratorHasCurrentEvent(iterator, &hasEvent);
        }
        DisposeMusicEventIterator(iterator);
    }

    for (UInt8 channel = 0; channel < 16; channel++) {
        for (UInt32 number = 0; number < 128; number++) {
            if (hasController[channel][number])
                MusicDeviceMIDIEvent(synth, 0xb0 | channel, number, controller[channel][number], 0);
        }
        if (hasProgram[channel])
            MusicDeviceMIDIEvent(synth, 0xc0 | channel, program[channel], 0, 0);
        if (hasBend[channel])
            MusicDeviceMIDIEvent(synth, 0xe0 | channel, bend[channel][0], bend[channel][1], 0);
    }
}

- (void)loadData:(NSData *)data {
    NewMusicSequence(&sequence);
    MusicSequenceFileLoadData (sequence, (__bridge CFDataRef _Nonnull)(data), kMusicSequenceFile_MIDIType, kMusicSequenceLoadSMF_ChannelsToTracks);
    MusicSequenceSetAUGraph(sequence, graph);

    TrimLeadingSilence(sequence);

    NewMusicPlayer(&player);
    MusicPlayerSetSequence(player, sequence);
    MusicPlayerPreroll(player);

    _duration = 0;
    MusicTimeStamp longest = SequenceLength(sequence);
    if (longest > 0) {
        Float64 seconds = 0;
        if (MusicSequenceGetSecondsForBeats(sequence, longest, &seconds) == noErr)
            _duration = (NSTimeInterval)seconds;
    }
}

- (double)position {
    if (hasPausedTime)
        return pausedTime;
    MusicTimeStamp now = 0;
    if (MusicPlayerGetTime(player, &now) != noErr)
        return 0;
    return now;
}

- (void)seekToPosition:(double)beats {
    if (beats <= 0)
        return;

    MusicTimeStamp target = beats;
    /* A looping player keeps counting past the end of the tracks, so a position
       taken during the third pass has to be folded back into the sequence. */
    MusicTimeStamp length = SequenceLength(sequence);
    if (length > 0 && target >= length)
        target = fmod(target, length);

    ChaseSynthState(sequence, target, sampleUnit);

    /* -play applies this the same way it resumes from a pause. */
    pausedTime = target;
    hasPausedTime = YES;
}

- (void)loop:(NSInteger)repeats {
    if (repeats == -1)
        repeats = 0; //Loop forever

    UInt32 numberOfTracks;

    MusicSequenceGetTrackCount(sequence, &numberOfTracks);
    for (UInt32 i = 0; i < numberOfTracks; i++) {
        MusicTrack track = NULL;
        MusicTimeStamp trackLen = 0;

        UInt32 trackLenLen = sizeof(trackLen);

        MusicSequenceGetIndTrack(sequence, i, &track);

        MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &trackLen, &trackLenLen);
        MusicTrackLoopInfo loopInfo = { trackLen, (SInt32)repeats };
        MusicTrackSetProperty(track, kSequenceTrackProperty_LoopInfo, &loopInfo, sizeof(loopInfo));
    }
}

- (void)play {
    [self startGraph];
    if (hasPausedTime) {
        MusicPlayerSetTime(player, pausedTime);
        hasPausedTime = NO;
    }
    MusicPlayerStart(player);
}

- (void)pause {
    /* Pausing something that is not playing must not throw away the point it is
       waiting to resume from: a channel restored from an autosave holds one before
       it has ever been started. */
    Boolean isPlaying = false;
    if (MusicPlayerIsPlaying(player, &isPlaying) == noErr && !isPlaying) {
        [self stopGraph];
        return;
    }

    pausedTime = 0;
    hasPausedTime = (MusicPlayerGetTime(player, &pausedTime) == noErr);
    MusicPlayerStop(player);
    [self stopGraph];
}

- (void)stop {
    MusicPlayerStop(player);
    DisposeMusicPlayer(player);
    DisposeMusicSequence(sequence);
    [self stopGraph];
    DisposeAUGraph(graph);
}

static void userCallback (void *inClientData, MusicSequence inSequence, MusicTrack inTrack, MusicTimeStamp inEventTime, const MusicEventUserData *inEventData, MusicTimeStamp inStartSliceBeat, MusicTimeStamp inEndSliceBeat);

- (void)addCallback:(void (^)(void))block {
    MusicTrack track = NULL;
    MusicTimeStamp trackLen = 0;
    UInt32 trackLenLen = sizeof(trackLen);
    //Get main track
    MusicSequenceGetIndTrack(sequence, 0, &track);
    //Get length of track
    MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &trackLen, &trackLenLen);
    //Create UserData for User Event with any data
    static const MusicEventUserData userData = {1, {0x09}};
    //Put new user event at the end of the track
    MusicTrackNewUserEvent(track, trackLen, &userData);
    //Set a callback for when User Events occur
    MusicSequenceSetUserCallback(sequence, userCallback, (void * _Nullable)CFBridgingRetain(block));
}

static void userCallback (void *inClientData, MusicSequence inSequence, MusicTrack inTrack, MusicTimeStamp inEventTime, const MusicEventUserData *inEventData, MusicTimeStamp inStartSliceBeat, MusicTimeStamp inEndSliceBeat) {
    typedef void (^MyBlockType)(void);
    MyBlockType block = (__bridge MyBlockType)inClientData;
    block();
}

@end
