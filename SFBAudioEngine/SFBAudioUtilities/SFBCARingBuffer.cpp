//
// Copyright (c) 2013 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#import <algorithm>
#import <cstdlib>
#import <cstring>
#import <limits>

#import "SFBCARingBuffer.hpp"

namespace {

/// Zeroes a range of bytes in @c buffers
/// @param buffers The destination buffers
/// @param bufferCount The number of buffers
/// @param byteOffset The byte offset in @c buffers to begin writing
/// @param byteCount The number of bytes per non-interleaved buffer to write
inline void ZeroRange(uint8_t * const _Nonnull * const _Nonnull buffers, uint32_t bufferCount, uint32_t byteOffset, uint32_t byteCount)
{
	for(uint32_t i = 0; i < bufferCount; ++i)
		std::memset(buffers[i] + byteOffset, 0, byteCount);
}

/// Zeroes a range of bytes in @c bufferList
/// @param bufferList The destination buffers
/// @param byteOffset The byte offset in @c bufferList to begin writing
/// @param byteCount The maximum number of bytes per non-interleaved buffer to write
inline void ZeroABL(AudioBufferList * const _Nonnull bufferList, uint32_t byteOffset, uint32_t byteCount)
{
	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
		if(byteOffset > bufferList->mBuffers[i].mDataByteSize)
			continue;
		std::memset(static_cast<uint8_t *>(bufferList->mBuffers[i].mData) + byteOffset, 0, std::min(byteCount, bufferList->mBuffers[i].mDataByteSize - byteOffset));
	}
}

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
		std::memcpy(buffers[i] + dstOffset, static_cast<uint8_t *>(bufferList->mBuffers[i].mData) + srcOffset, std::min(byteCount, bufferList->mBuffers[i].mDataByteSize - srcOffset));
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

SFB::CARingBuffer::CARingBuffer() noexcept
: mBuffers(nullptr), mCapacityFrames(0), mCapacityFramesMask(0)
{
	assert(mTimeBoundsQueueCounter.is_lock_free());
}

SFB::CARingBuffer::~CARingBuffer()
{
	std::free(mBuffers);
}

#pragma mark Buffer Management

bool SFB::CARingBuffer::Allocate(const CAStreamBasicDescription& format, uint32_t capacityFrames) noexcept
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

	// Zero the time bounds queue
	for(uint32_t i = 0; i < sTimeBoundsQueueSize; ++i) {
		mTimeBoundsQueue[i].mStartTime = 0;
		mTimeBoundsQueue[i].mEndTime = 0;
		mTimeBoundsQueue[i].mUpdateCounter = 0;
	}

	mTimeBoundsQueueCounter = 0;

	return true;
}

void SFB::CARingBuffer::Deallocate() noexcept
{
	if(mBuffers) {
		std::free(mBuffers);
		mBuffers = nullptr;

		mFormat.Reset();

		mCapacityFrames = 0;
		mCapacityFramesMask = 0;

		for(uint32_t i = 0; i < sTimeBoundsQueueSize; ++i) {
			mTimeBoundsQueue[i].mStartTime = 0;
			mTimeBoundsQueue[i].mEndTime = 0;
			mTimeBoundsQueue[i].mUpdateCounter = 0;
		}

		mTimeBoundsQueueCounter = 0;
	}
}

bool SFB::CARingBuffer::GetTimeBounds(int64_t& startTime, int64_t& endTime) const noexcept
{
	for(auto i = 0; i < 8; ++i) {
		auto currentCounter = mTimeBoundsQueueCounter.load(std::memory_order_acquire);
		auto currentIndex = currentCounter & sTimeBoundsQueueMask;

		const SFB::CARingBuffer::TimeBounds * const bounds = mTimeBoundsQueue + currentIndex;

		startTime = bounds->mStartTime;
		endTime = bounds->mEndTime;

		auto counter = bounds->mUpdateCounter.load(std::memory_order_acquire);
		if(counter == currentCounter)
			return true;
	}

	return false;
}

#pragma mark Reading and Writing Audio

bool SFB::CARingBuffer::Read(AudioBufferList * const bufferList, uint32_t frameCount, int64_t startRead) noexcept
{
	if(frameCount == 0)
		return true;

	if(!bufferList || frameCount > mCapacityFrames || startRead < 0)
		return false;

	auto endRead = startRead + static_cast<int64_t>(frameCount);

	auto startRead0 = startRead;
	auto endRead0 = endRead;

	if(!ClampTimesToBounds(startRead, endRead))
		return false;

	if(startRead == endRead) {
		ZeroABL(bufferList, 0, frameCount * mFormat.mBytesPerFrame);
		return true;
	}

	auto byteSize = static_cast<uint32_t>(endRead - startRead) * mFormat.mBytesPerFrame;

	auto destStartByteOffset = static_cast<uint32_t>(std::max(static_cast<int64_t>(0), (startRead - startRead0) * mFormat.mBytesPerFrame));
	if(destStartByteOffset > 0)
		ZeroABL(bufferList, 0, std::min(frameCount * mFormat.mBytesPerFrame, destStartByteOffset));

	auto destEndSize = static_cast<uint32_t>(std::max(static_cast<int64_t>(0), endRead0 - endRead));
	if(destEndSize > 0)
		ZeroABL(bufferList, destStartByteOffset + byteSize, destEndSize * mFormat.mBytesPerFrame);

	auto offset0 = FrameByteOffset(startRead);
	auto offset1 = FrameByteOffset(endRead);
	uint32_t byteCount;

	if(offset0 < offset1) {
		byteCount = offset1 - offset0;
		FetchABL(bufferList, destStartByteOffset, mBuffers, offset0, byteCount);
	}
	else {
		byteCount = static_cast<UInt32>((mCapacityFrames * mFormat.mBytesPerFrame) - offset0);
		FetchABL(bufferList, destStartByteOffset, mBuffers, offset0, byteCount);
		FetchABL(bufferList, destStartByteOffset + byteCount, mBuffers, 0, offset1);
		byteCount += offset1;
	}

	for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i)
		bufferList->mBuffers[i].mDataByteSize = static_cast<UInt32>(byteCount);

	return true;
}

bool SFB::CARingBuffer::Write(const AudioBufferList * const bufferList, uint32_t frameCount, int64_t startWrite) noexcept
{
	if(frameCount == 0)
		return true;

	if(!bufferList || frameCount > mCapacityFrames || startWrite < 0)
		return false;

	auto endWrite = startWrite + static_cast<int64_t>(frameCount);

	// Going backwards, throw everything out
	if(startWrite < EndTime())
		SetTimeBounds(startWrite, startWrite);
	// The buffer has not yet wrapped and will not need to
	else if(endWrite - StartTime() <= static_cast<int64_t>(mCapacityFrames))
		;
	// Advance the start time past the region about to be overwritten
	else {
		int64_t newStart = endWrite - static_cast<int64_t>(mCapacityFrames);	// one buffer of time behind the write position
		int64_t newEnd = std::max(newStart, EndTime());
		SetTimeBounds(newStart, newEnd);
	}

	uint32_t offset0, offset1;
	auto curEnd = EndTime();

	if(startWrite > curEnd) {
		// Zero the range of samples being skipped
		offset0 = FrameByteOffset(curEnd);
		offset1 = FrameByteOffset(startWrite);
		if(offset0 < offset1)
			ZeroRange(mBuffers, mFormat.ChannelStreamCount(), offset0, offset1 - offset0);
		else {
			ZeroRange(mBuffers, mFormat.ChannelStreamCount(), offset0, (mCapacityFrames * mFormat.mBytesPerFrame) - offset0);
			ZeroRange(mBuffers, mFormat.ChannelStreamCount(), 0, offset1);
		}

		offset0 = offset1;
	}
	else
		offset0 = FrameByteOffset(startWrite);

	offset1 = FrameByteOffset(endWrite);
	if(offset0 < offset1)
		StoreABL(mBuffers, offset0, bufferList, 0, offset1 - offset0);
	else {
		auto byteCount = (mCapacityFrames * mFormat.mBytesPerFrame) - offset0;
		StoreABL(mBuffers, offset0, bufferList, 0, byteCount);
		StoreABL(mBuffers, 0, bufferList, byteCount, offset1);
	}

	// Update the end time
	SetTimeBounds(StartTime(), endWrite);

	return true;
}

#pragma mark Internals

void SFB::CARingBuffer::SetTimeBounds(int64_t startTime, int64_t endTime) noexcept
{
	auto nextCounter = mTimeBoundsQueueCounter.load(std::memory_order_acquire) + 1;
	auto nextIndex = nextCounter & sTimeBoundsQueueMask;

	mTimeBoundsQueue[nextIndex].mStartTime = startTime;
	mTimeBoundsQueue[nextIndex].mEndTime = endTime;
	mTimeBoundsQueue[nextIndex].mUpdateCounter.store(nextCounter, std::memory_order_release);

	mTimeBoundsQueueCounter.store(nextCounter, std::memory_order_release);
}

bool SFB::CARingBuffer::ClampTimesToBounds(int64_t& startRead, int64_t& endRead) const noexcept
{
	int64_t startTime, endTime;
	if(!GetTimeBounds(startTime, endTime))
		return false;

	if(startRead > endTime || endRead < startTime) {
		endRead = startRead;
		return true;
	}

	startRead = std::max(startRead, startTime);
	endRead = std::max(std::min(endRead, endTime), startRead);

	return true;
}
