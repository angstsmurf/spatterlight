/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include "AudioOutput.h"

SFB::Audio::Output::Output()
	: mPlayer(nullptr),	mPrepareForFormatBlock(nullptr)
{}
SFB::Audio::Output::~Output()
{
	if(mPrepareForFormatBlock) {
		Block_release(mPrepareForFormatBlock);
		mPrepareForFormatBlock = nullptr;
	}
}

#pragma mark -

bool SFB::Audio::Output::SupportsFormat(const AudioFormat& format) const
{
	return _SupportsFormat(format);
}

void SFB::Audio::Output::SetPrepareForFormatBlock(FormatBlock block)
{
	if(mPrepareForFormatBlock) {
		Block_release(mPrepareForFormatBlock);
		mPrepareForFormatBlock = nullptr;
	}
	if(block)
		mPrepareForFormatBlock = Block_copy(block);
}

bool SFB::Audio::Output::Open()
{
	fprintf(stderr, "Opening output\n");

	if(_IsOpen())
		return true;

	return _Open();
}

bool SFB::Audio::Output::Close()
{
	fprintf(stderr, "Closing output\n");

	if(!_IsOpen())
		return true;

	return _Close();
}


bool SFB::Audio::Output::Start()
{
	fprintf(stderr, "Starting output\n");

	if(!_IsOpen())
		return false;

	if(_IsRunning())
		return true;

	return _Start();
}

bool SFB::Audio::Output::Stop()
{
	fprintf(stderr, "Stopping output\n");

	if(!_IsOpen())
		return false;

	if(!_IsRunning())
		return true;

	return _Stop();
}

bool SFB::Audio::Output::RequestStop()
{
	fprintf(stderr, "Requesting output stop\n");

	if(!_IsOpen())
		return false;

	if(!_IsRunning())
		return true;

	return _RequestStop();
}

bool SFB::Audio::Output::Reset()
{
	fprintf(stderr, "Resetting output\n");

	if(!_IsOpen())
		return false;

	// Some outputs may be able to reset while running

	return _Reset();
}

bool SFB::Audio::Output::SetupForDecoder(const Decoder& decoder)
{
	if(mPrepareForFormatBlock)
		mPrepareForFormatBlock(decoder.GetFormat());
	return _SetupForDecoder(decoder);
}

#pragma mark -

bool SFB::Audio::Output::CreateDeviceUID(CFStringRef& deviceUID) const
{
	return _CreateDeviceUID(deviceUID);
}

bool SFB::Audio::Output::SetDeviceUID(CFStringRef deviceUID)
{
    const char *deviceString = CFStringGetCStringPtr( deviceUID, kCFStringEncodingMacRoman ) ;
	fprintf(stderr, "Setting device UID to %s\n", deviceString);
	return _SetDeviceUID(deviceUID);
}

bool SFB::Audio::Output::GetDeviceSampleRate(Float64& sampleRate) const
{
	return _GetDeviceSampleRate(sampleRate);
}

bool SFB::Audio::Output::SetDeviceSampleRate(Float64 sampleRate)
{
	fprintf(stderr, "Setting device sample rate to %f\n", sampleRate);
	return _SetDeviceSampleRate(sampleRate);
}

size_t SFB::Audio::Output::GetPreferredBufferSize() const
{
	return _GetPreferredBufferSize();
}
