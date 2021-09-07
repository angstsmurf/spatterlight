//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import "SFBHALAudioObject.hpp"
#import "SFBHALAudioDevice.hpp"

namespace SFB {

class HALAudioSystemObject : public HALAudioObject
{

public:

	/// Creates an @c HALAudioSystemObject
	inline constexpr HALAudioSystemObject() noexcept
	: HALAudioObject(kAudioObjectSystemObject)
	{}

	/// Copy constructor
	constexpr HALAudioSystemObject(const HALAudioSystemObject& rhs) = default;

	/// Assignment operator
	HALAudioSystemObject& operator=(const HALAudioSystemObject& rhs) = default;

	/// Destructor
	virtual ~HALAudioSystemObject() = default;

	// Move constructor
	HALAudioSystemObject(HALAudioSystemObject&& rhs) = default;

	// Move assignment operator
	HALAudioSystemObject& operator=(HALAudioSystemObject&& rhs) = default;


	inline std::vector<AudioObjectID> DeviceIDs() const
	{
		return ArrayProperty<AudioObjectID>(CAPropertyAddress(kAudioHardwarePropertyDevices));
	}

	std::vector<HALAudioDevice> Devices() const
	{
		auto vec = DeviceIDs();
		std::vector<HALAudioDevice> result(vec.size());
		std::transform(vec.cbegin(), vec.cend(), result.begin(), [](AudioObjectID objectID) { return HALAudioDevice(objectID); });
		return result;
	}

	inline AudioObjectID DefaultInputDeviceID() const
	{
		return ArithmeticProperty<AudioObjectID>(CAPropertyAddress(kAudioHardwarePropertyDefaultInputDevice));
	}

	inline HALAudioObject DefaultInputDevice() const
	{
		return HALAudioObject(DefaultInputDeviceID());
	}

	inline AudioObjectID DefaultOutputDeviceID() const
	{
		return ArithmeticProperty<AudioObjectID>(CAPropertyAddress(kAudioHardwarePropertyDefaultOutputDevice));
	}

	inline HALAudioObject DefaultOutputDevice() const
	{
		return HALAudioObject(DefaultOutputDeviceID());
	}

	inline AudioObjectID DefaultSystemOutputDeviceID() const
	{
		return ArithmeticProperty<AudioObjectID>(CAPropertyAddress(kAudioHardwarePropertyDefaultSystemOutputDevice));
	}

	inline HALAudioObject DefaultSystemOutputDevice() const
	{
		return HALAudioObject(DefaultSystemOutputDeviceID());
	}

	AudioObjectID AudioDeviceIDForUID(CFStringRef _Nonnull inUID) const
	{
		AudioObjectID deviceID;
		AudioValueTranslation valueTranslation = { &inUID, sizeof(CFStringRef), &deviceID, sizeof(AudioObjectID) };
		CAPropertyAddress objectPropertyAddress(kAudioHardwarePropertyDeviceForUID);
		UInt32 size = sizeof(AudioValueTranslation);
		GetPropertyData(objectPropertyAddress, 0, nullptr, size, &valueTranslation);
		return deviceID;
	}

	inline HALAudioDevice AudioDeviceForUID(CFStringRef _Nonnull inUID) const
	{
		return HALAudioDevice(AudioDeviceIDForUID(inUID));
	}

	//	kAudioHardwarePropertyMixStereoToMono                       = 'stmo',
	//	kAudioHardwarePropertyPlugInList                            = 'plg#',
	//	kAudioHardwarePropertyTranslateBundleIDToPlugIn             = 'bidp',
	//	kAudioHardwarePropertyTransportManagerList                  = 'tmg#',
	//	kAudioHardwarePropertyTranslateBundleIDToTransportManager   = 'tmbi',
	//	kAudioHardwarePropertyBoxList                               = 'box#',
	//	kAudioHardwarePropertyTranslateUIDToBox                     = 'uidb',
	//	kAudioHardwarePropertyClockDeviceList                       = 'clk#',
	//	kAudioHardwarePropertyTranslateUIDToClockDevice             = 'uidc',
	//	kAudioHardwarePropertyProcessIsMaster                       = 'mast',
	//	kAudioHardwarePropertyIsInitingOrExiting                    = 'inot',
	//	kAudioHardwarePropertyUserIDChanged                         = 'euid',
	//	kAudioHardwarePropertyProcessIsAudible                      = 'pmut',
	//	kAudioHardwarePropertySleepingIsAllowed                     = 'slep',
	//	kAudioHardwarePropertyUnloadingIsAllowed                    = 'unld',
	//	kAudioHardwarePropertyHogModeIsAllowed                      = 'hogr',
	//	kAudioHardwarePropertyUserSessionIsActiveOrHeadless         = 'user',
	//	kAudioHardwarePropertyServiceRestarted                      = 'srst',
	//	kAudioHardwarePropertyPowerHint                             = 'powh'

};

} // namespace SFB
