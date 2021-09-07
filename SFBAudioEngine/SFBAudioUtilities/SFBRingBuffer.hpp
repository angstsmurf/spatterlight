//
// Copyright (c) 2014 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <atomic>

namespace SFB {

/// A generic ring buffer.
///
/// This class is thread safe when used from one reader thread and one writer thread (single producer, single consumer model).
class RingBuffer
{

public:

#pragma mark Creation and Destruction

	/// Creates a new @c RingBuffer
	/// @note @c Allocate() must be called before the object may be used.
	RingBuffer() noexcept;

	// This class is non-copyable
	RingBuffer(const RingBuffer& rhs) = delete;

	// This class is non-assignable
	RingBuffer& operator=(const RingBuffer& rhs) = delete;

	/// Destroys the @c RingBuffer and release all associated resources.
	~RingBuffer();

	// This class is non-movable
	RingBuffer(RingBuffer&& rhs) = delete;

	// This class is non-move assignable
	RingBuffer& operator=(RingBuffer&& rhs) = delete;

#pragma mark Buffer management

	/// Allocates space for data.
	/// @note This method is not thread safe.
	/// @note Capacities from 2 to 2,147,483,648 (0x80000000) bytes are supported
	/// @param byteCount The desired capacity, in bytes
	/// @return @c true on success, @c false on error
	bool Allocate(uint32_t byteCount) noexcept;

	/// Frees the resources used by this @c RingBuffer
	/// @note This method is not thread safe.
	void Deallocate() noexcept;


	/// Resets this @c RingBuffer to its default state.
	/// @note This method is not thread safe.
	void Reset() noexcept;


	/// Returns the capacity of this RingBuffer in bytes
	inline uint32_t CapacityBytes() const noexcept
	{
		return mCapacityBytes;
	}

	/// Returns the number of bytes available for reading
	uint32_t BytesAvailableToRead() const noexcept;

	/// Returns the free space available for writing in bytes
	uint32_t BytesAvailableToWrite() const noexcept;

#pragma mark Reading and writing data

	/// Read data from the @c RingBuffer, advancing the read pointer.
	/// @param destinationBuffer An address to receive the data
	/// @param byteCount The desired number of bytes to read
	/// @return The number of bytes actually read
	uint32_t Read(void * const _Nonnull destinationBuffer, uint32_t byteCount) noexcept;

	/// Read data from the @c RingBuffer without advancing the read pointer.
	/// @param destinationBuffer An address to receive the data
	/// @param byteCount The desired number of bytes to read
	/// @return The number of bytes actually read
	uint32_t Peek(void * const _Nonnull destinationBuffer, uint32_t byteCount) const noexcept;

	/// Write data to the @c RingBuffer, advancing the write pointer.
	/// @param sourceBuffer An address containing the data to copy
	/// @param byteCount The desired number of frames to write
	/// @return The number of bytes actually written
	uint32_t Write(const void * const _Nonnull sourceBuffer, uint32_t byteCount) noexcept;


	/// Advance the read position by the specified number of bytes
	void AdvanceReadPosition(uint32_t byteCount) noexcept;

	/// Advance the write position by the specified number of bytes
	void AdvanceWritePosition(uint32_t byteCount) noexcept;


	/// A read-only memory buffer
	struct ReadBuffer {
		/// The memory buffer location
		const uint8_t * const _Nullable mBuffer;
		/// The number of bytes of valid data in @c mBuffer
		const uint32_t mBufferSize;

		/// Construct an empty @c ReadBuffer
		ReadBuffer() noexcept
		: ReadBuffer(nullptr, 0)
		{}

		/// Construct a @c ReadBuffer for the specified location and size
		/// @param buffer The memory buffer location
		/// @param bufferSize The number of bytes of valid data in @c buffer
		ReadBuffer(const uint8_t * const _Nullable buffer, uint32_t bufferSize) noexcept
		: mBuffer(buffer), mBufferSize(bufferSize)
		{}
	};

	/// A pair of @c ReadBuffer objects
	using ReadBufferPair = std::pair<const ReadBuffer, const ReadBuffer>;

	/// Returns the read vector containing the current readable data
	const ReadBufferPair ReadVector() const noexcept;


	/// A write-only memory buffer
	struct WriteBuffer {
		/// The memory buffer location
		uint8_t * const _Nullable mBuffer;
		/// The capacity of @c mBuffer in bytes
		const uint32_t mBufferCapacity;

		/// Construct an empty @c WriteBuffer
		WriteBuffer() noexcept
		: WriteBuffer(nullptr, 0)
		{}

		/// Construct a @c WriteBuffer for the specified location and capacity
		/// @param buffer The memory buffer location
		/// @param bufferCapacity The capacity of @c buffer in bytes
		WriteBuffer(uint8_t * const _Nullable buffer, uint32_t bufferCapacity) noexcept
		: mBuffer(buffer), mBufferCapacity(bufferCapacity)
		{}
	};

	/// A pair of @c WriteBuffer objects
	using WriteBufferPair = std::pair<const WriteBuffer, const WriteBuffer>;

	/// Returns the write vector containing the current writable space
	const WriteBufferPair WriteVector() const noexcept;

private:

	/// The memory buffer holding the data
	uint8_t * _Nullable mBuffer;

	/// The capacity of @c mBuffer in bytes
	uint32_t mCapacityBytes;
	/// The capacity of @c mBuffer in bytes minus one
	uint32_t mCapacityBytesMask;

	/// The offset into @c mBuffer of the read location
	std::atomic_uint32_t mWritePosition;
	/// The offset into @c mBuffer of the write location
	std::atomic_uint32_t mReadPosition;

};

} // namespace SFB
