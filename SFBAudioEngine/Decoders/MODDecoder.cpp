/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#ifdef DEBUG
#define NSLog(FORMAT, ...)                                                     \
{ fprintf(stderr, FORMAT, ##__VA_ARGS__); fprintf(stderr, "\n"); }
#else
#define NSLog(...)
#endif

#include "CFErrorUtilities.h"
#include "CFWrapper.h"
#include "MODDecoder.h"

#define DUMB_SAMPLE_RATE    65536
#define DUMB_CHANNELS        2
#define DUMB_BIT_DEPTH        16
#define DUMB_BUF_FRAMES        512

namespace {

	void RegisterMODDecoder() __attribute__ ((constructor));
	void RegisterMODDecoder()
	{
		SFB::Audio::Decoder::RegisterSubclass<SFB::Audio::MODDecoder>();
	}

#pragma mark Callbacks

	static int skip_callback(void *f, dumb_off_t n)
	{
		assert(nullptr != f);

		auto decoder = static_cast<SFB::Audio::MODDecoder *>(f);
		return (decoder->GetInputSource().SeekToOffset(decoder->GetInputSource().GetOffset() + n) ? 0 : 1);
	}

	static int getc_callback(void *f)
	{
		assert(nullptr != f);

		auto decoder = static_cast<SFB::Audio::MODDecoder *>(f);

		uint8_t value;
		return (1 == decoder->GetInputSource().Read(&value, 1) ? value : -1);
	}

	static long getnc_callback(char *ptr, size_t n, void *f)
	{
		assert(nullptr != f);

		auto decoder = static_cast<SFB::Audio::MODDecoder *>(f);
		return static_cast<long>(decoder->GetInputSource().Read(ptr, (SInt64)n));
	}

	static void close_callback(void */*f*/)
	{}

    static int seek_callback(void *f, dumb_off_t offset)
    {
        assert(nullptr != f);

        auto decoder = static_cast<SFB::Audio::MODDecoder *>(f);

        if(!decoder->GetInputSource().SeekToOffset(offset))
            return -1;
        return 0;
    }

    static dumb_off_t get_size_callback(void *f)
    {
        assert(nullptr != f);

        auto decoder = static_cast<SFB::Audio::MODDecoder *>(f);

        dumb_off_t length = decoder->GetInputSource().GetLength();
        return length;
    }

}

#pragma mark Static Methods

CFArrayRef SFB::Audio::MODDecoder::CreateSupportedFileExtensions()
{
	CFStringRef supportedExtensions [] = { CFSTR("it"), CFSTR("xm"), CFSTR("s3m"), CFSTR("mod") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedExtensions, 4, &kCFTypeArrayCallBacks);
}

CFArrayRef SFB::Audio::MODDecoder::CreateSupportedMIMETypes()
{
	CFStringRef supportedMIMETypes [] = { CFSTR("audio/it"), CFSTR("audio/xm"), CFSTR("audio/s3m"), CFSTR("audio/mod"), CFSTR("audio/x-mod") };
	return CFArrayCreate(kCFAllocatorDefault, (const void **)supportedMIMETypes, 5, &kCFTypeArrayCallBacks);
}

bool SFB::Audio::MODDecoder::HandlesFilesWithExtension(CFStringRef extension)
{
	if(nullptr == extension)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("it"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("xm"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("s3m"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(extension, CFSTR("mod"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

bool SFB::Audio::MODDecoder::HandlesMIMEType(CFStringRef mimeType)
{
	if(nullptr == mimeType)
		return false;

	if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/it"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/xm"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/s3m"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/mod"), kCFCompareCaseInsensitive))
		return true;
	else if(kCFCompareEqualTo == CFStringCompare(mimeType, CFSTR("audio/x-mod"), kCFCompareCaseInsensitive))
		return true;

	return false;
}

SFB::Audio::Decoder::unique_ptr SFB::Audio::MODDecoder::CreateDecoder(InputSource::unique_ptr inputSource)
{
	return unique_ptr(new MODDecoder(std::move(inputSource)));
}

#pragma mark Creation and Destruction

SFB::Audio::MODDecoder::MODDecoder(InputSource::unique_ptr inputSource)
	: Decoder(std::move(inputSource)), df(nullptr, nullptr), duh(nullptr, nullptr), dsr(nullptr, nullptr), _samples(nullptr), mTotalFrames(0), mCurrentFrame(0)
{}

#pragma mark Functionality

bool SFB::Audio::MODDecoder::_Open(CFErrorRef *error)
{
	dfs.open = nullptr;
	dfs.skip = skip_callback;
	dfs.getc = getc_callback;
	dfs.getnc = getnc_callback;
    dfs.close = close_callback;
    dfs.seek = seek_callback;
    dfs.get_size = get_size_callback;

	df = unique_DUMBFILE_ptr(dumbfile_open_ex(this, &dfs), dumbfile_close);
	if(!df) {
		return false;
	}

    duh = unique_DUH_ptr(dumb_read_any(df.get(), 0, 0), unload_duh);

	if(!duh) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a MOD file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// NB: This must change if the sample rate changes because it is based on 65536 Hz
	mTotalFrames = duh_get_length(duh.get());

	dsr = unique_DUH_SIGRENDERER_ptr(duh_start_sigrenderer(duh.get(), 0, DUMB_CHANNELS, 0), duh_end_sigrenderer);
	if(!dsr) {
		if(error) {
			SFB::CFString description(CFCopyLocalizedString(CFSTR("The file “%@” is not a valid MOD file."), ""));
			SFB::CFString failureReason(CFCopyLocalizedString(CFSTR("Not a MOD file"), ""));
			SFB::CFString recoverySuggestion(CFCopyLocalizedString(CFSTR("The file's extension may not match the file's type."), ""));

			*error = CreateErrorForURL(Decoder::ErrorDomain, Decoder::InputOutputError, description, mInputSource->GetURL(), failureReason, recoverySuggestion);
		}

		return false;
	}

	// Generate interleaved 2 channel 44.1 16-bit output
	mFormat.mFormatID			= kAudioFormatLinearPCM;
	mFormat.mFormatFlags		= kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;

	mFormat.mSampleRate			= DUMB_SAMPLE_RATE;
	mFormat.mChannelsPerFrame	= DUMB_CHANNELS;
	mFormat.mBitsPerChannel		= DUMB_BIT_DEPTH;

	mFormat.mBytesPerPacket		= (mFormat.mBitsPerChannel / 8) * mFormat.mChannelsPerFrame;
	mFormat.mFramesPerPacket	= 1;
	mFormat.mBytesPerFrame		= mFormat.mBytesPerPacket * mFormat.mFramesPerPacket;

	mFormat.mReserved			= 0;

	// Set up the source format
	mSourceFormat.mFormatID				= kAudioFormatMOD;

	mSourceFormat.mSampleRate			= DUMB_SAMPLE_RATE;
	mSourceFormat.mChannelsPerFrame		= DUMB_CHANNELS;

	// Setup the channel layout
	mChannelLayout = ChannelLayout::ChannelLayoutWithTag(kAudioChannelLayoutTag_Stereo);

    _samples = allocate_sample_buffer(DUMB_CHANNELS, DUMB_BUF_FRAMES);
    if(!_samples) {
        return false;
    }

	return true;
}

bool SFB::Audio::MODDecoder::_Close(CFErrorRef */*error*/)
{
	dsr.reset();
	duh.reset();
	df.reset();

    if(_samples) {
        destroy_sample_buffer(_samples);
        _samples = nullptr;
    }

	return true;
}

SFB::CFString SFB::Audio::MODDecoder::_GetSourceFormatDescription() const
{
	return CFString(nullptr,
					CFSTR("MOD, %u channels, %u Hz"),
					mSourceFormat.mChannelsPerFrame,
					(unsigned int)mSourceFormat.mSampleRate);
}

UInt32 SFB::Audio::MODDecoder::_ReadAudio(AudioBufferList *bufferList, UInt32 frameCount)
{
	if(bufferList->mBuffers[0].mNumberChannels != mFormat.mChannelsPerFrame) {
		fprintf(stderr, "_ReadAudio() called with invalid parameters");
		return 0;
	}

	// EOF reached
	if(duh_sigrenderer_get_position(dsr.get()) > mTotalFrames) {
		bufferList->mBuffers[0].mDataByteSize = 0;
		return 0;
	}

    UInt32 maxFrames = bufferList->mBuffers[0].mDataByteSize / mFormat.mBytesPerFrame;

    if(frameCount > maxFrames) {
        NSLog("frameCount (%d) greater than maxFrames (%d). Reducing.", frameCount, maxFrames);
        frameCount = maxFrames;
    }

    if(frameCount == 0)
        return 0;

    // Reset output buffer data size
    bufferList->mBuffers[0].mDataByteSize = 0;

    UInt32 framesProcessed = 0;

    for(;;) {
        UInt32 framesRemaining = frameCount - framesProcessed;
        UInt32 framesToCopy = MIN(framesRemaining, DUMB_BUF_FRAMES);

        long samplesSize = framesToCopy;

        int16_t *buffer = (int16_t *)bufferList->mBuffers[0].mData;
        long framesCopied = duh_render_int(dsr.get(), &_samples, &samplesSize, DUMB_BIT_DEPTH, 0, 1, 65536.0f / DUMB_SAMPLE_RATE, framesToCopy, buffer + (framesProcessed * DUMB_CHANNELS));
        if(framesCopied != framesToCopy)
            NSLog("duh_render_int() returned short frame count: requested %d, got %ld", framesToCopy, framesCopied);

        framesProcessed += framesCopied;

        // All requested frames were read or EOS reached
        if(framesProcessed == frameCount || framesCopied == 0 || duh_sigrenderer_get_position(dsr.get()) > mTotalFrames)
            break;
    }

    mCurrentFrame += framesProcessed;

    bufferList->mBuffers[0].mDataByteSize = (UInt32)(framesProcessed * mFormat.mBytesPerFrame);
	bufferList->mBuffers[0].mNumberChannels = mFormat.mChannelsPerFrame;

	return (UInt32)framesProcessed;
}

SInt64 SFB::Audio::MODDecoder::_SeekToFrame(SInt64 frame)
{
	// DUMB cannot seek backwards, so the decoder must be reset
	if(frame < mCurrentFrame) {
		if(!_Close(nullptr) || !mInputSource->SeekToOffset(0) || !_Open(nullptr)) {
			fprintf(stderr, "Error reseting DUMB decoder");
			return -1;
		}

		mCurrentFrame = 0;
	}

	long framesToSkip = frame - mCurrentFrame;
	duh_sigrenderer_generate_samples(dsr.get(), 1, 65536.0f / DUMB_SAMPLE_RATE, framesToSkip, nullptr);
	mCurrentFrame += framesToSkip;

	return mCurrentFrame;
}
