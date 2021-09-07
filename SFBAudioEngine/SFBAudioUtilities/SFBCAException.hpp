//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <cctype>
#import <cstring>
#import <exception>
#import <system_error>

#import <CoreAudio/CoreAudio.h>
#import <AudioToolbox/AudioToolbox.h>

namespace SFB {

/// CoreAudio and related framework common error codes
enum class CAGeneralErrorCode {
	noError = 0,

	// CoreAudioBaseTypes.h
	unimplementedError 		= kAudio_UnimplementedError,
	fileNotFoundError 		= kAudio_FileNotFoundError,
	filePermissionError 	= kAudio_FilePermissionError,
	tooManyFilesOpenError 	= kAudio_TooManyFilesOpenError,
	badFilePathError 		= kAudio_BadFilePathError,
	paramError 				= kAudio_ParamError,
	memFullError 			= kAudio_MemFullError,
};

/// AudioObject error codes
enum class CAAudioObjectErrorCode {
	noError = 0,

	// AudioHardwareBase.h
//	hardwareNoError 					= kAudioHardwareNoError,
	hardwareNotRunningError 			= kAudioHardwareNotRunningError,
	hardwareUnspecifiedError 			= kAudioHardwareUnspecifiedError,
	hardwareUnknownPropertyError 		= kAudioHardwareUnknownPropertyError,
	hardwareBadPropertySizeError 		= kAudioHardwareBadPropertySizeError,
	hardwareIllegalOperationError 		= kAudioHardwareIllegalOperationError,
	hardwareBadObjectError 				= kAudioHardwareBadObjectError,
	hardwareBadDeviceError 				= kAudioHardwareBadDeviceError,
	hardwareBadStreamError 				= kAudioHardwareBadStreamError,
	hardwareUnsupportedOperationError 	= kAudioHardwareUnsupportedOperationError,
	deviceUnsupportedFormatError 		= kAudioDeviceUnsupportedFormatError,
	devicePermissionsError 				= kAudioDevicePermissionsError,
};

/// AudioUnit error codes
enum class CAAudioUnitErrorCode {
	noError = 0,

	// AUComponent.h
	invalidProperty 				= kAudioUnitErr_InvalidProperty,
	invalidParameter 				= kAudioUnitErr_InvalidParameter,
	invalidElement 					= kAudioUnitErr_InvalidElement,
	noConnection 					= kAudioUnitErr_NoConnection,
	failedInitialization 			= kAudioUnitErr_FailedInitialization,
	tooManyFramesToProcess 			= kAudioUnitErr_TooManyFramesToProcess,
	invalidFile 					= kAudioUnitErr_InvalidFile,
	unknownFileType 				= kAudioUnitErr_UnknownFileType,
	fileNotSpecified 				= kAudioUnitErr_FileNotSpecified,
	formatNotSupported 				= kAudioUnitErr_FormatNotSupported,
	uninitialized 					= kAudioUnitErr_Uninitialized,
	invalidScope 					= kAudioUnitErr_InvalidScope,
	propertyNotWritable 			= kAudioUnitErr_PropertyNotWritable,
	cannotDoInCurrentContext	 	= kAudioUnitErr_CannotDoInCurrentContext,
	invalidPropertyValue 			= kAudioUnitErr_InvalidPropertyValue,
	propertyNotInUse 				= kAudioUnitErr_PropertyNotInUse,
	initialized 					= kAudioUnitErr_Initialized,
	invalidOfflineRender 			= kAudioUnitErr_InvalidOfflineRender,
	unauthorized 					= kAudioUnitErr_Unauthorized,
	midiOutputBufferFull 			= kAudioUnitErr_MIDIOutputBufferFull,
	componentInstanceTimedOut 		= kAudioComponentErr_InstanceTimedOut,
	componentInstanceInvalidated 	= kAudioComponentErr_InstanceInvalidated,
	renderTimeout 					= kAudioUnitErr_RenderTimeout,
	extensionNotFound 				= kAudioUnitErr_ExtensionNotFound,
	invalidParameterValue 			= kAudioUnitErr_InvalidParameterValue,
	invalidFilePath 				= kAudioUnitErr_InvalidFilePath,
	missingKey 						= kAudioUnitErr_MissingKey,

	componentDuplicateDescription 		= kAudioComponentErr_DuplicateDescription,
	componentUnsupportedType 			= kAudioComponentErr_UnsupportedType,
	componentTooManyInstances 			= kAudioComponentErr_TooManyInstances,
	componentNotPermitted 				= kAudioComponentErr_NotPermitted,
	componentInitializationTimedOut 	= kAudioComponentErr_InitializationTimedOut,
	componentInvalidFormat 				= kAudioComponentErr_InvalidFormat,
};

// AudioFormat error codes
enum class CAAudioFormatErrorCode {
	noError = 0,

	// AudioFormat.h
	unspecifiedError 			= kAudioFormatUnspecifiedError,
	unsupportedPropertyError 	= kAudioFormatUnsupportedPropertyError,
	badPropertySizeError 		= kAudioFormatBadPropertySizeError,
	badSpecifierSizeError 		= kAudioFormatBadSpecifierSizeError,
	unsupportedDataFormatError 	= kAudioFormatUnsupportedDataFormatError,
	unknownFormatError 			= kAudioFormatUnknownFormatError,
};

// AudioCodec error codes
enum class CAAudioCodecErrorCode {
	noError = 0,

	// AudioCodec.h
	unspecifiedError 			= kAudioCodecUnspecifiedError,
	unknownPropertyError 		= kAudioCodecUnknownPropertyError,
	badPropertySizeError 		= kAudioCodecBadPropertySizeError,
	illegalOperationError 		= kAudioCodecIllegalOperationError,
	unsupportedFormatError 		= kAudioCodecUnsupportedFormatError,
	stateError 					= kAudioCodecStateError,
	notEnoughBufferSpaceError 	= kAudioCodecNotEnoughBufferSpaceError,
	badDataError 				= kAudioCodecBadDataError,
};

/// AudioConverter error codes
enum class CAAudioConverterErrorCode {
	noError = 0,

	// AudioConverter.h
	formatNotSupported 					= kAudioConverterErr_FormatNotSupported,
	operationNotSupported 				= kAudioConverterErr_OperationNotSupported,
	propertyNotSupported 				= kAudioConverterErr_PropertyNotSupported,
	invalidInputSize 					= kAudioConverterErr_InvalidInputSize,
	invalidOutputSize 					= kAudioConverterErr_InvalidOutputSize,
	unspecifiedError 					= kAudioConverterErr_UnspecifiedError,
	badPropertySizeError 				= kAudioConverterErr_BadPropertySizeError,
	requiresPacketDescriptionsError 	= kAudioConverterErr_RequiresPacketDescriptionsError,
	inputSampleRateOutOfRange 			= kAudioConverterErr_InputSampleRateOutOfRange,
	outputSampleRateOutOfRange 			= kAudioConverterErr_OutputSampleRateOutOfRange,
#if TARGET_OS_IPHONE
	hardwareInUse 						= kAudioConverterErr_HardwareInUse,
	noHardwarePermission 				= kAudioConverterErr_NoHardwarePermission,
#endif
};

/// AudioFile error codes
enum class CAAudioFileErrorCode {
	noError = 0,

	// AudioFile.h
	unspecifiedError 				= kAudioFileUnspecifiedError,
	unsupportedFileTypeError 		= kAudioFileUnsupportedFileTypeError,
	unsupportedDataFormatError 		= kAudioFileUnsupportedDataFormatError,
	unsupportedPropertyError 		= kAudioFileUnsupportedPropertyError,
	badPropertySizeError 			= kAudioFileBadPropertySizeError,
	permissionsError 				= kAudioFilePermissionsError,
	notOptimizedError 				= kAudioFileNotOptimizedError,

	invalidChunkError 				= kAudioFileInvalidChunkError,
	doesNotAllow64BitDataSizeError 	= kAudioFileDoesNotAllow64BitDataSizeError,
	invalidPacketOffsetError 		= kAudioFileInvalidPacketOffsetError,
	invalidPacketDependencyError 	= kAudioFileInvalidPacketDependencyError,
	invalidFileError 				= kAudioFileInvalidFileError,
	operationNotSupportedError 		= kAudioFileOperationNotSupportedError,

	notOpenError 					= kAudioFileNotOpenError,
	endOfFileError 					= kAudioFileEndOfFileError,
	positionError 					= kAudioFilePositionError,
	fileNotFoundError 				= kAudioFileFileNotFoundError,
};

/// ExtAudioFile error codes
enum class CAExtAudioFileErrorCode {
	noError = 0,

	// ExtAudioFile.h
	invalidProperty 					= kExtAudioFileError_InvalidProperty,
	invalidPropertySize 				= kExtAudioFileError_InvalidPropertySize,
	nonPCMClientFormat 					= kExtAudioFileError_NonPCMClientFormat,
	invalidChannelMap 					= kExtAudioFileError_InvalidChannelMap,
	invalidOperationOrder 				= kExtAudioFileError_InvalidOperationOrder,
	invalidDataFormat 					= kExtAudioFileError_InvalidDataFormat,
	maxPacketSizeUnknown 				= kExtAudioFileError_MaxPacketSizeUnknown,
	invalidSeek 						= kExtAudioFileError_InvalidSeek,
	asyncWriteTooLarge 					= kExtAudioFileError_AsyncWriteTooLarge,
	asyncWriteBufferOverflow 			= kExtAudioFileError_AsyncWriteBufferOverflow,
#if TARGET_OS_IPHONE
	codecUnavailableInputConsumed 		= kExtAudioFileError_CodecUnavailableInputConsumed,
	codecUnavailableInputNotConsumed 	= kExtAudioFileError_CodecUnavailableInputNotConsumed,
#endif

};

namespace detail {

class CAAudioObjectErrorCategory : public std::error_category
{

public:

	virtual const char * name() const noexcept override final
	{
		return "AudioObject";
	}

	virtual std::string message(int condition) const override final
	{
		switch(static_cast<CAAudioObjectErrorCode>(condition)) {
			case CAAudioObjectErrorCode::hardwareNotRunningError:				return "The function call requires that the hardware be running but it isn't";
			case CAAudioObjectErrorCode::hardwareUnspecifiedError:				return "The function call failed while doing something that doesn't provide any error messages";
			case CAAudioObjectErrorCode::hardwareUnknownPropertyError:			return "The AudioObject doesn't know about the property at the given address";
			case CAAudioObjectErrorCode::hardwareBadPropertySizeError:			return "An improperly sized buffer was provided when accessing the data of a property";
			case CAAudioObjectErrorCode::hardwareIllegalOperationError:			return "The requested operation couldn't be completed";
			case CAAudioObjectErrorCode::hardwareBadObjectError:				return "The AudioObjectID passed to the function doesn't map to a valid AudioObject";
			case CAAudioObjectErrorCode::hardwareBadDeviceError:				return "The AudioObjectID passed to the function doesn't map to a valid AudioDevice";
			case CAAudioObjectErrorCode::hardwareBadStreamError:				return "The AudioObjectID passed to the function doesn't map to a valid AudioStream";
			case CAAudioObjectErrorCode::hardwareUnsupportedOperationError: 	return "The AudioObject doesn't support the requested operation";
			case CAAudioObjectErrorCode::deviceUnsupportedFormatError:			return "The AudioStream doesn't support the requested format";
			case CAAudioObjectErrorCode::devicePermissionsError:				return "The requested operation can't be completed because the process doesn't have permission";

			default:
				switch(static_cast<CAGeneralErrorCode>(condition)) {
					case CAGeneralErrorCode::noError: 					return "The function call completed successfully";

					case CAGeneralErrorCode::unimplementedError: 		return "Unimplemented core routine";
					case CAGeneralErrorCode::fileNotFoundError: 		return "File not found";
					case CAGeneralErrorCode::filePermissionError: 		return "File cannot be opened due to either file, directory, or sandbox permissions";
					case CAGeneralErrorCode::tooManyFilesOpenError: 	return "File cannot be opened because too many files are already open";
					case CAGeneralErrorCode::badFilePathError: 			return "File cannot be opened because the specified path is malformed";
					case CAGeneralErrorCode::paramError: 				return "Error in user parameter list";
					case CAGeneralErrorCode::memFullError: 				return "Not enough room in heap zone";

					default:											return "unknown";
				}
		}
	}
};

class CAAudioUnitErrorCategory : public std::error_category
{

public:

	virtual const char * name() const noexcept override final
	{
		return "AudioUnit";
	}

	virtual std::string message(int condition) const override final
	{
		switch(static_cast<CAAudioUnitErrorCode>(condition)) {
			case CAAudioUnitErrorCode::invalidProperty: 				return "The property is not supported";
			case CAAudioUnitErrorCode::invalidParameter: 				return "The parameter is not supported";
			case CAAudioUnitErrorCode::invalidElement: 					return "The specified element is not valid";
			case CAAudioUnitErrorCode::noConnection: 					return "There is no connection (generally an audio unit is asked to render but it has not input from which to gather data)";
			case CAAudioUnitErrorCode::failedInitialization: 			return "The audio unit is unable to be initialized";
			case CAAudioUnitErrorCode::tooManyFramesToProcess: 			return "When an audio unit is initialized it has a value which specifies the max number of frames it will be asked to render at any given time. If an audio unit is asked to render more than this, this error is returned";
			case CAAudioUnitErrorCode::invalidFile: 					return "If an audio unit uses external files as a data source, this error is returned if a file is invalid (Apple's DLS synth returns this error)";
			case CAAudioUnitErrorCode::unknownFileType: 				return "If an audio unit uses external files as a data source, this error is returned if a file is invalid (Apple's DLS synth returns this error)";
			case CAAudioUnitErrorCode::fileNotSpecified: 				return "If an audio unit uses external files as a data source, this error is returned if a file hasn't been set on it (Apple's DLS synth returns this error)";
			case CAAudioUnitErrorCode::formatNotSupported: 				return "Returned if an input or output format is not supported";
			case CAAudioUnitErrorCode::uninitialized: 					return "Returned if an operation requires an audio unit to be initialized and it is not";
			case CAAudioUnitErrorCode::invalidScope: 					return "The specified scope is invalid";
			case CAAudioUnitErrorCode::propertyNotWritable: 			return "The property cannot be written";
			case CAAudioUnitErrorCode::cannotDoInCurrentContext: 		return "Returned when an audio unit is in a state where it can't perform the requested action now - but it could later. It's usually used to guard a render operation when a reconfiguration of its internal state is being performed";
			case CAAudioUnitErrorCode::invalidPropertyValue: 			return "The property is valid, but the value of the property being provided is not";
			case CAAudioUnitErrorCode::propertyNotInUse: 				return "Returned when a property is valid, but it hasn't been set to a valid value at this time";
			case CAAudioUnitErrorCode::initialized: 					return "Indicates the operation cannot be performed because the audio unit is initialized";
			case CAAudioUnitErrorCode::invalidOfflineRender: 			return "Used to indicate that the offline render operation is invalid. For instance, when the audio unit needs to be pre-flighted, but it hasn't been";
			case CAAudioUnitErrorCode::unauthorized: 					return "Returned by either Open or Initialize, this error is used to indicate that the audio unit is not authorised, that it cannot be used. A host can then present a UI to notify the user the audio unit is not able to be used in its current state";
			case CAAudioUnitErrorCode::midiOutputBufferFull: 			return "Returned during the render call, if the audio unit produces more MIDI output, than the default allocated buffer. The audio unit can provide a size hint, in case it needs a larger buffer. See the documentation for AUAudioUnit's MIDIOutputBufferSizeHint property";
			case CAAudioUnitErrorCode::componentInstanceTimedOut: 		return "kAudioComponentErr_InstanceTimedOut";
			case CAAudioUnitErrorCode::componentInstanceInvalidated: 	return "The component instance's implementation is not available, most likely because the process that published it is no longer running";
			case CAAudioUnitErrorCode::renderTimeout: 					return "The audio unit did not satisfy the render request in time";
			case CAAudioUnitErrorCode::extensionNotFound: 				return "The specified identifier did not match any Audio Unit Extensions";
			case CAAudioUnitErrorCode::invalidParameterValue: 			return "The parameter value is not supported, e.g. the value specified is NaN or infinite";
			case CAAudioUnitErrorCode::invalidFilePath: 				return "The file path that was passed is not supported. It is either too long or contains invalid characters";
			case CAAudioUnitErrorCode::missingKey: 						return "A required key is missing from a dictionary object";

			case CAAudioUnitErrorCode::componentDuplicateDescription: 		return "A non-unique component description was provided to AudioOutputUnitPublish";
			case CAAudioUnitErrorCode::componentUnsupportedType: 			return "An unsupported component type was provided to AudioOutputUnitPublish";
			case CAAudioUnitErrorCode::componentTooManyInstances: 			return "Components published via AudioOutputUnitPublish may only have one instance";
			case CAAudioUnitErrorCode::componentNotPermitted: 				return "App needs \"inter-app-audio\" entitlement or host app needs \"audio\" in its UIBackgroundModes. Or app is trying to register a component not declared in its Info.plist";
			case CAAudioUnitErrorCode::componentInitializationTimedOut: 	return "Host did not render in a timely manner; must uninitialize and reinitialize";
			case CAAudioUnitErrorCode::componentInvalidFormat: 				return "Inter-app AU element formats must have sample rates matching the hardware";

			default:
				switch(static_cast<CAGeneralErrorCode>(condition)) {
					case CAGeneralErrorCode::noError: 					return "The function call completed successfully";

					case CAGeneralErrorCode::unimplementedError: 		return "Unimplemented core routine";
					case CAGeneralErrorCode::fileNotFoundError: 		return "File not found";
					case CAGeneralErrorCode::filePermissionError: 		return "File cannot be opened due to either file, directory, or sandbox permissions";
					case CAGeneralErrorCode::tooManyFilesOpenError: 	return "File cannot be opened because too many files are already open";
					case CAGeneralErrorCode::badFilePathError: 			return "File cannot be opened because the specified path is malformed";
					case CAGeneralErrorCode::paramError: 				return "Error in user parameter list";
					case CAGeneralErrorCode::memFullError: 				return "Not enough room in heap zone";

					default:											return "unknown";
				}
		}
	}
};

class CAAudioFormatErrorCategory : public std::error_category
{

public:

	virtual const char * name() const noexcept override final
	{
		return "AudioFormat";
	}

	virtual std::string message(int condition) const override final
	{
		switch(static_cast<CAAudioFormatErrorCode>(condition)) {
			case CAAudioFormatErrorCode::unspecifiedError: 				return "kAudioFormatUnspecifiedError";
			case CAAudioFormatErrorCode::unsupportedPropertyError: 		return "kAudioFormatUnsupportedPropertyError";
			case CAAudioFormatErrorCode::badPropertySizeError: 			return "kAudioFormatBadPropertySizeError";
			case CAAudioFormatErrorCode::badSpecifierSizeError: 		return "kAudioFormatBadSpecifierSizeError";
			case CAAudioFormatErrorCode::unsupportedDataFormatError: 	return "kAudioFormatUnsupportedDataFormatError";
			case CAAudioFormatErrorCode::unknownFormatError: 			return "kAudioFormatUnknownFormatError";

			default:
				switch(static_cast<CAGeneralErrorCode>(condition)) {
					case CAGeneralErrorCode::noError: 					return "The function call completed successfully";

					case CAGeneralErrorCode::unimplementedError: 		return "Unimplemented core routine";
					case CAGeneralErrorCode::fileNotFoundError: 		return "File not found";
					case CAGeneralErrorCode::filePermissionError: 		return "File cannot be opened due to either file, directory, or sandbox permissions";
					case CAGeneralErrorCode::tooManyFilesOpenError: 	return "File cannot be opened because too many files are already open";
					case CAGeneralErrorCode::badFilePathError: 			return "File cannot be opened because the specified path is malformed";
					case CAGeneralErrorCode::paramError: 				return "Error in user parameter list";
					case CAGeneralErrorCode::memFullError: 				return "Not enough room in heap zone";

					default:											return "unknown";
				}
		}
	}
};

class CAAudioCodecErrorCategory : public std::error_category
{

public:

	virtual const char * name() const noexcept override final
	{
		return "AudioCodec";
	}

	virtual std::string message(int condition) const override final
	{
		switch(static_cast<CAAudioCodecErrorCode>(condition)) {
			case CAAudioCodecErrorCode::unspecifiedError: 				return "kAudioCodecUnspecifiedError";
			case CAAudioCodecErrorCode::unknownPropertyError: 			return "kAudioCodecUnknownPropertyError";
			case CAAudioCodecErrorCode::badPropertySizeError: 			return "kAudioCodecBadPropertySizeError";
			case CAAudioCodecErrorCode::illegalOperationError: 			return "kAudioCodecIllegalOperationError";
			case CAAudioCodecErrorCode::unsupportedFormatError: 		return "kAudioCodecUnsupportedFormatError";
			case CAAudioCodecErrorCode::stateError: 					return "kAudioCodecStateError";
			case CAAudioCodecErrorCode::notEnoughBufferSpaceError: 		return "kAudioCodecNotEnoughBufferSpaceError";
			case CAAudioCodecErrorCode::badDataError: 					return "kAudioCodecBadDataError";

			default:
				switch(static_cast<CAGeneralErrorCode>(condition)) {
					case CAGeneralErrorCode::noError: 					return "The function call completed successfully";

					case CAGeneralErrorCode::unimplementedError: 		return "Unimplemented core routine";
					case CAGeneralErrorCode::fileNotFoundError: 		return "File not found";
					case CAGeneralErrorCode::filePermissionError: 		return "File cannot be opened due to either file, directory, or sandbox permissions";
					case CAGeneralErrorCode::tooManyFilesOpenError: 	return "File cannot be opened because too many files are already open";
					case CAGeneralErrorCode::badFilePathError: 			return "File cannot be opened because the specified path is malformed";
					case CAGeneralErrorCode::paramError: 				return "Error in user parameter list";
					case CAGeneralErrorCode::memFullError: 				return "Not enough room in heap zone";

					default:											return "unknown";
				}
		}
	}
};

class CAAudioConverterErrorCategory : public std::error_category
{

public:

	virtual const char * name() const noexcept override final
	{
		return "AudioConverter";
	}

	virtual std::string message(int condition) const override final
	{
		switch(static_cast<CAAudioConverterErrorCode>(condition)) {
			case CAAudioConverterErrorCode::formatNotSupported: 				return "kAudioConverterErr_FormatNotSupported or kAudioFileUnsupportedDataFormatError";
			case CAAudioConverterErrorCode::operationNotSupported: 				return "kAudioConverterErr_OperationNotSupported";
			case CAAudioConverterErrorCode::propertyNotSupported: 				return "kAudioConverterErr_PropertyNotSupported";
			case CAAudioConverterErrorCode::invalidInputSize: 					return "kAudioConverterErr_InvalidInputSize";
			case CAAudioConverterErrorCode::invalidOutputSize: 					return "kAudioConverterErr_InvalidOutputSize";
			case CAAudioConverterErrorCode::unspecifiedError: 					return "kAudioConverterErr_UnspecifiedError";
			case CAAudioConverterErrorCode::badPropertySizeError: 				return "kAudioConverterErr_BadPropertySizeError";
			case CAAudioConverterErrorCode::requiresPacketDescriptionsError: 	return "kAudioConverterErr_RequiresPacketDescriptionsError";
			case CAAudioConverterErrorCode::inputSampleRateOutOfRange: 			return "kAudioConverterErr_InputSampleRateOutOfRange";
			case CAAudioConverterErrorCode::outputSampleRateOutOfRange: 		return "kAudioConverterErr_OutputSampleRateOutOfRange";
#if TARGET_OS_IPHONE
			case CAAudioConverterErrorCode::hardwareInUse: 						return "kAudioConverterErr_HardwareInUse";
			case CAAudioConverterErrorCode::noHardwarePermission: 				return "kAudioConverterErr_NoHardwarePermission";
#endif

			default:
				switch(static_cast<CAGeneralErrorCode>(condition)) {
					case CAGeneralErrorCode::noError: 					return "The function call completed successfully";

					case CAGeneralErrorCode::unimplementedError: 		return "Unimplemented core routine";
					case CAGeneralErrorCode::fileNotFoundError: 		return "File not found";
					case CAGeneralErrorCode::filePermissionError: 		return "File cannot be opened due to either file, directory, or sandbox permissions";
					case CAGeneralErrorCode::tooManyFilesOpenError: 	return "File cannot be opened because too many files are already open";
					case CAGeneralErrorCode::badFilePathError: 			return "File cannot be opened because the specified path is malformed";
					case CAGeneralErrorCode::paramError: 				return "Error in user parameter list";
					case CAGeneralErrorCode::memFullError: 				return "Not enough room in heap zone";

					default:											return "unknown";
				}
		}
	}
};

class CAAudioFileErrorCategory : public std::error_category
{

public:

	virtual const char * name() const noexcept override final
	{
		return "AudioFile";
	}

	virtual std::string message(int condition) const override final
	{
		switch(static_cast<CAAudioFileErrorCode>(condition)) {
			case CAAudioFileErrorCode::unspecifiedError: 				return "An unspecified error has occurred";
			case CAAudioFileErrorCode::unsupportedFileTypeError: 		return "The file type is not supported";
			case CAAudioFileErrorCode::unsupportedDataFormatError: 		return "The data format is not supported by this file type";
			case CAAudioFileErrorCode::unsupportedPropertyError: 		return "The property is not supported";
			case CAAudioFileErrorCode::badPropertySizeError: 			return "The size of the property data was not correct";
			case CAAudioFileErrorCode::permissionsError: 				return "The operation violated the file permissions";
			case CAAudioFileErrorCode::notOptimizedError: 				return "There are chunks following the audio data chunk that prevent extending the audio data chunk. The file must be optimized in order to write more audio data.";

			case CAAudioFileErrorCode::invalidChunkError: 				return "The chunk does not exist in the file or is not supported by the file";
			case CAAudioFileErrorCode::doesNotAllow64BitDataSizeError: 	return "The a file offset was too large for the file type. AIFF and WAVE have a 32 bit file size limit.";
			case CAAudioFileErrorCode::invalidPacketOffsetError: 		return "A packet offset was past the end of the file, or not at the end of the file when writing a VBR format, or a corrupt packet size was read when building the packet table.";
			case CAAudioFileErrorCode::invalidPacketDependencyError: 	return "Either the packet dependency info that's necessary for the audio format has not been provided, or the provided packet dependency info indicates dependency on a packet that's unavailable.";
			case CAAudioFileErrorCode::invalidFileError:				return "The file is malformed, or otherwise not a valid instance of an audio file of its type";
			case CAAudioFileErrorCode::operationNotSupportedError: 		return "The operation cannot be performed";

			case CAAudioFileErrorCode::notOpenError:					return "The file is closed";
			case CAAudioFileErrorCode::endOfFileError: 					return "End of file";
			case CAAudioFileErrorCode::positionError: 					return "Invalid file position";
			case CAAudioFileErrorCode::fileNotFoundError: 				return "File not found";

			default:
				switch(static_cast<CAGeneralErrorCode>(condition)) {
					case CAGeneralErrorCode::noError: 					return "The function call completed successfully";

					case CAGeneralErrorCode::unimplementedError: 		return "Unimplemented core routine";
					case CAGeneralErrorCode::fileNotFoundError: 		return "File not found";
					case CAGeneralErrorCode::filePermissionError: 		return "File cannot be opened due to either file, directory, or sandbox permissions";
					case CAGeneralErrorCode::tooManyFilesOpenError: 	return "File cannot be opened because too many files are already open";
					case CAGeneralErrorCode::badFilePathError: 			return "File cannot be opened because the specified path is malformed";
					case CAGeneralErrorCode::paramError: 				return "Error in user parameter list";
					case CAGeneralErrorCode::memFullError: 				return "Not enough room in heap zone";

					default:											return "unknown";
				}
		}
	}
};

class CAExtAudioFileErrorCategory : public std::error_category
{

public:

	virtual const char * name() const noexcept override final
	{
		return "ExtAudioFile";
	}

	virtual std::string message(int condition) const override final
	{
		switch(static_cast<CAExtAudioFileErrorCode>(condition)) {
			case CAExtAudioFileErrorCode::invalidProperty:						return "kExtAudioFileError_InvalidProperty";
			case CAExtAudioFileErrorCode::invalidPropertySize: 					return "kExtAudioFileError_InvalidPropertySize";
			case CAExtAudioFileErrorCode::nonPCMClientFormat: 					return "kExtAudioFileError_NonPCMClientFormat";
			case CAExtAudioFileErrorCode::invalidChannelMap: 					return "number of channels doesn't match format";
			case CAExtAudioFileErrorCode::invalidOperationOrder: 				return "kExtAudioFileError_InvalidOperationOrder";
			case CAExtAudioFileErrorCode::invalidDataFormat: 					return "kExtAudioFileError_InvalidDataFormat";
			case CAExtAudioFileErrorCode::maxPacketSizeUnknown: 				return "kExtAudioFileError_MaxPacketSizeUnknown";
			case CAExtAudioFileErrorCode::invalidSeek: 							return "writing, or offset out of bounds";
			case CAExtAudioFileErrorCode::asyncWriteTooLarge: 					return "kExtAudioFileError_AsyncWriteTooLarge";
			case CAExtAudioFileErrorCode::asyncWriteBufferOverflow: 			return "an async write could not be completed in time";
#if TARGET_OS_IPHONE
			case CAExtAudioFileErrorCode::codecUnavailableInputConsumed:		return "iOS only. Returned when ExtAudioFileWrite was interrupted. You must stop calling ExtAudioFileWrite. If the underlying audio converter can resume after an interruption (see kAudioConverterPropertyCanResumeFromInterruption), you must wait for an EndInterruption notification from AudioSession, and call AudioSessionSetActive(true) before resuming. In this situation, the buffer you provided to ExtAudioFileWrite was successfully consumed and you may proceed to the next buffer";
			case CAExtAudioFileErrorCode::codecUnavailableInputNotConsumed: 	return "iOS only. Returned when ExtAudioFileWrite was interrupted. You must stop calling ExtAudioFileWrite. If the underlying audio converter can resume after an interruption (see kAudioConverterPropertyCanResumeFromInterruption), you must wait for an EndInterruption notification from AudioSession, and call AudioSessionSetActive(true) before resuming. In this situation, the buffer you provided to ExtAudioFileWrite was not successfully consumed and you must try to write it again";
#endif

			default:
				switch(static_cast<CAGeneralErrorCode>(condition)) {
					case CAGeneralErrorCode::noError: 					return "The function call completed successfully";

					case CAGeneralErrorCode::unimplementedError: 		return "Unimplemented core routine";
					case CAGeneralErrorCode::fileNotFoundError: 		return "File not found";
					case CAGeneralErrorCode::filePermissionError: 		return "File cannot be opened due to either file, directory, or sandbox permissions";
					case CAGeneralErrorCode::tooManyFilesOpenError: 	return "File cannot be opened because too many files are already open";
					case CAGeneralErrorCode::badFilePathError: 			return "File cannot be opened because the specified path is malformed";
					case CAGeneralErrorCode::paramError: 				return "Error in user parameter list";
					case CAGeneralErrorCode::memFullError: 				return "Not enough room in heap zone";

					default:											return "unknown";
				}
		}
	}
};

} // namespace detail

extern inline const detail::CAAudioObjectErrorCategory& CAAudioObjectErrorCategory()
{
	static detail::CAAudioObjectErrorCategory c;
	return c;
}

extern inline const detail::CAAudioUnitErrorCategory& CAAudioUnitErrorCategory()
{
	static detail::CAAudioUnitErrorCategory c;
	return c;
}

extern inline const detail::CAAudioFormatErrorCategory& CAAudioFormatErrorCategory()
{
	static detail::CAAudioFormatErrorCategory c;
	return c;
}

extern inline const detail::CAAudioCodecErrorCategory& CAAudioCodecErrorCategory()
{
	static detail::CAAudioCodecErrorCategory c;
	return c;
}

extern inline const detail::CAAudioConverterErrorCategory& CAAudioConverterErrorCategory()
{
	static detail::CAAudioConverterErrorCategory c;
	return c;
}

extern inline const detail::CAAudioFileErrorCategory& CAAudioFileErrorCategory()
{
	static detail::CAAudioFileErrorCategory c;
	return c;
}

extern inline const detail::CAExtAudioFileErrorCategory& CAExtAudioFileErrorCategory()
{
	static detail::CAExtAudioFileErrorCategory c;
	return c;
}

inline std::error_code make_error_code(CAAudioObjectErrorCode e)
{
	return { static_cast<int>(e), CAAudioObjectErrorCategory() };
}

inline std::error_code make_error_code(CAAudioUnitErrorCode e)
{
	return { static_cast<int>(e), CAAudioUnitErrorCategory() };
}

inline std::error_code make_error_code(CAAudioFormatErrorCode e)
{
	return { static_cast<int>(e), CAAudioFormatErrorCategory() };
}

inline std::error_code make_error_code(CAAudioCodecErrorCode e)
{
	return { static_cast<int>(e), CAAudioCodecErrorCategory() };
}

inline std::error_code make_error_code(CAAudioConverterErrorCode e)
{
	return { static_cast<int>(e), CAAudioConverterErrorCategory() };
}

inline std::error_code make_error_code(CAAudioFileErrorCode e)
{
	return { static_cast<int>(e), CAAudioFileErrorCategory() };
}

inline std::error_code make_error_code(CAExtAudioFileErrorCode e)
{
	return { static_cast<int>(e), CAExtAudioFileErrorCategory() };
}

} // namespace SFB

namespace std {

template <> struct is_error_code_enum<SFB::CAAudioObjectErrorCode> : true_type {};
template <> struct is_error_code_enum<SFB::CAAudioUnitErrorCode> : true_type {};
template <> struct is_error_code_enum<SFB::CAAudioFormatErrorCode> : true_type {};
template <> struct is_error_code_enum<SFB::CAAudioCodecErrorCode> : true_type {};
template <> struct is_error_code_enum<SFB::CAAudioConverterErrorCode> : true_type {};
template <> struct is_error_code_enum<SFB::CAAudioFileErrorCode> : true_type {};
template <> struct is_error_code_enum<SFB::CAExtAudioFileErrorCode> : true_type {};

} // namespace std

namespace SFB {

/// Throws a @c std::system_error in the @c CAAudioObjectErrorCategory if @c result!=0
/// @note This is intended for results from the @c AudioObject API
/// @param result An @c OSStatus result code
/// @param operation An optional string describing the operation producing @c result
/// @throw @c std::system_error in the @c CAAudioObjectErrorCategory
inline void ThrowIfCAAudioObjectError(OSStatus result, const char * const operation = nullptr)
{
	if(__builtin_expect(result != 0, 0))
		throw std::system_error(result, CAAudioObjectErrorCategory(), operation);
}

/// Throws a @c std::system_error in the @c CAAudioUnitErrorCategory if @c result!=0
/// @note This is intended for results from the @c AudioUnit API
/// @param result An @c OSStatus result code
/// @param operation An optional string describing the operation producing @c result
/// @throw @c std::system_error in the @c CAAudioUnitErrorCategory
inline void ThrowIfCAAudioUnitError(OSStatus result, const char * const operation = nullptr)
{
	if(__builtin_expect(result != 0, 0))
		throw std::system_error(result, CAAudioUnitErrorCategory(), operation);
}

/// Throws a @c std::system_error in the @c CAAudioFormatErrorCategory if @c result!=0
/// @note This is intended for results from the @c AudioFormat API
/// @param result An @c OSStatus result code
/// @param operation An optional string describing the operation producing @c result
/// @throw @c std::system_error in the @c CAAudioFormatErrorCategory
inline void ThrowIfCAAudioFormatError(OSStatus result, const char * const operation = nullptr)
{
	if(__builtin_expect(result != 0, 0))
		throw std::system_error(result, CAAudioFormatErrorCategory(), operation);
}

/// Throws a @c std::system_error in the @c CAAudioCodecErrorCategory if @c result!=0
/// @note This is intended for results from the @c AudioCodec API
/// @param result An @c OSStatus result code
/// @param operation An optional string describing the operation producing @c result
/// @throw @c std::system_error in the @c CAAudioCodecErrorCategory
inline void ThrowIfCAAudioCodecError(OSStatus result, const char * const operation = nullptr)
{
	if(__builtin_expect(result != 0, 0))
		throw std::system_error(result, CAAudioCodecErrorCategory(), operation);
}

/// Throws a @c std::system_error in the @c CAAudioConverterErrorCategory if @c result!=0
/// @note This is intended for results from the @c AudioConverter API
/// @param result An @c OSStatus result code
/// @param operation An optional string describing the operation producing @c result
/// @throw @c std::system_error in the @c CAAudioConverterErrorCategory
inline void ThrowIfCAAudioConverterError(OSStatus result, const char * const operation = nullptr)
{
	if(__builtin_expect(result != 0, 0))
		throw std::system_error(result, CAAudioConverterErrorCategory(), operation);
}

/// Throws a @c std::system_error in the @c CAAudioFileErrorCategory if @c result!=0
/// @note This is intended for results from the @c AudioFile API
/// @param result An @c OSStatus result code
/// @param operation An optional string describing the operation producing @c result
/// @throw @c std::system_error in the @c CAAudioFileErrorCategory
inline void ThrowIfCAAudioFileError(OSStatus result, const char * const operation = nullptr)
{
	if(__builtin_expect(result != 0, 0))
		throw std::system_error(result, CAAudioFileErrorCategory(), operation);
}

/// Throws a @c std::system_error in the @c CAExtAudioFileErrorCategory if @c result!=0
/// @note This is intended for results from the @c ExtAudioFile API
/// @param result An @c OSStatus result code
/// @param operation An optional string describing the operation producing @c result
/// @throw @c std::system_error in the @c CAExtAudioFileErrorCategory
inline void ThrowIfCAExtAudioFileError(OSStatus result, const char * const operation = nullptr)
{
	if(__builtin_expect(result != 0, 0))
		throw std::system_error(result, CAExtAudioFileErrorCategory(), operation);
}

#if 0
class FourCCString
{

public:

	FourCCString(OSStatus errorCode)
	{
		auto err = CFSwapInt32HostToBig(errorCode);
		std::memcpy(&mString[0] + 1, &err, 4);
		if(std::isprint(mString[1]) && std::isprint(mString[2]) && std::isprint(mString[3]) && std::isprint(mString[4])) {
			mString[0] = mString[5] = '\'';
			mString[6] = '\0';
		}
		else if(errorCode > -200000 && errorCode < 200000)
			snprintf(mString, sizeof(mString), "%d", static_cast<int>(errorCode));
		else
			snprintf(mString, sizeof(mString), "0x%x", static_cast<int>(errorCode));
	}

	operator const char * const () const
	{
		return mString;
	}

private:

	char mString [16];

};
#endif

} // namespace SFB
