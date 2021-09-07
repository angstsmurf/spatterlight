//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <stdexcept>

#import <os/log.h>

#import <AudioToolbox/AudioUnit.h>

#import "SFBCAExtAudioFile.hpp"

CF_ASSUME_NONNULL_BEGIN

namespace SFB {

/// A class that asynchronously writes the output from an @c AudioUnit to a file
class AudioUnitRecorder
{

public:

	/// Default constructor
	AudioUnitRecorder() noexcept = delete;

	/// Copy constructor
	AudioUnitRecorder(const AudioUnitRecorder& rhs) noexcept = delete;

	/// Assignment operator
	AudioUnitRecorder& operator=(const AudioUnitRecorder& rhs) noexcept = delete;

	/// Destructor
	~AudioUnitRecorder() = default;

	/// Move constructor
	AudioUnitRecorder(AudioUnitRecorder&& rhs) noexcept = delete;

	/// Move assignment operator
	AudioUnitRecorder& operator=(AudioUnitRecorder&& rhs) noexcept = delete;

	/// Creates a new @c AudioUnitRecorder that asynchronously writes the output from an @c AudioUnit to a file
	/// @param au The @c AudioUnit to record
	/// @param outputFileURL The URL of the output audio file
	/// @param fileType The type of the file to create
	/// @param format The format of the audio data to be written to the file
	/// @param busNumber The bus number of @c au to record
	AudioUnitRecorder(AudioUnit au, CFURLRef outputFileURL, AudioFileTypeID fileType, const AudioStreamBasicDescription& format, UInt32 busNumber = 0)
	: mClientFormatIsSet(false), mAudioUnit(au), mBusNumber(busNumber)
	{
		if(!au)
			throw std::invalid_argument("au == nullptr");
		mExtAudioFile.CreateWithURL(outputFileURL, fileType, format, nullptr, kAudioFileFlags_EraseFile);
	}

	/// Starts recording
	void Start() {
		if(mExtAudioFile.IsValid()) {
			if(!mClientFormatIsSet) {
				AudioStreamBasicDescription clientFormat;
				UInt32 size = sizeof(clientFormat);
				OSStatus result = AudioUnitGetProperty(mAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, mBusNumber, &clientFormat, &size);
				ThrowIfCAAudioUnitError(result, "AudioUnitGetProperty");
				mExtAudioFile.SetClientDataFormat(clientFormat);
				mClientFormatIsSet = true;
			}
			mExtAudioFile.WriteAsync(0, nullptr);
			auto result = AudioUnitAddRenderNotify(mAudioUnit, RenderCallback, this);
			ThrowIfCAAudioUnitError(result, "AudioUnitAddRenderNotify");
		}
	}

	/// Stops recording
	void Stop() {
		if(mExtAudioFile.IsValid()) {
			OSStatus result = AudioUnitRemoveRenderNotify(mAudioUnit, RenderCallback, this);
			ThrowIfCAAudioUnitError(result, "AudioUnitRemoveRenderNotify");
		}
	}

private:

	/// Asynchronously writes rendered audio to the file
	static OSStatus RenderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
	{
		AudioUnitRecorder *THIS = static_cast<AudioUnitRecorder *>(inRefCon);
		if(*ioActionFlags & kAudioUnitRenderAction_PostRender) {
			if(THIS->mBusNumber == inBusNumber && !(*ioActionFlags & kAudioUnitRenderAction_PostRenderError)) {
				try {
					THIS->mExtAudioFile.WriteAsync(inNumberFrames, ioData);
				}
				catch(const std::exception& e) {
					os_log_debug(OS_LOG_DEFAULT, "Error writing frames: %{public}s", e.what());
				}
			}
		}
		return noErr;
	}

	/// The underlying @c ExtAudioFile
	CAExtAudioFile mExtAudioFile;
	/// @c true if the @c ExtAudioFile client format is set
	bool mClientFormatIsSet;
	/// The @c AudioUnit to record
	AudioUnit mAudioUnit;
	/// The bus number of @c mAudioUnit to record
	UInt32 mBusNumber;

};

} // namespace SFB

CF_ASSUME_NONNULL_END
