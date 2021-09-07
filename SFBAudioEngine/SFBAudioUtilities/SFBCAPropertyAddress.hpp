//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <CoreAudio/AudioHardware.h>

namespace SFB {

/// A class extending the functionality of a Core Audio @c AudioObjectPropertyAddress
class CAPropertyAddress : public AudioObjectPropertyAddress
{

public:

	/// Creates an empty @c CAPropertyAddress
	CAPropertyAddress() noexcept = default;

	/// Copy constructor
	CAPropertyAddress(const CAPropertyAddress& rhs) noexcept = default;

	/// Assignment operator
	CAPropertyAddress& operator=(const CAPropertyAddress& rhs) noexcept = default;

	/// Destructor
	~CAPropertyAddress() = default;

	/// Move constructor
	CAPropertyAddress(CAPropertyAddress&& rhs) noexcept = default;

	/// Move assignment operator
	CAPropertyAddress& operator=(CAPropertyAddress&& rhs) noexcept = default;


	/// Creates an @c CAPropertyAddress
	/// @param selector The property selector
	/// @param scope The property element
	/// @param element The property scope
	inline CAPropertyAddress(AudioObjectPropertySelector selector, AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal, AudioObjectPropertyElement element = kAudioObjectPropertyElementMaster) noexcept
	: AudioObjectPropertyAddress{selector, scope, element}
	{}

	// Native overloads

	/// Creates a new @c CAPropertyAddress for the specified @c AudioObjectPropertyAddress
	inline CAPropertyAddress(const AudioObjectPropertyAddress& rhs) noexcept
	: AudioObjectPropertyAddress{rhs}
	{}

	/// Assignment operator
	inline CAPropertyAddress& operator=(const AudioObjectPropertyAddress& rhs) noexcept
	{
		AudioObjectPropertyAddress::operator=(rhs);
		return *this;
	}

	// Comparison not defined because of wildcards

};

} // namespace SFB
