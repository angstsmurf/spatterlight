/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <timidity/timidity.h>

#import "AudioDecoder.h"

namespace SFB {

	namespace Audio {

		// ==========================================================
		// A Decoder subclass supporting MIDI files
		// ==========================================================
		class MIDIDecoder : public Decoder
		{

		public:

			// Data types handled by this class
			static CFArrayRef CreateSupportedFileExtensions();
			static CFArrayRef CreateSupportedMIMETypes();

			static bool HandlesFilesWithExtension(CFStringRef extension);
			static bool HandlesMIMEType(CFStringRef mimeType);

			static Decoder::unique_ptr CreateDecoder(InputSource::unique_ptr inputSource);

			// Creation
			explicit MIDIDecoder(InputSource::unique_ptr inputSource);

		private:

			// Audio access
			virtual bool _Open(CFErrorRef *error);
			virtual bool _Close(CFErrorRef *error);

			// The native format of the source audio
			virtual SFB::CFString _GetSourceFormatDescription() const;

			// Attempt to read frameCount frames of audio, returning the actual number of frames read
			virtual UInt32 _ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);

			// Source audio information
			inline virtual SInt64 _GetTotalFrames() const			{ return mTotalFrames; }
			inline virtual SInt64 _GetCurrentFrame() const			{ return mCurrentFrame; }

			// Seeking support
			inline virtual bool _SupportsSeeking() const			{ return mInputSource->SupportsSeeking(); }
			virtual SInt64 _SeekToFrame(SInt64 frame);

			using unique_MidIStream_ptr = std::unique_ptr<MidIStream, int(*)(MidIStream *)>;
			using unique_MidSong_ptr = std::unique_ptr<MidSong, void(*)(MidSong *)>;

			// Data members

            unique_MidIStream_ptr               stream;
            unique_MidSong_ptr                  song;
            MidSongOptions                      options;
			SInt64								mTotalFrames;
			SInt64								mCurrentFrame;
		};

	}
}
