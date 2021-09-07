//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <vector>

#import <AudioToolbox/AudioFile.h>

#import "SFBCAException.hpp"
#import "SFBCAStreamBasicDescription.hpp"

CF_ASSUME_NONNULL_BEGIN

namespace SFB {

/// A wrapper around @c AudioFile
class CAAudioFile
{

private:

	struct free_deleter {
		template <typename T>
		void operator()(T * _Nonnull ptr) const {
			std::free(const_cast<std::remove_const_t<T> *>(ptr));
		}
	};

public:
	/// Creates a @c CAAudioFile
	inline CAAudioFile() noexcept
	: mAudioFileID(nullptr)
	{}

	// This class is non-copyable
	CAAudioFile(const CAAudioFile& rhs) = delete;

	// This class is non-assignable
	CAAudioFile& operator=(const CAAudioFile& rhs) = delete;

	/// Destroys the @c CAAudioFile and release all associated resources.
	inline ~CAAudioFile()
	{
		if(mAudioFileID)
			AudioFileClose(mAudioFileID);
	}

	/// Move constructor
	CAAudioFile(CAAudioFile&& rhs) noexcept
	: mAudioFileID(rhs.mAudioFileID)
	{
		rhs.mAudioFileID = nullptr;
	}

	/// Move assignment operator
	CAAudioFile& operator=(CAAudioFile&& rhs) noexcept
	{
		if(this != &rhs) {
			mAudioFileID = rhs.mAudioFileID;
			rhs.mAudioFileID = nullptr;
		}
		return *this;
	}

	/// Returns @c true if this object's internal @c AudioFileID is not @c nullptr
	inline explicit operator bool() const noexcept
	{
		return mAudioFileID != nullptr;
	}

	/// Returns @c true if this object's internal @c AudioFileID is @c nullptr
	inline bool operator!() const noexcept
	{
		return !operator bool();
	}

	/// Returns @c true if this object's internal @c AudioFileID is not @c nullptr
	inline bool IsValid() const noexcept
	{
		return operator bool();
	}

	/// Returns the file's internal @c AudioFileID
	inline operator AudioFileID const _Nullable () const noexcept
	{
		return mAudioFileID;
	}

	/// Opens an existing audio file.
	/// @throw @c std::system_error
	void OpenURL(CFURLRef inURL, AudioFilePermissions inPermissions, AudioFileTypeID inFileTypeHint)
	{
		Close();
		auto result = AudioFileOpenURL(inURL, inPermissions, inFileTypeHint, &mAudioFileID);
		ThrowIfCAAudioFileError(result, "AudioFileOpenURL");
	}

	/// Creates a new audio file (or initialises an existing file).
	/// @throw @c std::system_error
	void CreateWithURL(CFURLRef inURL, AudioFileTypeID inFileType, const AudioStreamBasicDescription& inFormat, AudioFileFlags inFlags)
	{
		Close();
		auto result = AudioFileCreateWithURL(inURL, inFileType, &inFormat, inFlags, &mAudioFileID);
		ThrowIfCAAudioFileError(result, "AudioFileCreateWithURL");
	}

	/// Wipes clean an existing file. You provide callbacks that the AudioFile API will use to get the data.
	/// @throw @c std::system_error
	void InitializeWithCallbacks(void *inClientData, AudioFile_ReadProc inReadFunc, AudioFile_WriteProc inWriteFunc, AudioFile_GetSizeProc inGetSizeFunc, AudioFile_SetSizeProc inSetSizeFunc, AudioFileTypeID inFileType, const AudioStreamBasicDescription& inFormat, AudioFileFlags inFlags)
	{
		Close();
		auto result = AudioFileInitializeWithCallbacks(inClientData, inReadFunc, inWriteFunc, inGetSizeFunc, inSetSizeFunc, inFileType, &inFormat, inFlags, &mAudioFileID);
		ThrowIfCAAudioFileError(result, "AudioFileInitializeWithCallbacks");
	}

	/// Opens an existing file. You provide callbacks that the AudioFile API will use to get the data.
	/// @throw @c std::system_error
	void OpenWithCallbacks(void *inClientData, AudioFile_ReadProc inReadFunc, AudioFile_WriteProc _Nullable inWriteFunc, AudioFile_GetSizeProc inGetSizeFunc, AudioFile_SetSizeProc _Nullable inSetSizeFunc, AudioFileTypeID inFileTypeHint)
	{
		Close();
		auto result = AudioFileOpenWithCallbacks(inClientData, inReadFunc, inWriteFunc, inGetSizeFunc, inSetSizeFunc, inFileTypeHint, &mAudioFileID);
		ThrowIfCAAudioFileError(result, "AudioFileOpenWithCallbacks");
	}

	/// Close an existing audio file.
	/// @throw @c std::system_error
	void Close()
	{
		if(mAudioFileID) {
			auto result = AudioFileClose(mAudioFileID);
			ThrowIfCAAudioFileError(result, "AudioFileClose");
			mAudioFileID = nullptr;
		}
	}

	/// Moves the audio data to the end of the file and other internal optimizations of the file structure.
	/// @throw @c std::system_error
	void Optimize()
	{
		auto result = AudioFileOptimize(mAudioFileID);
		ThrowIfCAAudioFileError(result, "AudioFileOptimize");
	}

	/// Reads bytes of audio data from the audio file.
	/// @throw @c std::system_error
	OSStatus ReadBytes(bool inUseCache, SInt64 inStartingByte, UInt32& ioNumBytes, void *outBuffer)
	{
		auto result = AudioFileReadBytes(mAudioFileID, inUseCache, inStartingByte, &ioNumBytes, outBuffer);
		switch(result) {
			case noErr:
			case kAudioFileEndOfFileError:
				break;
			default:
				ThrowIfCAAudioFileError(result, "AudioFileReadBytes");
				break;
		}
		return result;
	}

	/// Writes bytes of audio data to the audio file.
	/// @throw @c std::system_error
	void WriteBytes(bool inUseCache, SInt64 inStartingByte, UInt32& ioNumBytes, const void *inBuffer)
	{
		auto result = AudioFileWriteBytes(mAudioFileID, inUseCache, inStartingByte, &ioNumBytes, inBuffer);
		ThrowIfCAAudioFileError(result, "AudioFileWriteBytes");
	}

	/// Reads packets of audio data from the audio file.
	/// @throw @c std::system_error
	OSStatus ReadPacketData(bool inUseCache, UInt32& ioNumBytes, AudioStreamPacketDescription * _Nullable outPacketDescriptions, SInt64 inStartingPacket, UInt32& ioNumPackets, void * _Nullable outBuffer)
	{
		auto result = AudioFileReadPacketData(mAudioFileID, inUseCache, &ioNumBytes, outPacketDescriptions, inStartingPacket, &ioNumPackets, outBuffer);
		switch(result) {
			case noErr:
			case kAudioFileEndOfFileError:
				break;
			default:
				ThrowIfCAAudioFileError(result, "AudioFileReadPacketData");
				break;
		}
		return result;
	}

	/// Writes packets of audio data to the audio file.
	/// @throw @c std::system_error
	void WritePackets(bool inUseCache, UInt32 inNumBytes, const AudioStreamPacketDescription * _Nullable inPacketDescriptions, SInt64 inStartingPacket, UInt32& ioNumPackets, const void *inBuffer)
	{
		auto result = AudioFileWritePackets(mAudioFileID, inUseCache, inNumBytes, inPacketDescriptions, inStartingPacket, &ioNumPackets, inBuffer);
		ThrowIfCAAudioFileError(result, "AudioFileWritePackets");
	}

	/// Gets the size of user data in a file.
	/// @throw @c std::system_error
	UInt32 GetUserDataSize(UInt32 inUserDataID, UInt32 inIndex)
	{
		UInt32 size;
		auto result = AudioFileGetUserDataSize(mAudioFileID, inUserDataID, inIndex, &size);
		ThrowIfCAAudioFileError(result, "AudioFileGetUserDataSize");
		return size;
	}

	/// Gets the data of a chunk in a file.
	/// @throw @c std::system_error
	void GetUserData(UInt32 inUserDataID, UInt32 inIndex, UInt32& ioUserDataSize, void *outUserData)
	{
		auto result = AudioFileGetUserData(mAudioFileID, inUserDataID, inIndex, &ioUserDataSize, outUserData);
		ThrowIfCAAudioFileError(result, "AudioFileGetUserData");
	}

	/// Sets the data of a chunk in a file.
	/// @throw @c std::system_error
	void SetUserData(UInt32 inUserDataID, UInt32 inIndex, UInt32 inUserDataSize, const void *inUserData)
	{
		auto result = AudioFileSetUserData(mAudioFileID, inUserDataID, inIndex, inUserDataSize, inUserData);
		ThrowIfCAAudioFileError(result, "AudioFileGetUserData");
	}

	/// Removes a user chunk in a file.
	/// @throw @c std::system_error
	void RemoveUserData(UInt32 inUserDataID, UInt32 inIndex)
	{
		auto result = AudioFileRemoveUserData(mAudioFileID, inUserDataID, inIndex);
		ThrowIfCAAudioFileError(result, "AudioFileRemoveUserData");
	}

	/// Gets information about the size of a property of an AudioFile and whether it can be set.
	/// @throw @c std::system_error
	UInt32 GetPropertyInfo(AudioFilePropertyID inPropertyID, UInt32 * _Nullable isWritable) const
	{
		UInt32 size;
		auto result = AudioFileGetPropertyInfo(mAudioFileID, inPropertyID, &size, isWritable);
		ThrowIfCAAudioFileError(result, "AudioFileGetPropertyInfo");
		return size;
	}

	/// Copies the value for a property of an AudioFile into a buffer.
	/// @throw @c std::system_error
	void GetProperty(AudioFilePropertyID inPropertyID, UInt32& ioDataSize, void *outPropertyData) const
	{
		auto result = AudioFileGetProperty(mAudioFileID, inPropertyID, &ioDataSize, outPropertyData);
		ThrowIfCAAudioFileError(result, "AudioFileGetProperty");
	}

	/// Sets the value for a property of an AudioFile.
	/// @throw @c std::system_error
	void SetProperty(AudioFilePropertyID inPropertyID, UInt32 inDataSize, const void *inPropertyData)
	{
		auto result = AudioFileSetProperty(mAudioFileID, inPropertyID, inDataSize, inPropertyData);
		ThrowIfCAAudioFileError(result, "AudioFileSetProperty");
	}

	/// Returns the file's format (@c kAudioFilePropertyFileFormat)
	/// @throw @c std::system_error
	AudioFileTypeID FileFormat() const
	{
		AudioFileTypeID fileFormat;
		UInt32 size = sizeof(fileFormat);
		GetProperty(kAudioFilePropertyFileFormat, size, &fileFormat);
		return fileFormat;
	}

	/// Returns the file's data format (@c kAudioFilePropertyDataFormat)
	/// @throw @c std::system_error
	CAStreamBasicDescription FileDataFormat() const
	{
		CAStreamBasicDescription fileDataFormat;
		UInt32 size = sizeof(fileDataFormat);
		GetProperty(kAudioFilePropertyDataFormat, size, &fileDataFormat);
		return fileDataFormat;
	}

#pragma mark Global Properties

	/// Gets the size of a global audio file property.
	/// @throw @c std::system_error
	static UInt32 GetGlobalInfoSize(AudioFilePropertyID inPropertyID, UInt32 inSpecifierSize, void * _Nullable inSpecifier)
	{
		UInt32 size;
		auto result = AudioFileGetGlobalInfoSize(inPropertyID, inSpecifierSize, inSpecifier, &size);
		ThrowIfCAAudioFileError(result, "AudioFileGetGlobalInfoSize");
		return size;
	}

	/// Copies the value of a global property into a buffer.
	/// @throw @c std::system_error
	static void GetGlobalInfo(AudioFilePropertyID inPropertyID, UInt32 inSpecifierSize, void * _Nullable inSpecifier, UInt32& ioDataSize, void *outPropertyData)
	{
		auto result = AudioFileGetGlobalInfo(inPropertyID, inSpecifierSize, inSpecifier, &ioDataSize, outPropertyData);
		ThrowIfCAAudioFileError(result, "AudioFileGetGlobalInfo");
	}


	/// Returns an array of @c AudioFileTypeID containing the file types (i.e. AIFF, WAVE, etc) that can be opened for reading.
	/// @throw @c std::system_error
	static std::vector<AudioFileTypeID> ReadableTypes()
	{
		auto size = GetGlobalInfoSize(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr);
		auto count = size / sizeof(AudioFileTypeID);
		auto types = std::vector<AudioFileTypeID>(count);
		GetGlobalInfo(kAudioFileGlobalInfo_ReadableTypes, 0, nullptr, size, &types[0]);
		return types;
	}

	/// Returns an array of @c AudioFileTypeID containing the file types (i.e. AIFF, WAVE, etc) that can be opened for writing.
	/// @throw @c std::system_error
	static std::vector<AudioFileTypeID> WritableTypes()
	{
		auto size = GetGlobalInfoSize(kAudioFileGlobalInfo_WritableTypes, 0, nullptr);
		auto count = size / sizeof(AudioFileTypeID);
		auto types = std::vector<AudioFileTypeID>(count);
		GetGlobalInfo(kAudioFileGlobalInfo_WritableTypes, 0, nullptr, size, &types[0]);
		return types;
	}

	/// Returns the name of @c type
	/// @throw @c std::system_error
	static CFString FileTypeName(AudioFileTypeID type)
	{
		CFStringRef s;
		UInt32 size = sizeof(s);
		GetGlobalInfo(kAudioFileGlobalInfo_FileTypeName, sizeof(type), &type, size, &s);
		return CFString(s);
	}

	/// Returns an array of supported formats for the @c fileType and @c formatID combination
	/// @throw @c std::system_error
	static std::vector<CAStreamBasicDescription> AvailableStreamDescriptions(AudioFileTypeID fileType, AudioFormatID formatID)
	{
		AudioFileTypeAndFormatID spec{ fileType, formatID };
		auto size = GetGlobalInfoSize(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, sizeof(spec), &spec);
		auto count = size / sizeof(AudioStreamBasicDescription);
		auto streamDescriptions = std::vector<CAStreamBasicDescription>(count);
		GetGlobalInfo(kAudioFileGlobalInfo_AvailableStreamDescriptionsForFormat, sizeof(spec), &spec, size, &streamDescriptions[0]);
		return streamDescriptions;

	}

	/// Returns an array of format IDs that can be read.
	/// @throw @c std::system_error
	static std::vector<AudioFormatID> AvailableFormatIDs(AudioFileTypeID type)
	{
		auto size = GetGlobalInfoSize(kAudioFileGlobalInfo_AvailableFormatIDs, sizeof(type), &type);
		auto count = size / sizeof(AudioFormatID);
		auto formatIDs = std::vector<AudioFormatID>(count);
		GetGlobalInfo(kAudioFileGlobalInfo_AvailableFormatIDs, sizeof(type), &type, size, &formatIDs[0]);
		return formatIDs;
	}


	/// Returns an array of recognized file extensions
	/// @throw @c std::system_error
	static CFArray AllExtensions()
	{
		CFArrayRef a;
		UInt32 size = sizeof(a);
		GetGlobalInfo(kAudioFileGlobalInfo_AllExtensions, 0, nullptr, size, &a);
		return CFArray(a);
	}

	/// Returns an array of recognized UTIs
	/// @throw @c std::system_error
	static CFArray AllUTIs()
	{
		CFArrayRef a;
		UInt32 size = sizeof(a);
		GetGlobalInfo(kAudioFileGlobalInfo_AllUTIs, 0, nullptr, size, &a);
		return CFArray(a);
	}

	/// Returns an array of recognized MIME types
	/// @throw @c std::system_error
	static CFArray AllMIMETypes()
	{
		CFArrayRef a;
		UInt32 size = sizeof(a);
		GetGlobalInfo(kAudioFileGlobalInfo_AllMIMETypes, 0, nullptr, size, &a);
		return CFArray(a);
	}


	/// Returns an array of file extensions for @c type
	/// @throw @c std::system_error
	static CFArray ExtensionsForType(AudioFileTypeID type)
	{
		CFArrayRef a;
		UInt32 size = sizeof(a);
		GetGlobalInfo(kAudioFileGlobalInfo_ExtensionsForType, sizeof(type), &type, size, &a);
		return CFArray(a);
	}

	/// Returns an array of UTIs for @c type
	/// @throw @c std::system_error
	static CFArray UTIsForType(AudioFileTypeID type)
	{
		CFArrayRef a;
		UInt32 size = sizeof(a);
		GetGlobalInfo(kAudioFileGlobalInfo_UTIsForType, sizeof(type), &type, size, &a);
		return CFArray(a);
	}

	/// Returns an array of MIME types for @c type
	/// @throw @c std::system_error
	static CFArray MIMETypesForType(AudioFileTypeID type)
	{
		CFArrayRef mimeTypes;
		UInt32 size = sizeof(mimeTypes);
		GetGlobalInfo(kAudioFileGlobalInfo_MIMETypesForType, sizeof(type), &type, size, &mimeTypes);
		return CFArray(mimeTypes);
	}


	/// Returns an array of @c AudioFileTypeID that support @c mimeType
	/// @throw @c std::system_error
	static std::vector<AudioFileTypeID> TypesForMIMEType(CFStringRef mimeType)
	{
		auto size = GetGlobalInfoSize(kAudioFileGlobalInfo_TypesForMIMEType, sizeof(mimeType), const_cast<void *>(reinterpret_cast<const void *>(mimeType)));
		auto count = size / sizeof(AudioFileTypeID);
		auto types = std::vector<AudioFileTypeID>(count);
		GetGlobalInfo(kAudioFileGlobalInfo_TypesForMIMEType, sizeof(mimeType), const_cast<void *>(reinterpret_cast<const void *>(mimeType)), size, &types[0]);
		return types;
	}

	/// Returns an array of @c AudioFileTypeID that support @c uti
	/// @throw @c std::system_error
	static std::vector<AudioFileTypeID> TypesForUTI(CFStringRef uti)
	{
		auto size = GetGlobalInfoSize(kAudioFileGlobalInfo_TypesForUTI, sizeof(uti), const_cast<void *>(reinterpret_cast<const void *>(uti)));
		auto count = size / sizeof(AudioFileTypeID);
		auto types = std::vector<AudioFileTypeID>(count);
		GetGlobalInfo(kAudioFileGlobalInfo_TypesForUTI, sizeof(uti), const_cast<void *>(reinterpret_cast<const void *>(uti)), size, &types[0]);
		return types;
	}

	/// Returns an array of @c AudioFileTypeID that support @c extension
	/// @throw @c std::system_error
	static std::vector<AudioFileTypeID> TypesForExtension(CFStringRef extension)
	{
		auto size = GetGlobalInfoSize(kAudioFileGlobalInfo_TypesForExtension, sizeof(extension), const_cast<void *>(reinterpret_cast<const void *>(extension)));
		auto count = size / sizeof(AudioFileTypeID);
		auto types = std::vector<AudioFileTypeID>(count);
		GetGlobalInfo(kAudioFileGlobalInfo_TypesForExtension, sizeof(extension), const_cast<void *>(reinterpret_cast<const void *>(extension)), size, &types[0]);
		return types;
	}

private:

	/// The underlying @c AudioFile object
	AudioFileID _Nullable mAudioFileID;

};

} // namespace SFB

CF_ASSUME_NONNULL_END
