//
// Copyright (c) 2014 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#import "SFBCAStreamBasicDescription.hpp"

// Most of this is stolen from Apple's CAStreamBasicDescription::Print()
SFB::CFString SFB::CAStreamBasicDescription::Description(const char * const prefix) const noexcept
{
	CFMutableString result{ CFStringCreateMutable(kCFAllocatorDefault, 0) };

	if(prefix)
		CFStringAppendCString(result, prefix, kCFStringEncodingUTF8);

	unsigned char formatID [5];
	auto formatIDBE = OSSwapHostToBigInt32(mFormatID);
	std::memcpy(formatID, &formatIDBE, 4);
	formatID[4] = '\0';

	// General description
	CFStringAppendFormat(result, NULL, CFSTR("%u ch, %.2f Hz, '%.4s' (0x%0.8x) "), mChannelsPerFrame, mSampleRate, formatID, mFormatFlags);

	if(kAudioFormatLinearPCM == mFormatID) {
		// Bit depth
		UInt32 fractionalBits = (kLinearPCMFormatFlagsSampleFractionMask & mFormatFlags) >> kLinearPCMFormatFlagsSampleFractionShift;
		if(fractionalBits > 0)
			CFStringAppendFormat(result, NULL, CFSTR("%d.%d-bit"), mBitsPerChannel - fractionalBits, fractionalBits);
		else
			CFStringAppendFormat(result, NULL, CFSTR("%d-bit"), mBitsPerChannel);

		// Endianness
		bool isInterleaved = !(kAudioFormatFlagIsNonInterleaved & mFormatFlags);
		UInt32 interleavedChannelCount = (isInterleaved ? mChannelsPerFrame : 1);
		UInt32 sampleSize = (mBytesPerFrame > 0 && interleavedChannelCount > 0 ? mBytesPerFrame / interleavedChannelCount : 0);
		if(sampleSize > 1)
			CFStringAppend(result, (kLinearPCMFormatFlagIsBigEndian & mFormatFlags) ? CFSTR(" big-endian") : CFSTR(" little-endian"));

		// Sign
		bool isInteger = !(kLinearPCMFormatFlagIsFloat & mFormatFlags);
		if(isInteger)
			CFStringAppend(result, (kLinearPCMFormatFlagIsSignedInteger & mFormatFlags) ? CFSTR(" signed") : CFSTR(" unsigned"));

		// Integer or floating
		CFStringAppend(result, isInteger ? CFSTR(" integer") : CFSTR(" float"));

		// Packedness
		if(sampleSize > 0 && ((sampleSize << 3) != mBitsPerChannel))
			CFStringAppendFormat(result, NULL, (kLinearPCMFormatFlagIsPacked & mFormatFlags) ? CFSTR(", packed in %d bytes") : CFSTR(", unpacked in %d bytes"), sampleSize);

		// Alignment
		if((sampleSize > 0 && ((sampleSize << 3) != mBitsPerChannel)) || ((mBitsPerChannel & 7) != 0))
			CFStringAppend(result, (kLinearPCMFormatFlagIsAlignedHigh & mFormatFlags) ? CFSTR(" high-aligned") : CFSTR(" low-aligned"));

		if(!isInterleaved)
			CFStringAppend(result, CFSTR(", deinterleaved"));
	}
	else if(kAudioFormatAppleLossless == mFormatID) {
		UInt32 sourceBitDepth = 0;
		switch(mFormatFlags) {
			case kAppleLosslessFormatFlag_16BitSourceData:		sourceBitDepth = 16;	break;
			case kAppleLosslessFormatFlag_20BitSourceData:		sourceBitDepth = 20;	break;
			case kAppleLosslessFormatFlag_24BitSourceData:		sourceBitDepth = 24;	break;
			case kAppleLosslessFormatFlag_32BitSourceData:		sourceBitDepth = 32;	break;
		}

		if(sourceBitDepth != 0)
			CFStringAppendFormat(result, NULL, CFSTR("from %d-bit source, "), sourceBitDepth);
		else
			CFStringAppend(result, CFSTR("from UNKNOWN source bit depth, "));

		CFStringAppendFormat(result, NULL, CFSTR(" %d frames/packet"), mFramesPerPacket);
	}
	else
		CFStringAppendFormat(result, NULL, CFSTR("%u bits/channel, %u bytes/packet, %u frames/packet, %u bytes/frame"), mBitsPerChannel, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame);

	return CFString(static_cast<CFStringRef>(result.Relinquish()));
}
