//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <vector>

#import <CoreAudio/CoreAudio.h>

#import "SFBCAException.hpp"
#import "SFBCAPropertyAddress.hpp"
#import "SFBCFWrapper.hpp"

namespace SFB {

enum class HALAudioObjectDirectionalScope {
	input,
	output,
};

class HALAudioObject
{

public:

#pragma mark Creation and Destruction

	/// Creates an unknown @c HALAudioObject
	inline constexpr HALAudioObject() noexcept
	: mObjectID(kAudioObjectUnknown)
	{}

	/// Copy constructor
	inline constexpr HALAudioObject(const HALAudioObject& rhs) noexcept = default;

	/// Assignment operator
	inline HALAudioObject& operator=(const HALAudioObject& rhs) noexcept = default;

	/// Destructor
	virtual ~HALAudioObject() = default;

	// Move constructor
	HALAudioObject(HALAudioObject&& rhs) noexcept = default;

	/// Move assignment operator
	HALAudioObject& operator=(HALAudioObject&& rhs) noexcept = default;


	/// Creates a @c HALAudioObject with the specified objectID
	inline constexpr HALAudioObject(AudioObjectID objectID) noexcept
	: mObjectID(objectID)
	{}

#pragma mark Comparison

	/// Returns @c true if this object's @c AudioObjectID is not @c kAudioObjectUnknown
	inline explicit operator bool() const noexcept
	{
		return mObjectID != kAudioObjectUnknown;
	}

	/// Returns @c true if this object's @c AudioObjectID is @c kAudioObjectUnknown
	inline bool operator!() const noexcept
	{
		return !operator bool();
	}

	/// Returns @c true if @c rhs is equal to @c this
	inline bool operator==(AudioObjectID rhs) const noexcept
	{
		return mObjectID == rhs;
	}

	/// Returns @c true if @c rhs is not equal to @c this
	inline bool operator!=(AudioObjectID rhs) const noexcept
	{
		return !operator==(rhs);
	}

	inline operator AudioObjectID() const noexcept
	{
		return mObjectID;
	}

	inline AudioObjectID ObjectID() const noexcept
	{
		return mObjectID;
	}

#pragma mark Property Operations

	inline bool HasProperty(const AudioObjectPropertyAddress& inAddress) const noexcept
	{
		return AudioObjectHasProperty(mObjectID, &inAddress);
	}

	bool IsPropertySettable(const AudioObjectPropertyAddress& inAddress) const
	{
		Boolean settable = false;
		auto result = AudioObjectIsPropertySettable(mObjectID, &inAddress, &settable);
		ThrowIfCAAudioObjectError(result, "AudioObjectIsPropertySettable");
		return settable != 0;
	}

	UInt32 GetPropertyDataSize(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void * _Nullable inQualifierData = nullptr) const
	{
		UInt32 size = 0;
		auto result = AudioObjectGetPropertyDataSize(mObjectID, &inAddress, inQualifierDataSize, inQualifierData, &size);
		ThrowIfCAAudioObjectError(result, "AudioObjectGetPropertyDataSize");
		return size;
	}


	inline void GetPropertyData(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void * _Nullable inQualifierData, UInt32& ioDataSize, void * _Nonnull outData) const
	{
		auto result = AudioObjectGetPropertyData(mObjectID, &inAddress, inQualifierDataSize, inQualifierData, &ioDataSize, outData);
		ThrowIfCAAudioObjectError(result, "AudioObjectGetPropertyData");
	}

	inline void SetPropertyData(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize, const void * _Nullable inQualifierData, UInt32 inDataSize, const void * _Nonnull inData)
	{
		auto result = AudioObjectSetPropertyData(mObjectID, &inAddress, inQualifierDataSize, inQualifierData, inDataSize, inData);
		ThrowIfCAAudioObjectError(result, "AudioObjectSetPropertyData");
	}


	template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, bool>::type = true>
	T ArithmeticProperty(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void * _Nullable inQualifierData = nullptr) const
	{
		T value;
		UInt32 size = sizeof(T);
		GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, size, &value);
		return value;
	}

	template <typename T, typename std::enable_if<std::is_trivial<T>::value, bool>::type = true>
	T StructProperty(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void * _Nullable inQualifierData = nullptr) const
	{
		T value;
		UInt32 size = sizeof(T);
		GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, size, &value);
		return value;
	}

	template <typename T, typename std::enable_if<std::is_trivial<T>::value, bool>::type = true>
	std::vector<T> ArrayProperty(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void * _Nullable inQualifierData = nullptr) const
	{
		auto size = GetPropertyDataSize(inAddress, inQualifierDataSize, inQualifierData);
		auto count = size / sizeof(T);
		auto vec = std::vector<T>(count);
		GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, size, &vec[0]);
		return vec;
	}

	template <typename T, typename std::enable_if</*std::is_class<T>::value &&*/ std::is_pointer<T>::value, bool>::type = true>
	CFWrapper<T> CFTypeProperty(const AudioObjectPropertyAddress& inAddress, UInt32 inQualifierDataSize = 0, const void * _Nullable inQualifierData = nullptr) const
	{
		T value;
		UInt32 size = sizeof(T);
		GetPropertyData(inAddress, inQualifierDataSize, inQualifierData, size, &value);
		return CFWrapper<T>(value);
	}

	inline void AddPropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc _Nonnull inListenerProc, void * _Nullable inClientData)
	{
		auto result = AudioObjectAddPropertyListener(mObjectID, &inAddress, inListenerProc, inClientData);
		ThrowIfCAAudioObjectError(result, "AudioObjectAddPropertyListener");
	}

	inline void RemovePropertyListener(const AudioObjectPropertyAddress& inAddress, AudioObjectPropertyListenerProc _Nonnull inListenerProc, void * _Nullable inClientData)
	{
		auto result = AudioObjectRemovePropertyListener(mObjectID, &inAddress, inListenerProc, inClientData);
		ThrowIfCAAudioObjectError(result, "AudioObjectRemovePropertyListener");
	}


	inline void AddPropertyListenerBlock(const AudioObjectPropertyAddress& inAddress, dispatch_queue_t _Nonnull inDispatchQueue, AudioObjectPropertyListenerBlock _Nonnull inListenerBlock)
	{
		auto result = AudioObjectAddPropertyListenerBlock(mObjectID, &inAddress, inDispatchQueue, inListenerBlock);
		ThrowIfCAAudioObjectError(result, "AudioObjectAddPropertyListenerBlock");
	}

	inline void RemovePropertyListenerBlock(const AudioObjectPropertyAddress& inAddress, dispatch_queue_t _Nonnull inDispatchQueue, AudioObjectPropertyListenerBlock _Nonnull inListenerBlock)
	{
		auto result = AudioObjectRemovePropertyListenerBlock(mObjectID, &inAddress, inDispatchQueue, inListenerBlock);
		ThrowIfCAAudioObjectError(result, "AudioObjectRemovePropertyListenerBlock");
	}

#pragma mark AudioObject Properties

	inline AudioClassID BaseClass() const
	{
		return ArithmeticProperty<AudioClassID>(CAPropertyAddress(kAudioObjectPropertyBaseClass));
	}

	inline AudioClassID Class() const
	{
		return ArithmeticProperty<AudioClassID>(CAPropertyAddress(kAudioObjectPropertyClass));
	}

	inline AudioObjectID OwnerID() const
	{
		return ArithmeticProperty<AudioObjectID>(CAPropertyAddress(kAudioObjectPropertyOwner));
	}

	inline HALAudioObject Owner() const
	{
		return HALAudioObject(OwnerID());
	}

	inline CFString Name() const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioObjectPropertyName));
	}

	inline CFString ModelName() const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioObjectPropertyModelName));
	}

	inline CFString Manufacturer() const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioObjectPropertyManufacturer));
	}

	inline CFString ElementName(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioObjectPropertyElementName, inScope, inElement));
	}

	inline CFString ElementCategoryName(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioObjectPropertyElementCategoryName, inScope, inElement));
	}

	inline CFString ElementNumberName(AudioObjectPropertyScope inScope, AudioObjectPropertyElement inElement) const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioObjectPropertyElementNumberName, inScope, inElement));
	}

	inline std::vector<AudioObjectID> OwnedObjectIDs() const
	{
		return ArrayProperty<AudioObjectID>(CAPropertyAddress(kAudioObjectPropertyOwnedObjects));
	}

	std::vector<HALAudioObject> OwnedObjects() const
	{
		auto vec = OwnedObjectIDs();
		std::vector<HALAudioObject> result(vec.size());
		std::transform(vec.cbegin(), vec.cend(), result.begin(), [](AudioObjectID objectID) { return HALAudioObject(objectID); });
		return result;
	}

	//	kAudioObjectPropertyIdentify            = 'iden',

	inline CFString SerialNumber() const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioObjectPropertySerialNumber));
	}

	inline CFString FirmwareVersion() const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioObjectPropertyFirmwareVersion));
	}

protected:

	// The underlying object ID
	AudioObjectID mObjectID;

};

} // namespace SFB
