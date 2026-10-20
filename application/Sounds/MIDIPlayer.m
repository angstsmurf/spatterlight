//
//  MIDIPlayer.m
//  MIDIPlayer
//
//  Based on code by 谢小进.
//

#import <AudioToolbox/AudioToolbox.h>

#import "MIDIPlayer.h"
#import "MIDISequenceUtils.h"


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

    /* Called at the end-of-sequence user event. Held here, and handed to the
       sequence unretained: the sequence never outlives -stop. */
    void (^endCallback)(void);
}

@end

/* Logs a failed Core Audio call. Returns YES if it succeeded. */
static BOOL Succeeded(OSStatus status, const char *call) {
    if (status != noErr)
        NSLog(@"MIDIPlayer: %s failed (%d)", call, (int)status);
    return status == noErr;
}

@implementation MIDIPlayer

- (instancetype)initWithData:(NSData *)data {
    if (self = [super init]) {
        hasPausedTime = NO;
        pausedTime = 0;
        if (![self createGraph] || ![self startGraph] || ![self loadData:data]) {
            [self stop];
            return nil;
        }
    }
    return self;
}

/* Every player builds its own synth graph. Reusing one would save the
   20-30 ms (97 ms the first time) of setting it up, but the synth would keep
   the previous sound's programs and controllers. */
- (BOOL)createGraph {
    if (!Succeeded(NewAUGraph(&graph), "NewAUGraph")) {
        graph = NULL;
        return NO;
    }

    AudioComponentDescription sampleDesc = {};
    sampleDesc.componentType            = kAudioUnitType_MusicDevice;
    sampleDesc.componentSubType         = kAudioUnitSubType_DLSSynth;
    sampleDesc.componentManufacturer    = kAudioUnitManufacturer_Apple;
    sampleDesc.componentFlags           = 0;
    sampleDesc.componentFlagsMask       = 0;
    if (!Succeeded(AUGraphAddNode(graph, &sampleDesc, &sampleNode), "AUGraphAddNode (synth)"))
        return NO;

    AudioComponentDescription ioUnitDesc;
    ioUnitDesc.componentType            = kAudioUnitType_Output;
    ioUnitDesc.componentSubType         = kAudioUnitSubType_DefaultOutput;
    ioUnitDesc.componentManufacturer    = kAudioUnitManufacturer_Apple;
    ioUnitDesc.componentFlags           = 0;
    ioUnitDesc.componentFlagsMask       = 0;
    if (!Succeeded(AUGraphAddNode(graph, &ioUnitDesc, &ioNode), "AUGraphAddNode (output)"))
        return NO;

    // A description of the mixer unit
    AudioComponentDescription mixerDescription;
    mixerDescription.componentType = kAudioUnitType_Mixer;
    mixerDescription.componentSubType = kAudioUnitSubType_MultiChannelMixer;
    mixerDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    mixerDescription.componentFlags = 0;
    mixerDescription.componentFlagsMask = 0;

    if (!Succeeded(AUGraphAddNode(graph, &mixerDescription, &mixerNode), "AUGraphAddNode (mixer)") ||
        !Succeeded(AUGraphOpen(graph), "AUGraphOpen") ||
        !Succeeded(AUGraphNodeInfo(graph, sampleNode, NULL, &sampleUnit), "AUGraphNodeInfo (synth)") ||
        !Succeeded(AUGraphNodeInfo(graph, ioNode, NULL, &ioUnit), "AUGraphNodeInfo (output)") ||
        !Succeeded(AUGraphNodeInfo(graph, mixerNode, NULL, &mixerUnit), "AUGraphNodeInfo (mixer)") ||
        !Succeeded(AUGraphConnectNodeInput(graph, sampleNode, 0, mixerNode, 0), "AUGraphConnectNodeInput (synth)") ||
        !Succeeded(AUGraphConnectNodeInput(graph, mixerNode, 0, ioNode, 0), "AUGraphConnectNodeInput (mixer)"))
        return NO;

    [self setVolume:1.0];

    return Succeeded(AUGraphInitialize(graph), "AUGraphInitialize");
}

- (void)setVolume:(CGFloat)volume {
    if (!mixerUnit)
        return;
    const AudioUnitScope scopes[] = { kAudioUnitScope_Output, kAudioUnitScope_Input };
    for (size_t i = 0; i < sizeof scopes / sizeof *scopes; i++)
        AudioUnitSetParameter(mixerUnit, kMultiChannelMixerParam_Volume, scopes[i], 0, (AudioUnitParameterValue)volume, 0);
}

- (BOOL)startGraph {
    if (!graph)
        return NO;
    Boolean isRunning = false;
    if (Succeeded(AUGraphIsRunning(graph, &isRunning), "AUGraphIsRunning") && isRunning)
        return YES;
    return Succeeded(AUGraphStart(graph), "AUGraphStart");
}

- (void)stopGraph {
    if (graph) {
        Boolean isRunning = false;
        if (AUGraphIsRunning(graph, &isRunning) == noErr && isRunning)
            Succeeded(AUGraphStop(graph), "AUGraphStop");
    }
}

- (BOOL)loadData:(NSData *)data {
    if (!Succeeded(NewMusicSequence(&sequence), "NewMusicSequence")) {
        sequence = NULL;
        return NO;
    }
    if (!Succeeded(MusicSequenceFileLoadData(sequence, (__bridge CFDataRef)data, kMusicSequenceFile_MIDIType, kMusicSequenceLoadSMF_ChannelsToTracks), "MusicSequenceFileLoadData") ||
        !Succeeded(MusicSequenceSetAUGraph(sequence, graph), "MusicSequenceSetAUGraph"))
        return NO;

    MIDISequenceTrimLeadingSilence(sequence);

    if (!Succeeded(NewMusicPlayer(&player), "NewMusicPlayer")) {
        player = NULL;
        return NO;
    }
    if (!Succeeded(MusicPlayerSetSequence(player, sequence), "MusicPlayerSetSequence"))
        return NO;
    Succeeded(MusicPlayerPreroll(player), "MusicPlayerPreroll");

    _duration = 0;
    MusicTimeStamp longest = MIDISequenceLength(sequence);
    if (longest > 0) {
        Float64 seconds = 0;
        if (MusicSequenceGetSecondsForBeats(sequence, longest, &seconds) == noErr)
            _duration = (NSTimeInterval)seconds;
    }
    return YES;
}

- (double)position {
    if (hasPausedTime)
        return pausedTime;
    MusicTimeStamp now = 0;
    if (!player || MusicPlayerGetTime(player, &now) != noErr)
        return 0;
    return now;
}

- (void)seekToPosition:(double)beats {
    if (beats <= 0)
        return;

    MusicTimeStamp target = beats;
    /* A looping player keeps counting past the end of the tracks, so a position
       taken during the third pass has to be folded back into the sequence. */
    MusicTimeStamp length = MIDISequenceLength(sequence);
    if (length > 0 && target >= length)
        target = fmod(target, length);

    AudioUnit synth = sampleUnit;
    if (synth) {
        MIDISequenceChaseSynthState(sequence, target, ^(const UInt8 *bytes, UInt32 count) {
            if (bytes[0] == 0xf0)
                MusicDeviceSysEx(synth, bytes, count);
            else
                MusicDeviceMIDIEvent(synth, bytes[0], bytes[1], count > 2 ? bytes[2] : 0, 0);
        });
    }

    /* -play applies this the same way it resumes from a pause. */
    pausedTime = target;
    hasPausedTime = YES;
}

- (void)loop:(NSInteger)repeats {
    if (repeats == -1)
        repeats = 0; //Loop forever

    MIDISequenceForEachTrack(sequence, NO, ^(MusicTrack track) {
        MusicTimeStamp trackLen = 0;
        UInt32 trackLenLen = sizeof(trackLen);
        MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &trackLen, &trackLenLen);
        MusicTrackLoopInfo loopInfo = { trackLen, (SInt32)repeats };
        MusicTrackSetProperty(track, kSequenceTrackProperty_LoopInfo, &loopInfo, sizeof(loopInfo));
    });
}

- (BOOL)play {
    if (!player || ![self startGraph])
        return NO;
    if (hasPausedTime) {
        Succeeded(MusicPlayerSetTime(player, pausedTime), "MusicPlayerSetTime");
        hasPausedTime = NO;
    }
    return Succeeded(MusicPlayerStart(player), "MusicPlayerStart");
}

- (void)pause {
    /* Pausing something that is not playing must not throw away the point it is
       waiting to resume from: a channel restored from an autosave holds one before
       it has ever been started. */
    if (!player)
        return;
    Boolean isPlaying = false;
    if (MusicPlayerIsPlaying(player, &isPlaying) == noErr && !isPlaying) {
        [self stopGraph];
        return;
    }

    pausedTime = 0;
    hasPausedTime = (MusicPlayerGetTime(player, &pausedTime) == noErr);
    Succeeded(MusicPlayerStop(player), "MusicPlayerStop");
    [self stopGraph];
}

/* Tear everything down. The player is unusable afterwards, and a second call
   does nothing. */
- (void)stop {
    if (player) {
        MusicPlayerStop(player);
        DisposeMusicPlayer(player);
        player = NULL;
    }
    if (sequence) {
        DisposeMusicSequence(sequence);
        sequence = NULL;
    }
    [self stopGraph];
    if (graph) {
        DisposeAUGraph(graph);
        graph = NULL;
        sampleUnit = NULL;
        ioUnit = NULL;
        mixerUnit = NULL;
    }
    endCallback = nil;
}

- (void)dealloc {
    [self stop];
}

static void userCallback (void *inClientData, MusicSequence inSequence, MusicTrack inTrack, MusicTimeStamp inEventTime, const MusicEventUserData *inEventData, MusicTimeStamp inStartSliceBeat, MusicTimeStamp inEndSliceBeat);

- (void)addCallback:(void (^)(void))block {
    if (!sequence)
        return;
    MusicTrack track = NULL;
    MusicTimeStamp trackLen = 0;
    UInt32 trackLenLen = sizeof(trackLen);
    //Get main track
    if (!Succeeded(MusicSequenceGetIndTrack(sequence, 0, &track), "MusicSequenceGetIndTrack"))
        return;
    //Get length of track
    MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &trackLen, &trackLenLen);
    //Create UserData for User Event with any data
    static const MusicEventUserData userData = {1, {0x09}};
    //Put new user event at the end of the track
    if (!Succeeded(MusicTrackNewUserEvent(track, trackLen, &userData), "MusicTrackNewUserEvent"))
        return;
    //Set a callback for when User Events occur
    endCallback = [block copy];
    Succeeded(MusicSequenceSetUserCallback(sequence, userCallback, (__bridge void *)endCallback), "MusicSequenceSetUserCallback");
}

static void userCallback (void *inClientData, MusicSequence inSequence, MusicTrack inTrack, MusicTimeStamp inEventTime, const MusicEventUserData *inEventData, MusicTimeStamp inStartSliceBeat, MusicTimeStamp inEndSliceBeat) {
    typedef void (^MyBlockType)(void);
    MyBlockType block = (__bridge MyBlockType)inClientData;
    block();
}

@end
