//
// Copyright (c) 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

namespace SFB {

/// A class that calls a closure upon destruction.
///
/// This allows similar functionality to @c defer in Swift and Go.
/// This is useful to limit the lifetime of non-C++ objects and provides an alternative to @c std::unique_ptr with a
/// custom deleter.
///
/// @code
/// ExtAudioFileRef eaf;
/// auto result = ExtAudioFileOpenURL(url, &eaf);
/// assert(result == noErr);
/// auto lambda = [eaf]() {
///     auto result = ExtAudioFileDispose(eaf);
///     assert(result == noErr);
/// };
/// SFB::DeferredClosure<decltype(lambda)> cleanup(lambda);
/// @endcode
template <typename F>
class DeferredClosure
{

public:

	/// Creates a new @c DeferredClosure executing @c closure when the destructor is called
	inline DeferredClosure(const F& closure) noexcept
	: mClosure(closure)
	{}

	// This class is non-copyable
	DeferredClosure(const DeferredClosure& rhs) = delete;

	// This class is non-assignable
	DeferredClosure& operator=(const DeferredClosure& rhs) = delete;

	/// Executes the closure
	inline ~DeferredClosure()
	{
		mClosure();
	}

	// This class is non-movable
	DeferredClosure(const DeferredClosure&& rhs) = delete;

	// This class is non-move assignable
	DeferredClosure& operator=(const DeferredClosure&& rhs) = delete;

private:

	/// The closure to invoke upon object destruction
	const F& mClosure;

};

} // namespace SFB
