//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import "SFBHALAudioObject.hpp"
#import "SFBCAStreamBasicDescription.hpp"

namespace SFB {

class HALAudioStream : public HALAudioObject
{

public:

	/// Creates an unknown @c HALAudioStream
	constexpr HALAudioStream() noexcept = default;

	/// Copy constructor
	constexpr HALAudioStream(const HALAudioStream& rhs) noexcept = default;

	/// Assignment operator
	HALAudioStream& operator=(const HALAudioStream& rhs) noexcept = default;

	/// Destructor
	virtual ~HALAudioStream() = default;

	/// Move constructor
	HALAudioStream(HALAudioStream&& rhs) = default;

	/// Move assignment operator
	HALAudioStream& operator=(HALAudioStream&& rhs) = default;


	/// Creates a @c HALAudioStream with the specified objectID
	inline constexpr HALAudioStream(AudioObjectID objectID) noexcept
	: HALAudioObject(objectID)
	{}

	inline bool IsActive() const
	{
		return ArithmeticProperty<UInt32>(CAPropertyAddress(kAudioStreamPropertyIsActive)) != 0;
	}

	inline UInt32 Direction() const
	{
		return ArithmeticProperty<UInt32>(CAPropertyAddress(kAudioStreamPropertyDirection));
	}

	inline UInt32 TerminalType() const
	{
		return ArithmeticProperty<UInt32>(CAPropertyAddress(kAudioStreamPropertyTerminalType));
	}

	inline UInt32 StartingChannel() const
	{
		return ArithmeticProperty<UInt32>(CAPropertyAddress(kAudioStreamPropertyStartingChannel));
	}

	inline UInt32 Latency() const
	{
		return ArithmeticProperty<UInt32>(CAPropertyAddress(kAudioStreamPropertyLatency));
	}

	inline CAStreamBasicDescription VirtualFormat() const
	{
		return StructProperty<AudioStreamBasicDescription>(CAPropertyAddress(kAudioStreamPropertyVirtualFormat));
	}

	inline std::vector<AudioStreamRangedDescription> AvailableVirtualFormats() const
	{
		return ArrayProperty<AudioStreamRangedDescription>(CAPropertyAddress(kAudioStreamPropertyAvailableVirtualFormats));
	}

	inline CAStreamBasicDescription PhysicalFormat() const
	{
		return StructProperty<AudioStreamBasicDescription>(CAPropertyAddress(kAudioStreamPropertyPhysicalFormat));
	}

	inline std::vector<AudioStreamRangedDescription> AvailablePhysicalFormats() const
	{
		return ArrayProperty<AudioStreamRangedDescription>(CAPropertyAddress(kAudioStreamPropertyAvailablePhysicalFormats));
	}

};

} // namespace SFB
