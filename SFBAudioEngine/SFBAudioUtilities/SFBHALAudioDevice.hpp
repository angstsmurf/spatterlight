//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import "SFBHALAudioObject.hpp"
#import "SFBHALAudioStream.hpp"

namespace SFB {

class HALAudioDevice : public HALAudioObject
{

public:

	/// Creates an unknown @c SFBHALAudioObject
	constexpr HALAudioDevice() noexcept = default;

	/// Copy constructor
	constexpr HALAudioDevice(const HALAudioDevice& rhs) noexcept = default;

	/// Assignment operator
	HALAudioDevice& operator=(const HALAudioDevice& rhs) noexcept = default;

	/// Destructor
	virtual ~HALAudioDevice() = default;

	/// Move constructor
	HALAudioDevice(HALAudioDevice&& rhs) = default;

	/// Move assignment operator
	HALAudioDevice& operator=(HALAudioDevice&& rhs) = default;


	/// Creates a @c HALAudioDevice with the specified objectID
	inline constexpr HALAudioDevice(AudioObjectID objectID) noexcept
	: HALAudioObject(objectID)
	{}


	//	kAudioDevicePropertyConfigurationApplication        = 'capp',

	inline CFString UID() const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioDevicePropertyDeviceUID));
	}

	inline CFString ModelUID() const
	{
		return CFTypeProperty<CFStringRef>(CAPropertyAddress(kAudioDevicePropertyModelUID));
	}

	//	kAudioDevicePropertyTransportType                   = 'tran',
	//	kAudioDevicePropertyRelatedDevices                  = 'akin',
	//	kAudioDevicePropertyClockDomain                     = 'clkd',
	//	kAudioDevicePropertyDeviceIsAlive                   = 'livn',
	//	kAudioDevicePropertyDeviceIsRunning                 = 'goin',
	//	kAudioDevicePropertyDeviceCanBeDefaultDevice        = 'dflt',
	//	kAudioDevicePropertyDeviceCanBeDefaultSystemDevice  = 'sflt',

	inline UInt32 Latency(HALAudioObjectDirectionalScope scope) const
	{
		return ArithmeticProperty<UInt32>(CAPropertyAddress(kAudioDevicePropertyLatency, scope == HALAudioObjectDirectionalScope::input ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput));
	}

	inline std::vector<AudioObjectID> StreamIDs(HALAudioObjectDirectionalScope scope) const
	{
		return ArrayProperty<AudioObjectID>(CAPropertyAddress(kAudioDevicePropertyStreams, scope == HALAudioObjectDirectionalScope::input ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput));
	}

	std::vector<HALAudioStream> Streams(HALAudioObjectDirectionalScope scope) const
	{
		auto vec = StreamIDs(scope);
		std::vector<HALAudioStream> result(vec.size());
		std::transform(vec.cbegin(), vec.cend(), result.begin(), [](AudioObjectID objectID) { return HALAudioStream(objectID); });
		return result;
	}

	//	kAudioObjectPropertyControlList                     = 'ctrl',

	inline UInt32 SafetyOffset(HALAudioObjectDirectionalScope scope) const
	{
		return ArithmeticProperty<UInt32>(CAPropertyAddress(kAudioDevicePropertySafetyOffset, scope == HALAudioObjectDirectionalScope::input ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput));
	}

	inline Float64 NominalSampleRate() const
	{
		return ArithmeticProperty<Float64>(CAPropertyAddress(kAudioDevicePropertyNominalSampleRate));
	}

	//	kAudioDevicePropertyAvailableNominalSampleRates     = 'nsr#',
	//	kAudioDevicePropertyIcon                            = 'icon',
	//	kAudioDevicePropertyIsHidden                        = 'hidn',
	//	kAudioDevicePropertyPreferredChannelsForStereo      = 'dch2',
	//	kAudioDevicePropertyPreferredChannelLayout          = 'srnd'

	//	kAudioDevicePropertyPlugIn                          = 'plug',
	//	kAudioDevicePropertyDeviceHasChanged                = 'diff',
	//	kAudioDevicePropertyDeviceIsRunningSomewhere        = 'gone',
	//	kAudioDeviceProcessorOverload                       = 'over',
	//	kAudioDevicePropertyIOStoppedAbnormally             = 'stpd',
	//	kAudioDevicePropertyHogMode                         = 'oink',

	inline UInt32 BufferFrameSize() const
	{
		return ArithmeticProperty<UInt32>(CAPropertyAddress(kAudioDevicePropertyBufferFrameSize));
	}

	//	kAudioDevicePropertyBufferFrameSizeRange            = 'fsz#',
	//	kAudioDevicePropertyUsesVariableBufferFrameSizes    = 'vfsz',
	//	kAudioDevicePropertyIOCycleUsage                    = 'ncyc',
	//	kAudioDevicePropertyStreamConfiguration             = 'slay',
	//	kAudioDevicePropertyIOProcStreamUsage               = 'suse',
	//	kAudioDevicePropertyActualSampleRate                = 'asrt',
	//	kAudioDevicePropertyClockDevice                     = 'apcd',
	//	kAudioDevicePropertyIOThreadOSWorkgroup				= 'oswg'

};

} // namespace SFB
