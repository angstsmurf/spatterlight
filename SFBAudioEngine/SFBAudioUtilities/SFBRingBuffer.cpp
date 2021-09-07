//
// Copyright (c) 2014 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#import <algorithm>
#import <cstdlib>
#import <cstring>
#import <limits>

#import "SFBRingBuffer.hpp"

namespace {

/// Returns the smallest power of two value greater than @c x
/// @param x A value in the range [2..2147483648]
/// @return The smallest power of two greater than @c x
inline constexpr uint32_t NextPowerOfTwo(uint32_t x) noexcept
{
	assert(x > 1);
	assert(x <= ((std::numeric_limits<uint32_t>::max() / 2) + 1));
	return static_cast<uint32_t>(1 << (32 - __builtin_clz(x - 1)));
}

}

#pragma mark Creation and Destruction

SFB::RingBuffer::RingBuffer() noexcept
: mBuffer(nullptr), mCapacityBytes(0), mCapacityBytesMask(0), mWritePosition(0), mReadPosition(0)
{
	assert(mWritePosition.is_lock_free());
}

SFB::RingBuffer::~RingBuffer()
{
	std::free(mBuffer);
}

#pragma mark Buffer Management

bool SFB::RingBuffer::Allocate(uint32_t capacityBytes) noexcept
{
	if(capacityBytes < 2 || capacityBytes > 0x80000000)
		return false;

	Deallocate();

	// Round up to the next power of two
	capacityBytes = NextPowerOfTwo(static_cast<uint32_t>(capacityBytes));

	mBuffer = static_cast<uint8_t *>(std::malloc(capacityBytes));
	if(!mBuffer)
		return false;

	mCapacityBytes = capacityBytes;
	mCapacityBytesMask = capacityBytes - 1;

	return true;
}

void SFB::RingBuffer::Deallocate() noexcept
{
	if(mBuffer) {
		std::free(mBuffer);
		mBuffer = nullptr;

		mCapacityBytes = 0;
		mCapacityBytesMask = 0;

		mReadPosition = 0;
		mWritePosition = 0;
	}
}

void SFB::RingBuffer::Reset() noexcept
{
	mReadPosition = 0;
	mWritePosition = 0;
}

uint32_t SFB::RingBuffer::BytesAvailableToRead() const noexcept
{
	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	if(writePosition > readPosition)
		return writePosition - readPosition;
	else
		return (writePosition - readPosition + mCapacityBytes) & mCapacityBytesMask;
}

uint32_t SFB::RingBuffer::BytesAvailableToWrite() const noexcept
{
	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	if(writePosition > readPosition)
		return ((readPosition - writePosition + mCapacityBytes) & mCapacityBytesMask) - 1;
	else if(writePosition < readPosition)
		return (readPosition - writePosition) - 1;
	else
		return mCapacityBytes - 1;
}

#pragma mark Reading and Writing Data

uint32_t SFB::RingBuffer::Read(void * const destinationBuffer, uint32_t byteCount) noexcept
{
	if(!destinationBuffer || byteCount == 0)
		return 0;

	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	uint32_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = writePosition - readPosition;
	else
		bytesAvailable = (writePosition - readPosition + mCapacityBytes) & mCapacityBytesMask;

	if(bytesAvailable == 0)
		return 0;

	auto bytesToRead = std::min(bytesAvailable, byteCount);
	if(readPosition + bytesToRead > mCapacityBytes) {
		auto bytesAfterReadPointer = mCapacityBytes - readPosition;
		std::memcpy(destinationBuffer, mBuffer + readPosition, bytesAfterReadPointer);
		std::memcpy(static_cast<uint8_t *>(destinationBuffer) + bytesAfterReadPointer, mBuffer, bytesToRead - bytesAfterReadPointer);
	}
	else
		std::memcpy(destinationBuffer, mBuffer + readPosition, bytesToRead);

	mReadPosition.store((readPosition + bytesToRead) & mCapacityBytesMask, std::memory_order_release);

	return bytesToRead;
}

uint32_t SFB::RingBuffer::Peek(void * const destinationBuffer, uint32_t byteCount) const noexcept
{
	if(!destinationBuffer || byteCount == 0)
		return 0;

	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	uint32_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = writePosition - readPosition;
	else
		bytesAvailable = (writePosition - readPosition + mCapacityBytes) & mCapacityBytesMask;

	if(bytesAvailable == 0)
		return 0;

	auto bytesToRead = std::min(bytesAvailable, byteCount);
	if(readPosition + bytesToRead > mCapacityBytes) {
		auto bytesAfterReadPointer = mCapacityBytes - readPosition;
		std::memcpy(destinationBuffer, mBuffer + readPosition, bytesAfterReadPointer);
		std::memcpy(static_cast<uint8_t *>(destinationBuffer) + bytesAfterReadPointer, mBuffer, bytesToRead - bytesAfterReadPointer);
	}
	else
		std::memcpy(destinationBuffer, mBuffer + readPosition, bytesToRead);

	return bytesToRead;
}

uint32_t SFB::RingBuffer::Write(const void * const sourceBuffer, uint32_t byteCount) noexcept
{
	if(!sourceBuffer || byteCount == 0)
		return 0;

	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	uint32_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = ((readPosition - writePosition + mCapacityBytes) & mCapacityBytesMask) - 1;
	else if(writePosition < readPosition)
		bytesAvailable = (readPosition - writePosition) - 1;
	else
		bytesAvailable = mCapacityBytes - 1;

	if(bytesAvailable == 0)
		return 0;

	auto bytesToWrite = std::min(bytesAvailable, byteCount);
	if(writePosition + bytesToWrite > mCapacityBytes) {
		auto bytesAfterWritePointer = mCapacityBytes - writePosition;
		std::memcpy(mBuffer + writePosition, sourceBuffer, bytesAfterWritePointer);
		std::memcpy(mBuffer, static_cast<const uint8_t *>(sourceBuffer) + bytesAfterWritePointer, bytesToWrite - bytesAfterWritePointer);
	}
	else
		std::memcpy(mBuffer + writePosition, sourceBuffer, bytesToWrite);

	mWritePosition.store((writePosition + bytesToWrite) & mCapacityBytesMask, std::memory_order_release);

	return bytesToWrite;
}

void SFB::RingBuffer::AdvanceReadPosition(uint32_t byteCount) noexcept
{
	mReadPosition.store((mReadPosition.load(std::memory_order_acquire) + byteCount) & mCapacityBytesMask, std::memory_order_release);
}

void SFB::RingBuffer::AdvanceWritePosition(uint32_t byteCount) noexcept
{
	mWritePosition.store((mWritePosition.load(std::memory_order_acquire) + byteCount) & mCapacityBytesMask, std::memory_order_release);
}

const SFB::RingBuffer::ReadBufferPair SFB::RingBuffer::ReadVector() const noexcept
{
	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	uint32_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = writePosition - readPosition;
	else
		bytesAvailable = (writePosition - readPosition + mCapacityBytes) & mCapacityBytesMask;

	auto endOfRead = readPosition + bytesAvailable;

	if(endOfRead > mCapacityBytes)
		return { { mBuffer + readPosition, mCapacityBytes - readPosition }, { mBuffer, endOfRead & mCapacityBytes } };
	else
		return { { mBuffer + readPosition, bytesAvailable }, {} };
}

const SFB::RingBuffer::WriteBufferPair SFB::RingBuffer::WriteVector() const noexcept
{
	auto writePosition = mWritePosition.load(std::memory_order_acquire);
	auto readPosition = mReadPosition.load(std::memory_order_acquire);

	uint32_t bytesAvailable;
	if(writePosition > readPosition)
		bytesAvailable = ((readPosition - writePosition + mCapacityBytes) & mCapacityBytesMask) - 1;
	else if(writePosition < readPosition)
		bytesAvailable = (readPosition - writePosition) - 1;
	else
		bytesAvailable = mCapacityBytes - 1;

	auto endOfWrite = writePosition + bytesAvailable;

	if(endOfWrite > mCapacityBytes)
		return { { mBuffer + writePosition, mCapacityBytes - writePosition }, { mBuffer, endOfWrite & mCapacityBytes } };
	else
		return { { mBuffer + writePosition, bytesAvailable }, {} };
}
