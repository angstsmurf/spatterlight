//
// Copyright (c) 2013 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <CoreAudioTypes/CoreAudioTypes.h>

#import "SFBCAStreamBasicDescription.hpp"

namespace SFB {

/// Allocates and returns a new @c AudioBufferList in a single allocation
/// @note The allocation is performed using @c std::malloc and should be deallocated using @c std::free
/// @param format The format of the audio the @c AudioBufferList will hold
/// @param frameCapacity The desired buffer capacity in audio frames
/// @return A newly-allocated @c AudioBufferList or @c nullptr
AudioBufferList * _Nullable AllocateAudioBufferList(const CAStreamBasicDescription& format, UInt32 frameCapacity) noexcept;

/// A class wrapping a Core Audio @c AudioBufferList with a specific format, frame capacity, and frame length
class CABufferList
{
	
public:
	
#pragma mark Creation and Destruction

	/// Creates an empty @c CABufferList
	/// @note @c Allocate() must be called before the object may be used.
	CABufferList() noexcept;

	// This class is non-copyable
	CABufferList(const CABufferList& rhs) = delete;

	// This class is non-assignable
	CABufferList& operator=(const CABufferList& rhs) = delete;

	/// Destroys the @c CABufferList and release all associated resources.
	~CABufferList();

	/// Creates a new @c CABufferList
	CABufferList(CABufferList&& rhs) noexcept;

	/// Replaces the buffer
	CABufferList& operator=(CABufferList&& rhs) noexcept;


	/// Creates a new @c CABufferList
	/// @param format The format of the audio the @c CABufferList will hold
	/// @param frameCapacity The desired buffer capacity in audio frames
	/// @throws @c std::invalid_argument if @c format.mBytesPerFrame==0
	/// @throws @c std::bad_alloc
	CABufferList(const CAStreamBasicDescription& format, UInt32 frameCapacity);

#pragma mark Buffer management

	/// Allocates space for audio
	/// @param format The format of the audio
	/// @param frameCapacity The desired capacity in audio frames
	/// @return @c true on sucess, @c false otherwise
	bool Allocate(const CAStreamBasicDescription& format, UInt32 frameCapacity) noexcept;

	/// Deallocates the memory associated with this @c CABufferList
	void Deallocate() noexcept;


	/// Resets the @c CABufferList to the default state in preparation for reading
	///
	/// This is equivalent to @c SetFrameLength(FrameCapacity())
	inline bool Reset() noexcept
	{
		return SetFrameLength(mFrameCapacity);
	}

	/// Clears the @c CABufferList
	///
	/// This is equivalent to @c SetFrameLength(0)
	/// @return @c true on sucess, @c false otherwise
	inline bool Clear() noexcept
	{
		return SetFrameLength(0);
	}


	/// Returns the length in audio frames of the data in this @c CABufferList
	inline UInt32 FrameLength() const noexcept
	{
		return mFrameLength;
	}

	/// Set the length in audio frames of the data in this @c CABufferList
	/// @param frameLength The number of valid audio frames
	/// @return @c true on sucess, @c false otherwise
	bool SetFrameLength(UInt32 frameLength) noexcept;

	/// Infers and updates the length in audio frames using the @c mDataByteSize of the internal @c AudioBufferList
	/// @return @c true on sucess, @c false otherwise
	/// @throws @c std::logic_error if the @c mDataByteSize values are inconsistent
	bool InferFrameLengthFromABL();

	inline bool IsEmpty() const noexcept
	{
		return mFrameLength == 0;
	}

	inline bool IsFull() const noexcept
	{
		return mFrameLength == mFrameCapacity;
	}

	/// Returns the audio frame capacity of this @c CABufferList
	inline UInt32 FrameCapacity() const noexcept
	{
		return mFrameCapacity;
	}

	/// Returns the format of this @c CABufferList
	inline const CAStreamBasicDescription& Format() const noexcept
	{
		return mFormat;
	}

#pragma mark Buffer utilities

	/// Prepends the contents of @c buffer
	/// @note The format of @c buffer must match the format of this @c CABufferList
	/// @param buffer A buffer of audio data
	/// @return The number of frames prepended
	inline UInt32 PrependContentsOfBuffer(const CABufferList& buffer) noexcept
	{
		return InsertFromBuffer(buffer, 0, buffer.mFrameLength, 0);
	}

	/// Prepends frames from @c buffer starting at @c readOffset
	/// @note The format of @c buffer must match the format of this @c CABufferList
	/// @param buffer A buffer of audio data
	/// @param readOffset The desired starting offset in @c buffer
	/// @return The number of frames prepended
	inline UInt32 PrependFromBuffer(const CABufferList& buffer, UInt32 readOffset) noexcept
	{
		if(readOffset > buffer.mFrameLength)
			return 0;
		return InsertFromBuffer(buffer, readOffset, (buffer.mFrameLength - readOffset), 0);
	}

	/// Prepends at most @c frameLength frames from @c buffer starting at @c readOffset
	/// @note The format of @c buffer must match the format of this @c CABufferList
	/// @param buffer A buffer of audio data
	/// @param readOffset The desired starting offset in @c buffer
	/// @param frameLength The desired number of frames
	/// @return The number of frames prepended
	inline UInt32 PrependFromBuffer(const CABufferList& buffer, UInt32 readOffset, UInt32 frameLength) noexcept
	{
		return InsertFromBuffer(buffer, readOffset, frameLength, 0);
	}

	/// Appends the contents of @c buffer
	/// @note The format of @c buffer must match the format of this @c CABufferList
	/// @param buffer A buffer of audio data
	/// @return The number of frames appended
	inline UInt32 AppendContentsOfBuffer(const CABufferList& buffer) noexcept
	{
		return InsertFromBuffer(buffer, 0, buffer.mFrameLength, mFrameLength);
	}

	/// Appends frames from @c buffer starting at @c readOffset
	/// @note The format of @c buffer must match the format of this @c CABufferList
	/// @param buffer A buffer of audio data
	/// @param readOffset The desired starting offset in @c buffer
	/// @return The number of frames appended
	inline UInt32 AppendFromBuffer(const CABufferList& buffer, UInt32 readOffset) noexcept
	{
		if(readOffset > buffer.mFrameLength)
			return 0;
		return InsertFromBuffer(buffer, readOffset, (buffer.mFrameLength - readOffset), mFrameLength);
	}

	/// Appends at most @c frameLength frames from @c buffer starting at @c readOffset
	/// @note The format of @c buffer must match the format of this @c CABufferList
	/// @param buffer A buffer of audio data
	/// @param readOffset The desired starting offset in @c buffer
	/// @param frameLength The desired number of frames
	/// @return The number of frames appended
	inline UInt32 AppendFromBuffer(const CABufferList& buffer, UInt32 readOffset, UInt32 frameLength) noexcept
	{
		return InsertFromBuffer(buffer, readOffset, frameLength, mFrameLength);
	}

	/// Inserts the contents of @c buffer in this @c CABufferList starting at @c writeOffset
	/// @note The format of @c buffer must match the format of this @c CABufferList
	/// @param buffer A buffer of audio data
	/// @param writeOffset The desired starting offset in this @c CABufferList
	/// @return The number of frames inserted
	inline UInt32 InsertContentsOfBuffer(const CABufferList& buffer, UInt32 writeOffset) noexcept
	{
		return InsertFromBuffer(buffer, 0, buffer.mFrameLength, writeOffset);
	}

	/// Inserts at most @c readLength frames from @c buffer starting at @c readOffset starting at @c writeOffset
	/// @note The format of @c buffer must match the format of this @c CABufferList
	/// @param buffer A buffer of audio data
	/// @param readOffset The desired starting offset in @c buffer
	/// @param frameLength The desired number of frames
	/// @param writeOffset The desired starting offset in this @c CABufferList
	/// @return The number of frames inserted
	UInt32 InsertFromBuffer(const CABufferList& buffer, UInt32 readOffset, UInt32 frameLength, UInt32 writeOffset) noexcept;


	/// Deletes at most the first @c frameLength frames from this @c CABufferList
	/// @param frameLength The desired number of frames
	/// @return The number of frames deleted
	inline UInt32 TrimFirst(UInt32 frameLength) noexcept
	{
		return TrimAtOffset(0, frameLength);
	}

	/// Deletes at most the last @c frameLength frames from this @c CABufferList
	/// @param frameLength The desired number of frames
	/// @return The number of frames deleted
	inline UInt32 TrimLast(UInt32 frameLength) noexcept
	{
		UInt32 framesToTrim = std::min(frameLength, mFrameLength);
		SetFrameLength(mFrameLength - framesToTrim);
		return framesToTrim;
	}

	/// Deletes at most @c frameLength frames from this @c CABufferList starting at @c offset
	/// @param offset The desired starting offset
	/// @param frameLength The desired number of frames
	/// @return The number of frames deleted
	UInt32 TrimAtOffset(UInt32 offset, UInt32 frameLength) noexcept;

	/// Fills the remainder of this @c CABufferList with silence
	/// @return The number of frames of silence appended
	inline UInt32 FillRemainderWithSilence() noexcept
	{
		return InsertSilence(mFrameLength, mFrameCapacity - mFrameLength);
	}

	/// Appends at most @c frameLength frames of silence
	/// @param frameLength The desired number of frames
	/// @return The number of frames of silence appended
	inline UInt32 AppendSilence(UInt32 frameLength) noexcept
	{
		return InsertSilence(mFrameLength, frameLength);
	}

	/// Inserts at most @c frameLength frames of silence starting at @c offset
	/// @param offset The desired starting offset
	/// @param frameLength The desired number of frames
	/// @return The number of frames of silence inserted
	UInt32 InsertSilence(UInt32 offset, UInt32 frameLength) noexcept;

#pragma mark AudioBufferList access

	/// Adopts an existing @c AudioBufferList
	/// @note The @c CABufferList assumes responsiblity for deallocating @c bufferList using @c std::free
	/// @param bufferList The @c AudioBufferList to adopt
	/// @param format The format of @c bufferList
	/// @param frameCapacity The frame capacity of @c bufferList
	/// @param frameLength The number of valid audio frames in @c bufferList
	/// @return @c true on sucess, @c false otherwise
	bool AdoptABL(AudioBufferList * _Nonnull bufferList, const AudioStreamBasicDescription& format, UInt32 frameCapacity, UInt32 frameLength) noexcept;

	/// Relinquishes ownership of the object's internal @c AudioBufferList and returns it
	/// @note The caller assumes responsiblity for deallocating the returned @c AudioBufferList using @c std::free
	AudioBufferList * _Nullable RelinquishABL() noexcept;

	/// Returns a pointer to this object's internal @c AudioBufferList
	inline AudioBufferList * _Nullable ABL() noexcept
	{
		return mBufferList;
	}

	/// Returns a const pointer to this object's internal @c AudioBufferList
	inline const AudioBufferList * _Nullable ABL() const noexcept
	{
		return mBufferList;
	}


	/// Returns @c true if this object's internal @c AudioBufferList is not @c nullptr
	inline explicit operator bool() const noexcept
	{
		return mBufferList != nullptr;
	}

	/// Returns @c true if this object's internal @c AudioBufferList is @c nullptr
	inline bool operator!() const noexcept
	{
		return !operator bool();
	}


	/// Returns a pointer to this object's internal @c AudioBufferList
	inline AudioBufferList * _Nullable operator->() noexcept
	{
		return mBufferList;
	}

	/// Returns a const pointer to this object's internal @c AudioBufferList
	inline const AudioBufferList * _Nullable operator->() const noexcept
	{
		return mBufferList;
	}

	/// Returns a pointer to this object's internal @c AudioBufferList
	inline operator AudioBufferList * const _Nullable () noexcept
	{
		return mBufferList;
	}

	/// Returns a const pointer to this object's internal @c AudioBufferList
	inline operator const AudioBufferList * const _Nullable () const noexcept
	{
		return mBufferList;
	}

private:

	/// The underlying @c AudioChannelLayout struct
	AudioBufferList * _Nullable mBufferList;
	/// The format of @c mBufferList
	CAStreamBasicDescription mFormat;
	/// The capacity of @c mBufferList in frames
	UInt32 mFrameCapacity;
	/// The number of valid frames in @c mBufferList
	UInt32 mFrameLength;

};

} // namespace SFB
