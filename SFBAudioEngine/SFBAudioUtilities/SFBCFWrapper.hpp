//
// Copyright (c) 2012 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioUtilities
// MIT license
//

#pragma once

#import <CFNetwork/CFNetwork.h>
#import <CoreFoundation/CoreFoundation.h>
#import <ImageIO/ImageIO.h>
#import <Security/Security.h>

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

namespace SFB {

/// A wrapper around a Core Foundation object.
///
/// @c CFWrapper simplifies the use of CFTypes in C++ by wrapping a CF object, ensuring
/// @c CFRelease will be called when the @c CFWrapper goes out of scope.
/// @tparam T A @c CFType
template <typename T>
class CFWrapper
{

public:

#pragma mark Creation and Destruction

	/// Creates a new @c CFWrapper
	inline CFWrapper() noexcept
	: CFWrapper(nullptr)
	{}

	/// Creates a new @c CFWrapper
	CFWrapper(const CFWrapper& rhs) noexcept
	: mObject(rhs.mObject), mRelease(rhs.mRelease)
	{
		if(mObject && mRelease)
			CFRetain(mObject);
	}

	/// Replaces the wrapped object
	CFWrapper& operator=(const CFWrapper& rhs) noexcept
	{
		if(mObject != rhs.mObject) {
			if(mObject && mRelease)
				CFRelease(mObject);

			mObject = rhs.mObject;
			mRelease = rhs.mRelease;

			if(mObject && mRelease)
				CFRetain(mObject);
		}

		return *this;
	}

	/// Destroys this @c CFWrapper and ensure @c CFRelease() is called if necessary
	~CFWrapper()
	{
		if(mObject && mRelease)
			CFRelease(mObject);
		mObject = nullptr;
	}

	/// Creates a new @c CFWrapper
	CFWrapper(CFWrapper&& rhs) noexcept
	: mObject(rhs.mObject), mRelease(rhs.mRelease)
	{
		rhs.mObject = nullptr;
	}

	/// Replaces the wrapped object
	CFWrapper& operator=(CFWrapper&& rhs) noexcept
	{
		if(mObject != rhs.mObject) {
			if(mObject && mRelease)
				CFRelease(mObject);

			mObject = rhs.mObject;
			mRelease = rhs.mRelease;

			rhs.mObject = nullptr;
		}

		return *this;
	}


	/// Create a new @c CFWrapper
	/// @note The @c CFWrapper takes ownership of @c object
	/// @param object The object to wrap
	inline explicit CFWrapper(T object) noexcept
	: CFWrapper(object, true)
	{}

	/// Creates a new @c CFWrapper
	/// @param object The object to wrap
	/// @param release Whether this @c CFWrapper should take ownership of @c object
	CFWrapper(T object, bool release) noexcept
	: mObject(object), mRelease(release)
	{}

#pragma mark Assignment

	/// Replaces the wrapped object
	/// @note The @c CFWrapper takes ownership of @c rhs
	/// @param rhs The object to wrap
	CFWrapper& operator=(const T& rhs) noexcept
	{
		if(mObject != rhs) {
			if(mObject && mRelease)
				CFRelease(mObject);

			mObject = rhs;
			mRelease = true;
		}

		return *this;
	}

#pragma mark Pointer management

	/// Relinquishes ownership of the wrapped object and returns it
	inline T Relinquish() noexcept
	{
		T object = mObject;
		mObject = nullptr;

		return object;
	}

#pragma mark Equality testing

	/// Tests two @c CFWrapper objects for equality using @c CFEqual()
	inline bool operator==(const CFWrapper& rhs) const noexcept
	{
		if(mObject == rhs.mObject)
			return true;

		// CFEqual doesn't handle nullptr
		if(!mObject || !rhs.mObject)
			return false;

		return CFEqual(mObject, rhs.mObject);
	}

	/// Tests two @c CFWrapper objects for inequality
	inline bool operator!=(const CFWrapper& rhs) const noexcept
	{
		return !operator==(rhs);
	}

#pragma mark Core Foundation object access

	/// Returns @c true if the wrapped object is not @c nullptr
	inline explicit operator bool() const noexcept
	{
		return mObject != nullptr;
	}

	/// Returns @c true if the wrapped object is @c nullptr
	inline bool operator!() const noexcept
	{
		return !operator bool();
	}

	/// Returns the wrapped object
	inline operator T() const noexcept
	{
		return mObject;
	}


	/// Returns a pointer to the wrapped object
	inline T * _Nonnull operator&() noexcept
	{
		return &mObject;
	}


	/// Returns the wrapped object
	inline T Object() const noexcept
	{
		return mObject;
	}

#pragma mark Core Foundation object creation

	/// Creates a new wrapped @c CFStringRef using @c CFStringCreateWithCString with the default allocator
	template <typename = std::enable_if<std::is_same<T, CFStringRef>::value>>
	CFWrapper(const char * _Nonnull cStr, CFStringEncoding encoding)
	: CFWrapper(CFStringCreateWithCString(kCFAllocatorDefault, cStr, encoding))
	{}

	/// Creates a new wrapped @c CFStringRef using @c CFStringCreateWithFormatAndArguments with the default allocator
	template <typename = std::enable_if<std::is_same<T, CFStringRef>::value>>
	CFWrapper(CFDictionaryRef _Nullable formatOptions, CFStringRef _Nonnull format, ...) CF_FORMAT_FUNCTION(3,4)
	: CFWrapper()
	{
		va_list ap;
		va_start(ap, format);
		*this = CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, formatOptions, format, ap);
		va_end(ap);
	}

	/// Creates a new wrapped @c CFNumberRef using @c CFNumberCreate with the default allocator
	template <typename = std::enable_if<std::is_same<T, CFNumberRef>::value>>
	CFWrapper(CFNumberType theType, const void * _Nonnull valuePtr)
	: CFWrapper(CFNumberCreate(kCFAllocatorDefault, theType, valuePtr))
	{}

	/// Creates a new wrapped @c CFArrayRef using @c CFArrayCreate with the default allocator
	template <typename = std::enable_if<std::is_same<T, CFArrayRef>::value>>
	CFWrapper(const void * _Nonnull * _Nullable values, CFIndex numValues, const CFArrayCallBacks * _Nonnull callBacks)
	: CFWrapper(CFArrayCreate(kCFAllocatorDefault, values, numValues, callBacks))
	{}

	/// Creates a new wrapped @c CFMutableArrayRef using @c CFArrayCreateMutable with the default allocator
	template <typename = std::enable_if<std::is_same<T, CFMutableArrayRef>::value>>
	CFWrapper(CFIndex capacity, const CFArrayCallBacks * _Nonnull callBacks)
	: CFWrapper(CFArrayCreateMutable(kCFAllocatorDefault, capacity, callBacks))
	{}

	/// Creates a new wrapped @c CFDictionaryRef using @c CFDictionaryCreate with the default allocator
	template <typename = std::enable_if<std::is_same<T, CFDictionaryRef>::value>>
	CFWrapper(const void * _Nonnull * _Nullable keys, const void * _Nonnull * _Nullable values, CFIndex numValues, const CFDictionaryKeyCallBacks * _Nonnull keyCallBacks, const CFDictionaryValueCallBacks * _Nonnull valueCallBacks)
	: CFWrapper(CFDictionaryCreate(kCFAllocatorDefault, keys, values, numValues, keyCallBacks, valueCallBacks))
	{}

	/// Creates a new wrapped @c CFMutableDictionaryRef using @c CFDictionaryCreateMutable with the default allocator
	template <typename = std::enable_if<std::is_same<T, CFMutableDictionaryRef>::value>>
	CFWrapper(CFIndex capacity, const CFDictionaryKeyCallBacks * _Nonnull keyCallBacks, const CFDictionaryValueCallBacks * _Nonnull valueCallBacks)
	: CFWrapper(CFDictionaryCreateMutable(kCFAllocatorDefault, capacity, keyCallBacks, valueCallBacks))
	{}

	/// Creates a new wrapped @c CFDataRef using @c CFDataCreate with the default allocator
	template <typename = std::enable_if<std::is_same<T, CFDataRef>::value>>
	CFWrapper(const UInt8 * _Nullable bytes, CFIndex length)
	: CFWrapper(CFDataCreate(kCFAllocatorDefault, bytes, length))
	{}

#ifdef __OBJC__

	/// Returns the wrapped string
	template <typename = std::enable_if<std::is_same<T, CFStringRef>::value>>
	inline NSString * _Nullable NSString() const noexcept
	{
		return (__bridge ::NSString *)mObject;
	}

	/// Returns the wrapped string
	template <typename = std::enable_if<std::is_same<T, CFStringRef>::value>>
	inline operator ::NSString * _Nullable () const noexcept
	{
		return (__bridge ::NSString *)mObject;
	}

	/// Returns the wrapped array
	template <typename = std::enable_if<std::is_same<T, CFArrayRef>::value>>
	inline NSArray * _Nullable NSArray() const noexcept
	{
		return (__bridge ::NSArray *)mObject;
	}

	/// Returns the wrapped array
	template <typename = std::enable_if<std::is_same<T, CFArrayRef>::value>>
	inline operator ::NSArray * _Nullable () const noexcept
	{
		return (__bridge ::NSArray *)mObject;
	}

	/// Returns the wrapped dictionary
	template <typename = std::enable_if<std::is_same<T, CFDictionaryRef>::value>>
	inline NSDictionary * _Nullable NSDictionary() const noexcept
	{
		return (__bridge ::NSDictionary *)mObject;
	}

	/// Returns the wrapped dictionary
	template <typename = std::enable_if<std::is_same<T, CFDictionaryRef>::value>>
	inline operator ::NSDictionary * _Nullable () const noexcept
	{
		return (__bridge ::NSDictionary *)mObject;
	}

#endif

private:

	/// The Core Foundation object
	T mObject;
	/// Whether @c CFRelease should be called on destruction or reassignment
	bool mRelease;

};

#pragma mark Typedefs for common CF types

/// A wrapped @c CFTypeRef
using CFType = CFWrapper<CFTypeRef>;
/// A wrapped @c CFDataRef
using CFData = CFWrapper<CFDataRef>;
/// A wrapped @c CFMutableDataRef
using CFMutableData = CFWrapper<CFMutableDataRef>;
/// A wrapped @c CFStringRef
using CFString = CFWrapper<CFStringRef>;
/// A wrapped @c CFMutableStringRef
using CFMutableString = CFWrapper<CFMutableStringRef>;
/// A wrapped @c CFAttributedStringRef
using CFAttributedString = CFWrapper<CFAttributedStringRef>;
/// A wrapped @c CFMutableAttributedStringRef
using CFMutableAttributedString = CFWrapper<CFMutableAttributedStringRef>;
/// A wrapped @c CFDictionaryRef
using CFDictionary = CFWrapper<CFDictionaryRef>;
/// A wrapped @c CFMutableDictionaryRef
using CFMutableDictionary = CFWrapper<CFMutableDictionaryRef>;
/// A wrapped @c CFArrayRef
using CFArray = CFWrapper<CFArrayRef>;
/// A wrapped @c CFMutableArrayRef
using CFMutableArray = CFWrapper<CFMutableArrayRef>;
/// A wrapped @c CFSetRef
using CFSet = CFWrapper<CFSetRef>;
/// A wrapped @c CFMutableSetRef
using CFMutableSet = CFWrapper<CFMutableSetRef>;
/// A wrapped @c CFBagRef
using CFBag = CFWrapper<CFBagRef>;
/// A wrapped @c CFMutableBagRef
using CFMutableBag = CFWrapper<CFMutableBagRef>;
/// A wrapped @c CFPropertyListRef
using CFPropertyList = CFWrapper<CFPropertyListRef>;
/// A wrapped @c CFBitVectorRef
using CFBitVector = CFWrapper<CFBitVectorRef>;
/// A wrapped @c CFMutableBitVectorRef
using CFMutableBitVector = CFWrapper<CFMutableBitVectorRef>;
/// A wrapped @c CFCharacterSetRef
using CFCharacterSet = CFWrapper<CFCharacterSetRef>;
/// A wrapped @c CFMutableCharacterSetRef
using CFMutableCharacterSet = CFWrapper<CFMutableCharacterSetRef>;
/// A wrapped @c CFURLRef
using CFURL = CFWrapper<CFURLRef>;
/// A wrapped @c CFUUIDRef
using CFUUID = CFWrapper<CFUUIDRef>;
/// A wrapped @c CFNumberRef
using CFNumber = CFWrapper<CFNumberRef>;
/// A wrapped @c CFBooleanRef
using CFBoolean = CFWrapper<CFBooleanRef>;
/// A wrapped @c CFErrorRef
using CFError = CFWrapper<CFErrorRef>;
/// A wrapped @c CFDateRef
using CFDate = CFWrapper<CFDateRef>;
/// A wrapped @c CFReadStreamRef
using CFReadStream = CFWrapper<CFReadStreamRef>;
/// A wrapped @c CFWriteStreamRef
using CFWriteStream = CFWrapper<CFWriteStreamRef>;
/// A wrapped @c CFHTTPMessageRef
using CFHTTPMessage = CFWrapper<CFHTTPMessageRef>;
/// A wrapped @c CGImageSourceRef
using CGImageSource = CFWrapper<CGImageSourceRef>;
/// A wrapped @c SecCertificateRef
using SecCertificate = CFWrapper<SecCertificateRef>;
#if !TARGET_OS_IPHONE
/// A wrapped @c SecKeychainItemRef
using SecKeychainItem = CFWrapper<SecKeychainItemRef>;
/// A wrapped @c SecTransformRef
using SecTransform = CFWrapper<SecTransformRef>;
#endif

} // namespace SFB
