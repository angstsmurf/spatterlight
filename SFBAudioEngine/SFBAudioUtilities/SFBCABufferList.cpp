//
// Copyright (c) 2013 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#import <algorithm>
#import <cstdlib>
#import <limits>
#import <new>

#import "SFBCABufferList.hpp"

AudioBufferList * SFB::AllocateAudioBufferList(const CAStreamBasicDescription& format, UInt32 frameCapacity) noexcept
{
	if(format.mBytesPerFrame == 0 || frameCapacity > (std::numeric_limits<UInt32>::max() / format.mBytesPerFrame))
		return nullptr;

	auto bufferDataSize = format.FrameCountToByteSize(frameCapacity);
	auto bufferCount = format.ChannelStreamCount();
	auto bufferListSize = offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * bufferCount);
	auto allocationSize = bufferListSize + (bufferDataSize * bufferCount);

	auto abl = static_cast<AudioBufferList *>(std::malloc(allocationSize));
	if(!abl)
		return nullptr;

	std::memset(abl, 0, allocationSize);

	abl->mNumberBuffers = bufferCount;

	for(UInt32 i = 0; i < bufferCount; ++i) {
		abl->mBuffers[i].mNumberChannels = format.InterleavedChannelCount();
		abl->mBuffers[i].mData = reinterpret_cast<uint8_t *>(abl) + bufferListSize + (bufferDataSize * i);
		abl->mBuffers[i].mDataByteSize = bufferDataSize;
	}

	return abl;
}

SFB::CABufferList::CABufferList() noexcept
: mBufferList(nullptr), mFrameCapacity(0), mFrameLength(0)
{}

SFB::CABufferList::~CABufferList()
{
	std::free(mBufferList);
}

SFB::CABufferList::CABufferList(CABufferList&& rhs) noexcept
: mBufferList(rhs.mBufferList), mFormat(rhs.mFormat), mFrameCapacity(rhs.mFrameCapacity), mFrameLength(rhs.mFrameLength)
{
	rhs.mBufferList = nullptr;
	rhs.mFormat.Reset();
	rhs.mFrameCapacity = 0;
	rhs.mFrameLength = 0;
}

SFB::CABufferList& SFB::CABufferList::operator=(CABufferList&& rhs) noexcept
{
	if(this != &rhs) {
		Deallocate();

		mBufferList = rhs.mBufferList;
		mFormat = rhs.mFormat;
		mFrameCapacity = rhs.mFrameCapacity;
		mFrameLength = rhs.mFrameLength;

		rhs.mBufferList = nullptr;
		rhs.mFormat.Reset();
		rhs.mFrameCapacity = 0;
		rhs.mFrameLength = 0;
	}

	return *this;
}

SFB::CABufferList::CABufferList(const CAStreamBasicDescription& format, UInt32 frameCapacity)
: CABufferList()
{
	if(format.mBytesPerFrame == 0)
		throw std::invalid_argument("format.mBytesPerFrame == 0");
	if(!Allocate(format, frameCapacity))
		throw std::bad_alloc();
}

#pragma mark Buffer Management

bool SFB::CABufferList::Allocate(const CAStreamBasicDescription& format, UInt32 frameCapacity) noexcept
{
	if(mBufferList)
		Deallocate();

	mBufferList = AllocateAudioBufferList(format, frameCapacity);
	if(!mBufferList)
		return false;

	mFormat = format;
	mFrameCapacity = frameCapacity;
	mFrameLength = 0;

	return true;
}

void SFB::CABufferList::Deallocate() noexcept
{
	if(mBufferList) {
		std::free(mBufferList);
		mBufferList = nullptr;

		mFormat.Reset();

		mFrameCapacity = 0;
		mFrameLength = 0;
	}
}

bool SFB::CABufferList::SetFrameLength(UInt32 frameLength) noexcept
{
	if(!mBufferList || frameLength > mFrameCapacity)
		return false;

	mFrameLength = frameLength;

	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i)
		mBufferList->mBuffers[i].mDataByteSize = mFrameLength * mFormat.mBytesPerFrame;

	return true;
}

bool SFB::CABufferList::InferFrameLengthFromABL()
{
	if(!mBufferList)
		return false;

	auto buffer0ByteSize = mBufferList->mBuffers[0].mDataByteSize;
	for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
		if(mBufferList->mBuffers[i].mDataByteSize != buffer0ByteSize)
			throw std::logic_error("inconsistent values for mBufferList->mBuffers[].mBytesPerFrame");
	}

	auto frameLength = buffer0ByteSize / mFormat.mBytesPerFrame;
	if(frameLength > mFrameCapacity)
		throw std::logic_error("mBufferList->mBuffers[0].mBytesPerFrame / mFormat.mBytesPerFrame > mFrameCapacity");

	mFrameLength = frameLength;

	return true;
}

#pragma mark Buffer Utilities

UInt32 SFB::CABufferList::InsertFromBuffer(const CABufferList& buffer, UInt32 readOffset, UInt32 frameLength, UInt32 writeOffset) noexcept
{
	if(mFormat != buffer.mFormat)
//		throw std::invalid_argument("mFormat != buffer.mFormat");
		return 0;

	if(readOffset > buffer.mFrameLength || writeOffset > mFrameLength || frameLength == 0 || buffer.mFrameLength == 0)
		return 0;

	auto framesToInsert = std::min(mFrameCapacity - mFrameLength, std::min(frameLength, buffer.mFrameLength - readOffset));

	auto framesToMove = mFrameLength - writeOffset;
	if(framesToMove) {
		auto moveToOffset = writeOffset + framesToInsert;
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			auto dst = static_cast<uint8_t *>(mBufferList->mBuffers[i].mData) + (moveToOffset * mFormat.mBytesPerFrame);
			const auto src = static_cast<const uint8_t *>(mBufferList->mBuffers[i].mData) + (writeOffset * mFormat.mBytesPerFrame);
			std::memmove(dst, src, framesToMove * mFormat.mBytesPerFrame);
		}
	}

	if(framesToInsert) {
		for(UInt32 i = 0; i < buffer.mBufferList->mNumberBuffers; ++i) {
			auto dst = static_cast<uint8_t *>(mBufferList->mBuffers[i].mData) + (writeOffset * mFormat.mBytesPerFrame);
			const auto src = static_cast<const uint8_t *>(buffer.mBufferList->mBuffers[i].mData) + (readOffset * mFormat.mBytesPerFrame);
			std::memcpy(dst, src, framesToInsert * mFormat.mBytesPerFrame);
		}

		SetFrameLength(mFrameLength + framesToInsert);
	}

	return framesToInsert;
}

UInt32 SFB::CABufferList::TrimAtOffset(UInt32 offset, UInt32 frameLength) noexcept
{
	if(offset > mFrameLength || frameLength == 0)
		return 0;

	auto framesToTrim = std::min(frameLength, mFrameLength - offset);

	auto framesToMove = mFrameLength - (offset + framesToTrim);
	if(framesToMove) {
		auto moveFromOffset = offset + framesToTrim;
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			auto dst = static_cast<uint8_t *>(mBufferList->mBuffers[i].mData) + (offset * mFormat.mBytesPerFrame);
			const auto src = static_cast<const uint8_t *>(mBufferList->mBuffers[i].mData) + (moveFromOffset * mFormat.mBytesPerFrame);
			std::memmove(dst, src, framesToMove * mFormat.mBytesPerFrame);
		}
	}

	SetFrameLength(mFrameLength - framesToTrim);

	return framesToTrim;
}

UInt32 SFB::CABufferList::InsertSilence(UInt32 offset, UInt32 frameLength) noexcept
{
	if(!(mFormat.IsFloat() || mFormat.IsSignedInteger()))
//		throw std::logic_error("Inserting silence for unsigned integer samples not supported");
		return 0;

	if(offset > mFrameLength || frameLength == 0)
		return 0;

	auto framesToZero = std::min(mFrameCapacity - mFrameLength, frameLength);

	auto framesToMove = mFrameLength - offset;
	if(framesToMove) {
		auto moveToOffset = offset + framesToZero;
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			auto dst = static_cast<uint8_t *>(mBufferList->mBuffers[i].mData) + (moveToOffset * mFormat.mBytesPerFrame);
			const auto src = static_cast<const uint8_t *>(mBufferList->mBuffers[i].mData) + (offset * mFormat.mBytesPerFrame);
			std::memmove(dst, src, framesToMove * mFormat.mBytesPerFrame);
		}
	}

	if(framesToZero) {
		// For floating-point numbers this code is non-portable: the C standard doesn't require IEEE 754 compliance
		// However, setting all bits to 0 using memset() on macOS results in a floating-point value of 0
		for(UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
			auto dst = static_cast<uint8_t *>(mBufferList->mBuffers[i].mData) + (offset * mFormat.mBytesPerFrame);
			std::memset(dst, 0, framesToZero * mFormat.mBytesPerFrame);
		}

		SetFrameLength(mFrameLength + framesToZero);
	}

	return framesToZero;
}

bool SFB::CABufferList::AdoptABL(AudioBufferList *bufferList, const AudioStreamBasicDescription& format, UInt32 frameCapacity, UInt32 frameLength) noexcept
{
	if(!bufferList || frameLength > frameCapacity)
		return false;

	Deallocate();

	mBufferList = bufferList;
	mFormat = format;
	mFrameCapacity = frameCapacity;
	SetFrameLength(frameLength);

	return true;
}

AudioBufferList * SFB::CABufferList::RelinquishABL() noexcept
{
	auto bufferList = mBufferList;

	mBufferList = nullptr;
	mFormat.Reset();
	mFrameCapacity = 0;
	mFrameLength = 0;

	return bufferList;
}
