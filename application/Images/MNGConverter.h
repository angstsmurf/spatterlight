//
//  MNGConverter.h
//  Spatterlight
//
//  MNG, the animated PNG format of HTML TADS, rewritten as an animated GIF,
//  which ImageAnimation already plays. Only what the games in the HTML TADS
//  corpus use is read: whole PNG frames placed by DEFI on an MHDR canvas,
//  with FRAM delays, LOOP/ENDL and TERM. No delta images, no JNG.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Whether data starts with the MNG signature.
BOOL MNGIsMNGData(NSData *data);

// The frames of an MNG as a GIF, or nil if not even its first frame can be
// decoded. A chunk this does not read ends the animation where it is, so a
// file with one still shows its first frame.
NSData * _Nullable MNGConvertToGIF(NSData *mng);

NS_ASSUME_NONNULL_END
