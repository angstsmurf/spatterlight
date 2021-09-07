//
// Copyright (c) 2013 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#import <algorithm>
#import <cstdlib>
#import <cstring>
#import <limits>

#import "SFBAudioRingBuffer.hpp"

namespace {

/// Copies non-interleaved audio from @c bufferList to @c buffers
/// @param buffers The destination buffers
/// @param dstOffset The byte offset in @c buffers to begin writing
/// @param bufferList The source buffers
/// @param srcOffset The byte offset in @c bufferList to begin reading
/// @param byteCount The maximum number of bytes per non-interleaved buffer to read and write
inline void StoreABL(uint8_t * const _Nonnull * const _Nonnull buffers, uint32_t dstOffset, const AudioBufferList * const _Nonnull bufferList, uint32_t srcOffset, uint32_t byteCount) noexcept
{
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
		if(srcOffset > bufferList->mBuffers[i].mDataByteSize)
			continue;
		std::memcpy(buffers[i] + dstOffset, static_cast<const uint8_t *>(bufferList->mBuffers[i].mData) + srcOffset, std::min(byteCount, bufferList->mBuffers[i].mDataByteSize - srcOffset));
	}
}

/// Copies non-interleaved audio from @c buffers to @c bufferList
/// @param bufferList The destination buffers
/// @param dstOffset The byte offset in @c bufferList to begin writing
/// @param buffers The source buffers
/// @param srcOffset The byte offset in @c bufferList to begin reading
/// @param byteCount The maximum number of bytes per non-interleaved buffer to read and write
inline void FetchABL(AudioBufferList * const _Nonnull bufferList, uint32_t dstOffset, const uint8_t * const _Nonnull * const _Nonnull buffers, uint32_t srcOffset, uint32_t byteCount) noexcept
{
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
		if(dstOffset > bufferList->mBuffers[i].mDataByteSize)
			continue;
		std::memcpy(static_cast<uint8_t *>(bufferList->mBuffers[i].mData) + dstOffset, buffers[i] + srcOffset, std::min(byteCount, bufferList->mBuffers[i].mDataByteSize - dstOffset));
	}
}

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

SFB::AudioRingBuffer::AudioRingBuffer() noexcept
: mBuffers(nullptr), mCapacityFrames(0), mCapacityFramesMask(0), mWritePointer(0), mReadPointer(0)
{
	assert(mWritePointer.is_lock_free());
}

SFB::AudioRingBuffer::~AudioRingBuffer()
{
	std::free(mBuffers);
}

#pragma mark Buffer Management

bool SFB::AudioRingBuffer::Allocate(const CAStreamBasicDescription& format, uint32_t capacityFrames) noexcept
{
	// Only non-interleaved formats are supported
	if(format.IsInterleaved() || capacityFrames < 2 || capacityFrames > 0x80000000)
		return false;

	Deallocate();

	// Round up to the next power of two
	capacityFrames = NextPowerOfTwo(static_cast<uint32_t>(capacityFrames));

	mFormat = format;

	mCapacityFrames = capacityFrames;
	mCapacityFramesMask = capacityFrames - 1;

	uint32_t capacityBytes = capacityFrames * format.mBytesPerFrame;

	// One memory allocation holds everything- first the pointers followed by the deinterleaved channels
	uint32_t allocationSize = (capacityBytes + sizeof(uint8_t *)) * format.mChannelsPerFrame;
	uint8_t *memoryChunk = static_cast<uint8_t *>(std::malloc(allocationSize));
	if(!memoryChunk)
		return false;

	// Zero the entire allocation
	std::memset(memoryChunk, 0, allocationSize);

	// Assign the pointers and channel buffers
	mBuffers = reinterpret_cast<uint8_t **>(memoryChunk);
	memoryChunk += format.mChannelsPerFrame * sizeof(uint8_t *);
	for(UInt32 i = 0; i < format.mChannelsPerFrame; ++i) {
		mBuffers[i] = memoryChunk;
		memoryChunk += capacityBytes;
	}

	mReadPointer = 0;
	mWritePointer = 0;

	return true;
}

void SFB::AudioRingBuffer::Deallocate() noexcept
{
	if(mBuffers) {
		std::free(mBuffers);
		mBuffers = nullptr;

		mFormat.Reset();

		mCapacityFrames = 0;
		mCapacityFramesMask = 0;

		mReadPointer = 0;
		mWritePointer = 0;
	}
}

void SFB::AudioRingBuffer::Reset() noexcept
{
	mReadPointer = 0;
	mWritePointer = 0;
}

uint32_t SFB::AudioRingBuffer::FramesAvailableToRead() const noexcept
{
	auto writePointer = mWritePointer.load(std::memory_order_acquire);
	auto readPointer = mReadPointer.load(std::memory_order_acquire);

	if(writePointer > readPointer)
		return writePointer - readPointer;
	else
		return (writePointer - readPointer + mCapacityFrames) & mCapacityFramesMask;
}

uint32_t SFB::AudioRingBuffer::FramesAvailableToWrite() const noexcept
{
	auto writePointer = mWritePointer.load(std::memory_order_acquire);
	auto readPointer = mReadPointer.load(std::memory_order_acquire);

	if(writePointer > readPointer)
		return ((readPointer - writePointer + mCapacityFrames) & mCapacityFramesMask) - 1;
	else if(writePointer < readPointer)
		return (readPointer - writePointer) - 1;
	else
		return mCapacityFrames - 1;
}

#pragma mark Reading and Writing Audio

uint32_t SFB::AudioRingBuffer::Read(AudioBufferList * const bufferList, uint32_t frameCount) noexcept
{
	if(!bufferList || frameCount == 0)
		return 0;

	auto writePointer = mWritePointer.load(std::memory_order_acquire);
	auto readPointer = mReadPointer.load(std::memory_order_acquire);

	uint32_t framesAvailable;
	if(writePointer > readPointer)
		framesAvailable = writePointer - readPointer;
	else
		framesAvailable = (writePointer - readPointer + mCapacityFrames) & mCapacityFramesMask;

	if(framesAvailable == 0)
		return 0;

	auto framesToRead = std::min(framesAvailable, frameCount);
	if(readPointer + framesToRead > mCapacityFrames) {
		auto framesAfterReadPointer = mCapacityFrames - readPointer;
		auto bytesAfterReadPointer = framesAfterReadPointer * mFormat.mBytesPerFrame;
		FetchABL(bufferList, 0, mBuffers, readPointer * mFormat.mBytesPerFrame, bytesAfterReadPointer);
		FetchABL(bufferList, bytesAfterReadPointer, mBuffers, 0, (framesToRead - framesAfterReadPointer) * mFormat.mBytesPerFrame);
	}
	else
		FetchABL(bufferList, 0, mBuffers, readPointer * mFormat.mBytesPerFrame, framesToRead * mFormat.mBytesPerFrame);

	mReadPointer.store((readPointer + framesToRead) & mCapacityFramesMask, std::memory_order_release);

	// Set the ABL buffer sizes
	auto byteSize = static_cast<UInt32>(framesToRead) * mFormat.mBytesPerFrame;
	for(UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex)
		bufferList->mBuffers[bufferIndex].mDataByteSize = byteSize;

	return framesToRead;
}

uint32_t SFB::AudioRingBuffer::Write(const AudioBufferList * const bufferList, uint32_t frameCount) noexcept
{
	if(!bufferList || frameCount == 0)
		return 0;

	auto writePointer = mWritePointer.load(std::memory_order_acquire);
	auto readPointer = mReadPointer.load(std::memory_order_acquire);

	uint32_t framesAvailable;
	if(writePointer > readPointer)
		framesAvailable = ((readPointer - writePointer + mCapacityFrames) & mCapacityFramesMask) - 1;
	else if(writePointer < readPointer)
		framesAvailable = (readPointer - writePointer) - 1;
	else
		framesAvailable = mCapacityFrames - 1;

	if(framesAvailable == 0)
		return 0;

	auto framesToWrite = std::min(framesAvailable, frameCount);
	if(writePointer + framesToWrite > mCapacityFrames) {
		auto framesAfterWritePointer = mCapacityFrames - writePointer;
		auto bytesAfterWritePointer = framesAfterWritePointer * mFormat.mBytesPerFrame;
		StoreABL(mBuffers, writePointer * mFormat.mBytesPerFrame, bufferList, 0, bytesAfterWritePointer);
		StoreABL(mBuffers, 0, bufferList, bytesAfterWritePointer, (framesToWrite - framesAfterWritePointer) * mFormat.mBytesPerFrame);
	}
	else
		StoreABL(mBuffers, writePointer * mFormat.mBytesPerFrame, bufferList, 0, framesToWrite * mFormat.mBytesPerFrame);

	mWritePointer.store((writePointer + framesToWrite) & mCapacityFramesMask, std::memory_order_release);

	return framesToWrite;
}
