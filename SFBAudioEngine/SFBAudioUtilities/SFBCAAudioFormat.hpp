//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <vector>

#import <AudioToolbox/AudioFormat.h>

#import "SFBCAException.hpp"

CF_ASSUME_NONNULL_BEGIN

namespace SFB {
namespace CAAudioFormat {

/// Retrieves information about the given property.
/// @param inPropertyID An @c AudioFormatPropertyID constant.
/// @param inSpecifierSize The size of the specifier data.
/// @param inSpecifier A specifier is a buffer of data used as an input argument to some of the properties.
/// @return The size in bytes of the current value of the property.
/// @throw @c std::system_error
inline UInt32 GetPropertyInfo(AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize, const void * _Nullable inSpecifier)
{
	UInt32 size;
	auto result = AudioFormatGetPropertyInfo(inPropertyID, inSpecifierSize, inSpecifier, &size);
	ThrowIfCAAudioFormatError(result, "AudioFormatGetPropertyInfo");
	return size;
}

/// Retrieves the indicated property data
/// @param inPropertyID An @c AudioFormatPropertyID constant.
/// @param inSpecifierSize The size of the specifier data.
/// @param inSpecifier A specifier is a buffer of data used as an input argument to some of the properties.
/// @param ioPropertyDataSize On input the size of the @c outPropertyData buffer. On output the number of bytes written to the buffer.
/// @param outPropertyData The buffer in which to write the property data.
/// @throw @c std::system_error
inline void GetProperty(AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize, const void * _Nullable inSpecifier, UInt32& ioPropertyDataSize, void *outPropertyData)
{
	auto result = AudioFormatGetProperty(inPropertyID, inSpecifierSize, inSpecifier, &ioPropertyDataSize, outPropertyData);
	ThrowIfCAAudioFormatError(result, "AudioFormatGetProperty");
}

/// Returns an array of format IDs that are valid output formats for a converter.
/// @throw @c std::system_error
inline std::vector<AudioFormatID> EncodeFormatIDs()
{
	auto size = GetPropertyInfo(kAudioFormatProperty_EncodeFormatIDs, 0, nullptr);
	auto count = size / sizeof(AudioFormatID);
	auto formatIDs = std::vector<AudioFormatID>(count);
	GetProperty(kAudioFormatProperty_EncodeFormatIDs, 0, nullptr, size, &formatIDs[0]);
	return formatIDs;
}

/// Returns an array of format IDs that are valid input formats for a converter.
/// @throw @c std::system_error
inline std::vector<AudioFormatID> DecodeFormatIDs()
{
	auto size = GetPropertyInfo(kAudioFormatProperty_DecodeFormatIDs, 0, nullptr);
	auto count = size / sizeof(AudioFormatID);
	auto formatIDs = std::vector<AudioFormatID>(count);
	GetProperty(kAudioFormatProperty_DecodeFormatIDs, 0, nullptr, size, &formatIDs[0]);
	return formatIDs;
}

} // namespace CAAudioFormat
} // namespace SFB

CF_ASSUME_NONNULL_END
