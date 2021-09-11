/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "MIDIDecoder.h"

#include <timidity/timidity_internal.h>

#define TIMIDITY_SAMPLE_RATE    44100
#define TIMIDITY_CHANNELS		2
#define TIMIDITY_BIT_DEPTH		16

namespace {

	void RegisterMIDIDecoder() __attribute__ ((constructor));
	void RegisterMIDIDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::MIDIDecoder>();
	}

#pragma mark Callbacks


long tell_callback(void *f);

    size_t read_callback(void *f, void *ptr, size_t size, size_t nmemb)
	{
        assert(nullptr != f);

        auto decoder = static_cast<SFB::Audio::MIDIDecoder *>(f);
        return static_cast<size_t>(decoder->GetInputSource().Read(ptr, (SInt64)(size * nmemb)) / (SInt64)size);
	}

    long tell_callback(void *f)
	{
		assert(nullptr != f);

        auto decoder = static_cast<SFB::Audio::MIDIDecoder *>(f);
        return (decoder->GetInputSource().GetOffset());
	}

	long seek_callback(void *f, long offset, int whence)
	{
		assert(nullptr != f);

        auto decoder = static_cast<SFB::Audio::MIDIDecoder *>(f);

        SInt64 startpos;
        switch (whence) {
            case SEEK_SET:
                startpos = 0;
                break;
            case SEEK_CUR:
                startpos = decoder->GetInputSource().GetOffset();
                break;
            case SEEK_END:
                startpos = decoder->GetInputSource().GetLength() - 1;
                break;
            default:
                return 1; // Error!
        }

        return (decoder->GetInputSource().SeekToOffset(startpos + offset) ? 0 : 1);
	}

	void close_callback(void */*f*/)
	{}

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MIDIDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("mid") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 1, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MIDIDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/midi"), CFSTR("audio/x-midi") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 2, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MIDIDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mid"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::MIDIDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/midi"), kCFCompareCaseInsensitive))
		return true;

    if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-midi"), kCFCompareCaseInsensitive))
        return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::MIDIDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new MIDIDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::MIDIDecoder::MIDIDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), stream(nullptr, nullptr), song(nullptr, nullptr), mTotalFrames(0), mCurrentFrame(0)
{}

#pragma mark Functionality

bool SFB::Audio::MIDIDecoder::_Open(CFErrorRef *error)
{

//    mid_init_no_config();
    mid_init("/usr/local/Cellar/timidity/2.15.0_1/share/freepats/crude.cfg");

    stream = unique_MidIStream_ptr(mid_istream_open_callbacks((MidIStreamReadFunc)read_callback, (MidIStreamSeekFunc)seek_callback, (MidIStreamTellFunc)tell_callback, (MidIStreamCloseFunc)close_callback, this), mid_istream_close);
	if(!stream) {
		return false;
	}

    options.rate = TIMIDITY_SAMPLE_RATE;
    options.format = (TIMIDITY_BIT_DEPTH == 16)? MID_AUDIO_S16LSB : MID_AUDIO_U8;
    options.channels = TIMIDITY_CHANNELS;
    options.buffer_size = TIMIDITY_SAMPLE_RATE;

    song = unique_MidSong_ptr(mid_song_load(stream.get(), &options), mid_song_free);

	if(!song) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MIDI file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a MIDI file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}


    mid_song_start(song.get());

	// Generate interleaved 2 channel 44.1 16-bit output
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;

	mFormat.mSampleRate			= TIMIDITY_SAMPLE_RATE;
	mFormat.mChannelsPerFrame	= TIMIDITY_CHANNELS;
	mFormat.mBitsPerChannel		= TIMIDITY_BIT_DEPTH;

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatMIDIStream;

	mSourceFormat.mSampleRate			= TIMIDITY_SAMPLE_RATE;
	mSourceFormat.mChannelsPerFrame		= TIMIDITY_CHANNELS;

	// Setup the channel layout
	mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);

    mTotalFrames = song.get()->samples;

	return true;
}

bool SFB::Audio::MIDIDecoder::_Close(CFErrorRef */*error*/)
{
    mid_exit();

	return true;
}

SFB::CFString SFB::Audio::MIDIDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("MIDI, %u channels, %u Hz"),
					mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::MIDIDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(bufferList->mBuffers[0].mNumberChannels != mFormat.mChannelsPerFrame) {
		fprintf(stderr, "_ReadAudio() called with invalid parameters");
		return 0;
	}

    // EOF reached
	if(song.get()->current_sample > mTotalFrames) {
		bufferList->mBuffers[0].mDataByteSize = 0;
		return 0;
	}

    size_t buffer_size = frameCount * mFormat.mBytesPerFrame;


    size_t bytesRead = mid_song_read_wave (song.get(), (sint8 *)bufferList->mBuffers[0].mData, buffer_size);

    size_t framesRendered = bytesRead / mFormat.mBytesPerFrame;

	mCurrentFrame += framesRendered;

	bufferList->mBuffers[0].mDataByteSize = (UInt32)bytesRead;
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return (UInt32)framesRendered;
}

SInt64 SFB::Audio::MIDIDecoder::_SeekToFrame(SInt64 frame)
{
	// Timidity cannot seek backwards, so the decoder must be reset
	if(frame < mCurrentFrame) {
		if(!_Close(nullptr) || !mInputSource->SeekToOffset(0) || !_Open(nullptr)) {
			fprintf(stderr, "Error resetting Timidity decoder");
			return -1;
		}

		mCurrentFrame = 0;
	}

	long framesToSkip = frame - mCurrentFrame;


    uint32 framesInMilliSeconds = (uint32)(song.get()->current_sample / TIMIDITY_SAMPLE_RATE) * 1000;
    framesInMilliSeconds       += (song.get()->current_sample % TIMIDITY_SAMPLE_RATE) * 1000 / TIMIDITY_SAMPLE_RATE;

    mid_song_seek(song.get(), (uint32)framesInMilliSeconds);
	mCurrentFrame += framesToSkip;

	return mCurrentFrame;
}
