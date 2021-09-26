//
//  MIDIPlayer.m
//  MIDIPlayer
//
//  Created by 谢小进 on 15/4/27.
//  Copyright (c) 2015年 Seedfield. All rights reserved.
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
}

@end

@implementation MIDIPlayer

- (instancetype)initWithData:(NSData *)data {
    if (self = [super init]) {
		[self createGraph];
		[self startGraph];
        [self loadData:data];
    }
    return self;
}

- (void)createGraph {
    NewAUGraph(&graph);
	
	AudioComponentDescription sampleDesc = {};
	sampleDesc.componentType			= kAudioUnitType_MusicDevice;
    sampleDesc.componentSubType         = kAudioUnitSubType_DLSSynth;
	sampleDesc.componentManufacturer	= kAudioUnitManufacturer_Apple;
	sampleDesc.componentFlags			= 0;
	sampleDesc.componentFlagsMask		= 0;
    AUGraphAddNode(graph, &sampleDesc, &sampleNode);
	
    AudioComponentDescription ioUnitDesc;
    ioUnitDesc.componentType			= kAudioUnitType_Output;
    ioUnitDesc.componentSubType			= kAudioUnitSubType_DefaultOutput;
    ioUnitDesc.componentManufacturer	= kAudioUnitManufacturer_Apple;
    ioUnitDesc.componentFlags			= 0;
    ioUnitDesc.componentFlagsMask		= 0;
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

- (void)loadData:(NSData *)data {
    NewMusicSequence(&sequence);
    MusicSequenceFileLoadData (sequence, (__bridge CFDataRef _Nonnull)(data), kMusicSequenceFile_MIDIType, kMusicSequenceLoadSMF_ChannelsToTracks);
    MusicSequenceSetAUGraph(sequence, graph);
	
	NewMusicPlayer(&player);
	MusicPlayerSetSequence(player, sequence);
    MusicPlayerPreroll(player);
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
    MusicPlayerStart(player);
}

- (void)pause {
    [self stopGraph];
    MusicPlayerStop(player);
}

- (void)stop {
	MusicPlayerStop(player);
	DisposeMusicPlayer(player);
    DisposeMusicSequence(sequence);
	[self stopGraph];
    DisposeAUGraph(graph);
}

- (void)addCallback:(void (^)(void))block {
    MusicTrack track = NULL;
    MusicTimeStamp trackLen = 0;
    UInt32 trackLenLen = sizeof(trackLen);
    //Get main track
    MusicSequenceGetIndTrack(sequence, 0, &track);
    //Get length of track
    MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &trackLen, &trackLenLen);
    //Create UserData for User Event with any data
    static MusicEventUserData userData = {1, {0x09}};
    //Put new user event at the end of the track
    MusicTrackNewUserEvent(track, trackLen, &userData);
    //Set a callback for when User Events occur
    MusicSequenceSetUserCallback(sequence, userCallback, (void * _Nullable)CFBridgingRetain(block));
}

    void userCallback (void *inClientData, MusicSequence inSequence, MusicTrack inTrack, MusicTimeStamp inEventTime, const MusicEventUserData *inEventData, MusicTimeStamp inStartSliceBeat, MusicTimeStamp inEndSliceBeat)
    {
        typedef void (^MyBlockType)(void);
        MyBlockType block = (__bridge MyBlockType)inClientData;
        block();
    }

@end
