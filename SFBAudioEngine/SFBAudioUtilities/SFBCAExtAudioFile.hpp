//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <AudioToolbox/ExtendedAudioFile.h>

#import "SFBCAException.hpp"
#import "SFBCABufferList.hpp"
#import "SFBCAChannelLayout.hpp"
#import "SFBCAStreamBasicDescription.hpp"

CF_ASSUME_NONNULL_BEGIN

namespace SFB {

/// A wrapper around @c ExtAudioFile
class CAExtAudioFile
{

private:

	struct free_deleter {
		template <typename T>
		void operator()(T * _Nonnull ptr) const {
			std::free(const_cast<std::remove_const_t<T> *>(ptr));
		}
	};

public:
	/// Creates a @c CAExtAudioFile
	inline CAExtAudioFile() noexcept
	: mExtAudioFile(nullptr)
	{}

	// This class is non-copyable
	CAExtAudioFile(const CAExtAudioFile& rhs) = delete;

	// This class is non-assignable
	CAExtAudioFile& operator=(const CAExtAudioFile& rhs) = delete;

	/// Destroys the @c CAExtAudioFile and release all associated resources.
	inline ~CAExtAudioFile()
	{
		if(mExtAudioFile)
			ExtAudioFileDispose(mExtAudioFile);
	}

	/// Move constructor
	CAExtAudioFile(CAExtAudioFile&& rhs) noexcept
	: mExtAudioFile(rhs.mExtAudioFile)
	{
		rhs.mExtAudioFile = nullptr;
	}

	/// Move assignment operator
	CAExtAudioFile& operator=(CAExtAudioFile&& rhs) noexcept
	{
		if(this != &rhs) {
			mExtAudioFile = rhs.mExtAudioFile;
			rhs.mExtAudioFile = nullptr;
		}
		return *this;
	}

	/// Returns @c true if this object's internal @c ExtAudioFileRef is not @c nullptr
	inline explicit operator bool() const noexcept
	{
		return mExtAudioFile != nullptr;
	}

	/// Returns @c true if this object's internal @c ExtAudioFileRef is @c nullptr
	inline bool operator!() const noexcept
	{
		return !operator bool();
	}

	/// Returns @c true if this object's internal @c ExtAudioFileRef is not @c nullptr
	inline bool IsValid() const noexcept
	{
		return operator bool();
	}

	/// Returns the file's internal @c ExtAudioFileRef
	inline operator ExtAudioFileRef const _Nullable () const noexcept
	{
		return mExtAudioFile;
	}

	/// Opens an audio file specified by a @c CFURLRef.
	///
	/// Allocates a new @c ExtAudioFileRef, for reading an existing audio file.
	/// @param inURL The audio file to read
	/// @throw @c std::system_error
	void OpenURL(CFURLRef inURL)
	{
		Close();
		auto result = ExtAudioFileOpenURL(inURL, &mExtAudioFile);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileOpenURL");
	}

	/// Wraps an @c AudioFileID in an @c ExtAudioFileRef.
	///
	/// Allocates a new ExtAudioFileRef which wraps an existing AudioFileID. The
	/// client is responsible for keeping the AudioFileID open until the
	/// ExtAudioFileRef is disposed. Disposing the ExtAudioFileRef will not close
	/// the AudioFileID when this Wrap API call is used, so the client is also
	/// responsible for closing the AudioFileID when finished with it.
	///
	/// @param inFileID The @c AudioFileID to wrap
	/// @param inForWriting @c true if the @c AudioFileID is a new file opened for writing
	/// @throw @c std::system_error
	void WrapAudioFileID(AudioFileID inFileID, bool inForWriting)
	{
		Close();
		auto result = ExtAudioFileWrapAudioFileID(inFileID, inForWriting, &mExtAudioFile);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileWrapAudioFileID");
	}

	/// Creates a new audio file.
	///
	/// If the file to be created is in an encoded format, it is permissible for the
	/// sample rate in @c inStreamDesc to be 0, since in all cases, the file's encoding
	/// @c AudioConverter may produce audio at a different sample rate than the source. The
	/// file will be created with the audio format actually produced by the encoder.
	///
	/// @param inURL The URL of the new audio file.
	/// @param inFileType The type of file to create. This is a constant from @c AudioToolbox/AudioFile.h, e.g.
	/// @c kAudioFileAIFFType. Note that this is not an HFSTypeCode.
	/// @param inStreamDesc The format of the audio data to be written to the file.
	/// @param inChannelLayout The channel layout of the audio data. If non-null, this must be consistent
	/// with the number of channels specified by @c inStreamDesc.
	/// @param inFlags The same flags as are used with @c AudioFileCreateWithURL
	/// Can use these to control whether an existing file is overwritten (or not).
	/// @throw @c std::system_error
	void CreateWithURL(CFURLRef inURL, AudioFileTypeID inFileType, const AudioStreamBasicDescription& inStreamDesc, const AudioChannelLayout * _Nullable const inChannelLayout, UInt32 inFlags)
	{
		Close();
		auto result = ExtAudioFileCreateWithURL(inURL, inFileType, &inStreamDesc, inChannelLayout, inFlags, &mExtAudioFile);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileCreateWithURL");
	}

	/// Closes the file and disposes of the internal @c ExtAudioFileRef
	/// @throw @c std::system_error
	void Close()
	{
		if(mExtAudioFile) {
			auto result = ExtAudioFileDispose(mExtAudioFile);
			ThrowIfCAExtAudioFileError(result, "ExtAudioFileDispose");
			mExtAudioFile = nullptr;
		}
	}

	/// Performs a synchronous sequential read.
	///
	/// If the file has a client data format, then the audio data from the file is
	/// translated from the file data format to the client format, via the
	/// @c ExtAudioFile's internal @c AudioConverter.
	///
	/// (Note that the use of sequential reads/writes means that an @c ExtAudioFile must
	/// not be read on multiple threads; clients wishing to do this should use the
	/// lower-level @c AudioFile API set).
	///
	/// @param ioNumberFrames On entry, ioNumberFrames is the number of frames to be read from the file.
	/// On exit, it is the number of frames actually read. A number of factors may
	/// cause a fewer number of frames to be read, including the supplied buffers
	/// not being large enough, and internal optimizations. If 0 frames are
	/// returned, however, this indicates that end-of-file was reached.
	/// @param ioData Buffer(s) into which the audio data is read.
	/// @throw @c std::system_error
	void Read(UInt32& ioNumberFrames, AudioBufferList *ioData)
	{
		auto result = ExtAudioFileRead(mExtAudioFile, &ioNumberFrames, ioData);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileRead");
	}

	/// Performs a synchronous sequential read.
	/// @param buffer Buffer into which the audio data is read.
	/// @throw @c std::system_error
	void Read(SFB::CABufferList& buffer)
	{
		buffer.Reset();
		UInt32 frameCount = buffer.FrameCapacity();
		Read(frameCount, buffer);
		buffer.SetFrameLength(frameCount);
	}

	/// Performs a synchronous sequential write.
	///
	///	If the file has a client data format, then the audio data in ioData is
	/// translated from the client format to the file data format, via the
	/// @c ExtAudioFile's internal @c AudioConverter.
	///
	/// @param inNumberFrames The number of frames to write
	/// @param ioData The buffer(s) from which audio data is written to the file
	/// @throw @c std::system_error
	OSStatus Write(UInt32 inNumberFrames, const AudioBufferList *ioData)
	{
		auto result = ExtAudioFileWrite(mExtAudioFile, inNumberFrames, ioData);
		switch(result) {
			case noErr:
				break;
#if TARGET_OS_IPHONE
			case kExtAudioFileError_CodecUnavailableInputConsumed:
			case kExtAudioFileError_CodecUnavailableInputNotConsumed:
				break;
#endif
			default:
				ThrowIfCAExtAudioFileError(result, "ExtAudioFileWrite");
				break;
		}
		return result;
	}

	/// Performs an asynchronous sequential write
	///
	/// Writes the provided buffer list to an internal ring buffer and notifies an
	/// internal thread to perform the write at a later time. The first time this is
	/// called, allocations may be performed. You can call this with 0 frames and null
	/// buffer in a non-time-critical context to initialize the asynchronous mechanism.
	/// Once initialized, subsequent calls are very efficient and do not take locks;
	/// thus this may be used to write to a file from a realtime thread.
	///
	/// The client must not mix synchronous and asynchronous writes to the same file.
	///
	/// Pending writes are not guaranteed to be flushed to disk until
	/// @c ExtAudioFileDispose is called.
	///
	/// N.B. Errors may occur after this call has returned. Such errors may be returned
	/// from subsequent calls to this function.
	///
	/// @param inNumberFrames The number of frames to write
	/// @param ioData The buffer(s) from which audio data is written to the file
	/// @throw @c std::system_error
	void WriteAsync(UInt32 inNumberFrames, const AudioBufferList * _Nullable ioData)
	{
		auto result = ExtAudioFileWriteAsync(mExtAudioFile, inNumberFrames, ioData);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileOpenURL");
	}

	/// Seeks to a specific frame position.
	///
	/// Sets the file's read position to the specified sample frame number. The next call
	/// to @c ExtAudioFileRead will return samples from precisely this location, even if it
	/// is located in the middle of a packet.
	///
	/// This function's behavior with files open for writing is currently undefined.
	///
	/// @param inFrameOffset The desired seek position, in sample frames, relative to the beginning of
	/// the file. This is specified in the sample rate and frame count of the file's format
	/// (not the client format)
	/// @throw @c std::system_error
	void Seek(SInt64 inFrameOffset)
	{
		auto result = ExtAudioFileSeek(mExtAudioFile, inFrameOffset);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileSeek");
	}

	/// Returns the file's read/write position.
	/// @return The file's current read/write position in sample frames. This is specified in the
	/// sample rate and frame count of the file's format (not the client format)
	/// @throw @c std::system_error
	SInt64 Tell() const
	{
		SInt64 pos;
		auto result = ExtAudioFileTell(mExtAudioFile, &pos);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileTell");
		return pos;
	}

	/// Gets information about a property
	/// @param inPropertyID The property being queried.
	/// @param outWritable If non-null, on exit, this indicates whether the property value is settable.
	/// @return The size of the property's value.
	/// @throw @c std::system_error
	UInt32 GetPropertyInfo(ExtAudioFilePropertyID inPropertyID, Boolean * _Nullable outWritable) const
	{
		UInt32 size;
		auto result = ExtAudioFileGetPropertyInfo(mExtAudioFile, inPropertyID, &size, outWritable);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileGetPropertyInfo");
		return size;
	}

	/// Gets a property value
	/// @param inPropertyID The property being fetched.
	/// @param ioPropertyDataSize On entry, the size (in bytes) of the memory pointed to by @c outPropertyData.
	/// On exit, the actual size of the property data returned.
	/// @param outPropertyData The value of the property is copied to the memory this points to.
	/// @throw @c std::system_error
	void GetProperty(ExtAudioFilePropertyID inPropertyID, UInt32& ioPropertyDataSize, void *outPropertyData) const
	{
		auto result = ExtAudioFileGetProperty(mExtAudioFile, inPropertyID, &ioPropertyDataSize, outPropertyData);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileGetProperty");
	}

	/// Sets a property value
	/// @param inPropertyID The property being set.
	/// @param inPropertyDataSize The size of the property data, in bytes.
	/// @param inPropertyData Points to the property's new value.
	/// @throw @c std::system_error
	void SetProperty(ExtAudioFilePropertyID inPropertyID, UInt32 inPropertyDataSize, const void *inPropertyData)
	{
		auto result = ExtAudioFileSetProperty(mExtAudioFile, inPropertyID, inPropertyDataSize, inPropertyData);
		ThrowIfCAExtAudioFileError(result, "ExtAudioFileSetProperty");
	}

	/// Returns the file's channel layout (@c kExtAudioFileProperty_FileChannelLayout)
	/// @throw @c std::system_error
	/// @throw @c std::bad_alloc
	CAChannelLayout FileChannelLayout() const
	{
		auto size = GetPropertyInfo(kExtAudioFileProperty_FileChannelLayout, nullptr);
		std::unique_ptr<AudioChannelLayout, free_deleter> layout{static_cast<AudioChannelLayout *>(std::malloc(size))};
		if(!layout)
			throw std::bad_alloc();
		GetProperty(kExtAudioFileProperty_FileChannelLayout, size, layout.get());
		return layout.get();
	}

	/// Sets the file's channel layout (@c kExtAudioFileProperty_FileChannelLayout)
	/// @throw @c std::system_error
	void SetFileChannelLayout(const CAChannelLayout& fileChannelLayout)
	{
		SetProperty(kExtAudioFileProperty_FileChannelLayout, static_cast<UInt32>(fileChannelLayout.Size()), fileChannelLayout);
	}

	/// Returns the file's data format (@c kExtAudioFileProperty_FileDataFormat)
	/// @throw @c std::system_error
	CAStreamBasicDescription FileDataFormat() const
	{
		CAStreamBasicDescription fileDataFormat;
		UInt32 size = sizeof(fileDataFormat);
		GetProperty(kExtAudioFileProperty_FileDataFormat, size, &fileDataFormat);
		return fileDataFormat;
	}

	/// Returns the client data format (@c kExtAudioFileProperty_ClientDataFormat)
	/// @throw @c std::system_error
	CAStreamBasicDescription ClientDataFormat() const
	{
		CAStreamBasicDescription clientDataFormat;
		UInt32 size = sizeof(clientDataFormat);
		GetProperty(kExtAudioFileProperty_ClientDataFormat, size, &clientDataFormat);
		return clientDataFormat;
	}

	/// Sets the client data format (@c kExtAudioFileProperty_ClientDataFormat)
	/// @throw @c std::system_error
	void SetClientDataFormat(const CAStreamBasicDescription& clientDataFormat, const CAChannelLayout * const _Nullable clientChannelLayout = nullptr, UInt32 codecManufacturer = 0)
	{
		if(codecManufacturer)
			SetProperty(kExtAudioFileProperty_CodecManufacturer, sizeof(codecManufacturer), &codecManufacturer);
		SetProperty(kExtAudioFileProperty_ClientDataFormat, sizeof(clientDataFormat), &clientDataFormat);
		if(clientChannelLayout)
			SetClientChannelLayout(*clientChannelLayout);
	}

	/// Returns the client channel layout (@c kExtAudioFileProperty_ClientChannelLayout)
	/// @throw @c std::system_error
	/// @throw @c std::bad_alloc
	CAChannelLayout ClientChannelLayout() const
	{
		auto size = GetPropertyInfo(kExtAudioFileProperty_ClientChannelLayout, nullptr);
		std::unique_ptr<AudioChannelLayout, free_deleter> layout{static_cast<AudioChannelLayout *>(std::malloc(size))};
		if(!layout)
			throw std::bad_alloc();
		GetProperty(kExtAudioFileProperty_ClientChannelLayout, size, layout.get());
		return layout.get();
	}

	/// Sets the client channel layout (@c kExtAudioFileProperty_ClientChannelLayout)
	/// @throw @c std::system_error
	void SetClientChannelLayout(const CAChannelLayout& clientChannelLayout)
	{
		SetProperty(kExtAudioFileProperty_ClientChannelLayout, static_cast<UInt32>(clientChannelLayout.Size()), clientChannelLayout);
	}

	/// Returns the internal @c AudioConverter (@c kExtAudioFileProperty_AudioConverter)
	/// @throw @c std::system_error
	AudioConverterRef _Nullable Converter() const
	{
		UInt32 size = sizeof(AudioConverterRef);
		AudioConverterRef converter = nullptr;
		GetProperty(kExtAudioFileProperty_AudioConverter, size, &converter);
		return converter;
	}

	/// Returns @c true if the @c ExtAudioFile has an internal @c AudioConverter
	bool HasConverter() const
	{
		return Converter() != nullptr;
	}

	/// Sets a property on the internal @c AudioConverter
	void SetConverterProperty(AudioConverterPropertyID inPropertyID, UInt32 inPropertyDataSize, const void *inPropertyData)
	{
		auto result = AudioConverterSetProperty(Converter(), inPropertyID, inPropertyDataSize, inPropertyData);
		ThrowIfCAAudioConverterError(result, "AudioConverterSetProperty");
		CFPropertyListRef config = nullptr;
		SetProperty(kExtAudioFileProperty_ConverterConfig, sizeof(config), &config);
	}

	/// Returns the length of the file in audio frames (@c kExtAudioFileProperty_FileLengthFrames)
	/// @throw @c std::system_error
	SInt64 FrameLength() const
	{
		SInt64 frameLength;
		UInt32 size = sizeof(frameLength);
		GetProperty(kExtAudioFileProperty_FileLengthFrames, size, &frameLength);
		return frameLength;
	}

private:

	/// The underlying @c ExtAudioFile object
	ExtAudioFileRef _Nullable mExtAudioFile;

};

} // namespace SFB

CF_ASSUME_NONNULL_END
