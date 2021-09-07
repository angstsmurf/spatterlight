//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <AudioToolbox/AudioFile.h>

namespace SFB {

/// A bare-bones wrapper around @c AudioFile that does little more than call @c AudioFileClose on destruction
class AudioFileWrapper
{

public:

	/// Creates an empty @c AudioFileWrapper
	inline AudioFileWrapper() noexcept
	: mAudioFile(nullptr)
	{}

	// This class is non-copyable
	AudioFileWrapper(const AudioFileWrapper& rhs) = delete;

	// This class is non-assignable
	AudioFileWrapper& operator=(const AudioFileWrapper& rhs) = delete;

	/// Calls @c AudioFileClose on the managed @c AudioFile
	inline ~AudioFileWrapper() noexcept
	{
		reset();
	}

	/// Move constructor
	inline AudioFileWrapper(AudioFileWrapper&& rhs) noexcept
	: mAudioFile(rhs.release())
	{}

	// Move assignment operator
	inline AudioFileWrapper& operator=(AudioFileWrapper&& rhs)
	{
		if(this != &rhs)
			reset(rhs.release());
		return *this;
	}

	/// Creates an @c AudioFileWrapper managing @c audioFile
	inline AudioFileWrapper(AudioFileID audioFile) noexcept
	: mAudioFile(audioFile)
	{}

	/// Returns @c true if the managed @c AudioFile is not @c nullptr
	inline explicit operator bool() const noexcept
	{
		return mAudioFile != nullptr;
	}

	/// Returns the managed @c AudioFile
	inline operator AudioFileID() const noexcept
	{
		return mAudioFile;
	}

	/// Returns the managed @c AudioFile
	inline AudioFileID get() const noexcept
	{
		return mAudioFile;
	}

	/// Closes the managed @c AudioFile and replaces it with @c audioFile
	inline void reset(AudioFileID audioFile = nullptr) noexcept
	{
		auto oldAudioFile = mAudioFile;
		mAudioFile = audioFile;
		if(oldAudioFile)
			AudioFileClose(oldAudioFile);
	}

	/// Swaps the managed @c AudioFile of @c *this and @c other
	inline void swap(AudioFileWrapper& other) noexcept
	{
		auto audioFile = mAudioFile;
		mAudioFile = other.mAudioFile;
		other.mAudioFile = audioFile;
	}

	/// Releases ownership of the managed @c AudioFile and returns it
	inline AudioFileID release() noexcept
	{
		auto oldAudioFile = mAudioFile;
		mAudioFile = nullptr;
		return oldAudioFile;
	}

private:

	/// The managed @c AudioFile
	AudioFileID mAudioFile;

};

} // namespace SFB
