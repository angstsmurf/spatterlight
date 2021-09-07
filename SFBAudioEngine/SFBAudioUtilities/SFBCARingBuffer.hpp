//
// Copyright (c) 2013 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <atomic>

#import <CoreAudioTypes/CoreAudioTypes.h>

#import "SFBCAStreamBasicDescription.hpp"

namespace SFB {

/// A ring buffer supporting timestamped non-interleaved audio based on Apple's @c CARingBuffer.
///
/// This class is thread safe when used from one reader thread and one writer thread (single producer, single consumer model).
class CARingBuffer
{

public:

#pragma mark Creation and Destruction

	/// Creates a new @c CARingBuffer
	/// @note @c Allocate() must be called before the object may be used.
	CARingBuffer() noexcept;

	// This class is non-copyable
	CARingBuffer(const CARingBuffer& rhs) = delete;

	// This class is non-assignable
	CARingBuffer& operator=(const CARingBuffer& rhs) = delete;

	/// Destroys the @c CARingBuffer and release all associated resources.
	~CARingBuffer();

	// This class is non-movable
	CARingBuffer(CARingBuffer&& rhs) = delete;

	// This class is non-move assignable
	CARingBuffer& operator=(CARingBuffer&& rhs) = delete;

#pragma mark Buffer management

	/// Allocates space for audio data.
	/// @note Only interleaved formats are supported.
	/// @note This method is not thread safe.
	/// @note Capacities from 2 to 2,147,483,648 (0x80000000) frames are supported
	/// @param format The format of the audio that will be written to and read from this buffer.
	/// @param capacityFrames The desired capacity, in frames
	/// @return @c true on success, @c false on error
	bool Allocate(const CAStreamBasicDescription& format, uint32_t capacityFrames) noexcept;

	/// Frees the resources used by this @c CARingBuffer
	/// @note This method is not thread safe.
	void Deallocate() noexcept;


	/// Returns the capacity in frames of this @c CARingBuffer
	inline uint32_t CapacityFrames() const noexcept
	{
		return mCapacityFrames;
	}

	/// Returns the format of this @c CARingBuffer
	inline const CAStreamBasicDescription& Format() const noexcept
	{
		return mFormat;
	}

	/// Gets the time bounds of the audio contained in this @c CARingBuffer
	/// @param startTime The starting sample time of audio contained in the buffer
	/// @param endTime The end sample time of audio contained in the buffer
	/// @return @c true on success, @c false on error
	bool GetTimeBounds(int64_t& startTime, int64_t& endTime) const noexcept;

#pragma mark Reading and writing audio

	/// Reads audio from the @c CARingBuffer
	///
	/// The sample times should normally increase sequentially, although gaps are filled with silence. A sufficiently large
	/// gap effectively empties the buffer before storing the new data.
	/// @note Negative time stamps are not supported
	/// @note If @c timeStamp is less than the previous sample time the behavior is undefined
	/// @param bufferList An @c AudioBufferList to receive the audio
	/// @param frameCount The desired number of frames to read
	/// @param timeStamp The starting sample time
	/// @return @c true on success, @c false on error
	bool Read(AudioBufferList * const _Nonnull bufferList, uint32_t frameCount, int64_t timeStamp) noexcept;

	/// Writes audio to the @c CARingBuffer
	/// @note Negative time stamps are not supported
	/// @param bufferList An @c AudioBufferList containing the audio to copy
	/// @param frameCount The desired number of frames to write
	/// @param timeStamp The starting sample time
	/// @return @c true on success, @c false on error
	bool Write(const AudioBufferList * const _Nonnull bufferList, uint32_t frameCount, int64_t timeStamp) noexcept;

protected:

	/// Returns the byte offset of @c frameNumber
	inline uint32_t FrameByteOffset(int64_t frameNumber) const noexcept
	{
		return (static_cast<uint64_t>(frameNumber) & mCapacityFramesMask) * mFormat.mBytesPerFrame;
	}

	/// Constrains @c startRead and @c endRead to valid timestamps in the buffer
	bool ClampTimesToBounds(int64_t& startRead, int64_t& endRead) const noexcept;

	/// Returns the buffer's starting sample time
	/// @note This should only be called from @c Write()
	inline int64_t StartTime() const noexcept
	{
		return mTimeBoundsQueue[mTimeBoundsQueueCounter.load(std::memory_order_acquire) & sTimeBoundsQueueMask].mStartTime;
	}

	/// Returns the buffer's ending sample time
	/// @note This should only be called from @c Write()
	inline int64_t EndTime() const noexcept
	{
		return mTimeBoundsQueue[mTimeBoundsQueueCounter.load(std::memory_order_acquire) & sTimeBoundsQueueMask].mEndTime;
	}

	/// Sets the buffer's start and end sample times
	/// @note This should only be called from @c Write()
	void SetTimeBounds(int64_t startTime, int64_t endTime) noexcept;

private:

	/// The format of the audio
	CAStreamBasicDescription mFormat;

	/// The channel pointers and buffers allocated in one chunk of memory
	uint8_t * _Nonnull * _Nullable mBuffers;

	/// The frame capacity per channel
	uint32_t mCapacityFrames;
	/// Mask used to wrap read and write locations
	/// @note Equal to @c mCapacityFrames-1
	uint32_t mCapacityFramesMask;

	/// A range of valid sample times in the buffer
	struct TimeBounds {
		/// The starting sample time
		int64_t mStartTime;
		/// The ending sample time
		int64_t mEndTime;
		/// The value of @c mTimeBoundsQueueCounter when the struct was modified
		std::atomic_uint64_t mUpdateCounter;
	};

	/// The number of elements in @c mTimeBoundsQueue
	static const uint32_t sTimeBoundsQueueSize = 32;
	/// Mask value used to wrap time bounds counters
	/// @note Equal to @c sTimeBoundsQueueSize-1
	static const uint32_t sTimeBoundsQueueMask = sTimeBoundsQueueSize - 1;

	/// Array of @c TimeBounds structs
	TimeBounds mTimeBoundsQueue[sTimeBoundsQueueSize];
	/// Monotonically increasing counter incremented when the buffer's time bounds changes
	std::atomic_uint64_t mTimeBoundsQueueCounter;

};

} // namespace SFB
