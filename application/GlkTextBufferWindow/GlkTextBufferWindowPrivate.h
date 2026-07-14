#import "GlkTextBufferWindow.h"

#import "GlkTextBufferWindow+Autorestore.h"
#import "GlkTextBufferWindow+Hyperlinks.h"
#import "GlkTextBufferWindow+Images.h"
#import "GlkTextBufferWindow+Input.h"
#import "GlkTextBufferWindow+Output.h"
#import "GlkTextBufferWindow+Scrolling.h"
#import "GlkTextBufferWindow+Speech.h"
#import "GlkTextBufferWindow+Styles.h"

@class MarginContainer;

NS_ASSUME_NONNULL_BEGIN

/*
 * Class extension shared by GlkTextBufferWindow.m and its category files
 * (+Autorestore, +Hyperlinks, +Images, +Input, +Output, +Scrolling, +Speech,
 * +Styles). Not for use outside the GlkTextBufferWindow implementation.
 */

@interface GlkTextBufferWindow () <NSSecureCoding, NSTextViewDelegate, NSTextStorageDelegate> {
    // --- Text system components ---
    NSScrollView *scrollview;           // Scroll view wrapping the text view
    NSLayoutManager *layoutmanager;     // Manages glyph layout for the text storage
    MarginContainer *container;         // Custom text container supporting margin images and flow breaks
    NSTextStorage *textstorage;         // The live text storage displayed in the text view
    NSMutableAttributedString *bufferTextstorage; // Buffer for text not yet committed to textstorage

    // --- Input state ---
    BOOL line_request;                  // YES when the interpreter is waiting for a line of text
    BOOL hyper_request;                 // YES when the interpreter is waiting for a hyperlink click

    BOOL echo_toggle_pending;           // If YES, line echo behavior will be inverted
                                        // starting from the next line event
    BOOL echo;                          // If NO, line input text will be deleted when entered

    NSUInteger fence;                   // Character index marking the start of editable input.
                                        // Text before the fence is game output and cannot be edited.

    NSRange pendingEchoDeleteRange;     // When echo is off and the player submits a line, the
                                        // typed text is left in place and this range records what
                                        // to delete. The deletion is then coalesced with the next
                                        // output append into a single NSTextStorage edit pass so
                                        // there is no intermediate paint where the input has
                                        // vanished but the game's reprint hasn't arrived yet.

    CGFloat lastLineheight;             // Used to restore scroll position with sub-line precision

    NSAttributedString *storedNewline;  // Deferred trailing newline to avoid blank lines at bottom

    // --- Scroll position persistence ---
    NSUInteger lastVisible;             // Character index of last visible character before resize
    CGFloat lastScrollOffset;           // Sub-line scroll offset (fraction of cell height)
    BOOL lastAtBottom;                  // Was scrolled to bottom before resize
    BOOL lastAtTop;                     // Was scrolled to top before resize
    NSUInteger rewrapLastseenChar;      // Character under the _lastseen y-coordinate when a frame
                                        // width change invalidated the layout; restoreScroll uses it
                                        // to recompute _lastseen in the new layout. NSNotFound when
                                        // no remap is pending.
    BOOL rewrapFollowupScroll;          // A deferred auto-scroll was pending when the rewrap hit;
                                        // re-run it once the restore has made coordinates valid again

    BOOL pauseScrolling;               // Temporarily pause auto-scrolling (during command scripts)
    BOOL commandScriptWasRunning;      // Track command script transitions

    BOOL scrolling;                    // YES during animated scroll to prevent re-entry
    BOOL scrollToBottomPending;        // YES when a scrollToBottomAnimated callback is queued
    NSDate *scrollAdjustTimeStamp;
    CGFloat preScrollPosition;
    CGFloat docHeightAtLastSeen;       // NSHeight(_textview.frame) at the moment markLastSeen
                                       // was last called. When the whole doc fit in the viewport
                                       // at the keypress, the auto-scroll caps target
                                       // (docHeightAtLastSeen - lineHeight) — i.e. the last line
                                       // of the on-screen content sits at the top of the new
                                       // viewport, with the game's response stacked below. 0
                                       // means "not captured" (auto-scroll falls back to the
                                       // existing scroll-to-bottom behavior).

    NSMutableArray<NSEvent *> *bufferedEvents; // Key events received when no input request is active

    // --- VoiceOver speech tracking ---
    NSRange lastSpokenRange;           // Range of text last spoken to avoid repeating
    NSString *lastSpokenString;        // Text content last spoken

    // --- Background layout ---
    BOOL backgroundLayoutInProgress;   // YES while a background layout pass is running
    NSUInteger backgroundLayoutGeneration; // Incremented when text changes, to discard stale results

    // --- Backing ivars for properties declared in GlkTextBufferWindow.h ---
    // Declared explicitly here (auto-synthesis adopts them) so the category
    // files, which are separate translation units, can access them directly.
    BufferTextView *_textview;
    unichar _lastchar;
    NSInteger _lastseen;
    NSUInteger _printPositionOnInput;
    NSUInteger _restoredLastVisible;
    CGFloat _restoredScrollOffset;
    BOOL _restoredAtBottom;
    BOOL _restoredAtTop;
    NSRange _restoredSelection;
    NSString *_restoredSearch;
    BOOL _restoredFindBarVisible;
    BOOL _pendingScroll;
    BOOL _pendingClear;
    BOOL _pendingScrollRestore;
    NSAttributedString *_restoredInput;
    NSDictionary *_inputAttributes;
    GlkTextGridWindow *_quoteBox;
    NSInteger _lastNewTextOnTurn;
}

// Shared helpers implemented in GlkTextBufferWindow.m proper.
- (BOOL)coalescePendingEchoDeleteWithAppend:(NSAttributedString *)toAppend;
- (BOOL)applyPendingEchoDeleteIfValid;
- (void)configureBufferTextView:(BufferTextView *)textView;

@end

NS_ASSUME_NONNULL_END
