/* slow_draw_page.h -- shared "slow draw" (progressive reveal) machinery for the
 * Graphics Magician picture renderers.
 *
 * Every graphics_magician* renderer (Apple II, CGA, DHGR, PCjr) exposes the same
 * slow-draw contract: while recording is on, each framebuffer byte the renderer
 * plots is appended to an ordered op list as well as applied to the final page;
 * the host then replays those ops onto a separate *visible* page a chunk at a
 * time on a Glk timer, re-blitting after each chunk, so a picture is revealed in
 * the order it was painted -- the few seconds the real interpreter took to fill
 * its video page.  op13 emits a DELAY marker that pauses the reveal.
 *
 * The reveal STATE MACHINE -- the chunked advance, DELAY/pause handling, the
 * draw cursor and the dirty-row band -- was duplicated, near byte-for-byte, in
 * all four renderers; it lives here exactly once, in SlowDrawEngine.  What
 * genuinely differs between renderers is only the per-op *leaf* behaviour:
 *
 *   - how a recorded op is applied to the visible page(s)  (flat single page vs.
 *     the DHGR main/aux page pair, with the Coveted-Mirror panel columns skipped
 *     during a reveal),
 *   - how a byte offset maps back to a screen row for the dirty band  (flat
 *     offset/stride vs. the Apple II hi-res de-interleave table), and
 *   - the run-adjacency test  (DHGR additionally requires the same page).
 *
 * A renderer supplies those three things as a small Policy struct and drives the
 * engine; renderers whose page is a single flat buffer with offset/stride rows
 * (CGA, PCjr) can use the ready-made SlowDrawPage wrapper and need no policy.
 *
 * Header-only with inline / template methods so each renderer keeps compiling as
 * a single self-contained translation unit (the offline test harnesses link one
 * renderer .cpp and nothing else).
 */

#ifndef GLK_COMPREHEND_SLOW_DRAW_PAGE_H
#define GLK_COMPREHEND_SLOW_DRAW_PAGE_H

#include <cstdint>
#include <cstring>
#include <vector>

namespace Glk {
namespace Comprehend {

// One recorded framebuffer byte write.  An offset of SlowDrawEngine::kDelayMarker
// is not a write but an op13 DELAY whose `value` is the unit count.  `page` tags
// which buffer the byte belongs to for multi-page renderers (DHGR main=1/aux=0);
// single-page renderers leave it 0.
struct SlowWriteOp { uint16_t offset; uint8_t page; uint8_t value; };

// The progressive-reveal state machine.  `Policy` (held by reference, so it may
// carry buffer pointers / renderer state) must provide:
//
//   bool apply(const SlowWriteOp &op);
//       Write op.value into the visible page it belongs to.  Return false to
//       skip it (e.g. a DHGR panel column), which also suppresses dirty marking.
//   int  rowOf(const SlowWriteOp &op);
//       The screen row this op touches, for the dirty band, or < 0 to skip
//       (address-space gaps between hi-res rows).
//   bool adjacent(const SlowWriteOp &a, const SlowWriteOp &b);
//       Whether b continues a's visual run (revealed together to avoid seams).
//   void sync();
//       Copy the final page(s) onto the visible page(s) (used when not animating
//       so a later overlay composes onto the right background).
template <class Policy>
class SlowDrawEngine {
public:
    // A WriteOp with this (out-of-page) offset is a DELAY marker, not a byte.
    static const uint16_t kDelayMarker = 0xffff;
    // Wall-clock length of one op13 delay unit, in slow-draw ticks; kept equal
    // across renderers so all titles animate at a comparable speed at the host's
    // 10 ms tick (GM_SLOW_TICK_MS).
    static const int kDelayTicksPerUnit = 10;

    explicit SlowDrawEngine(Policy &policy) : _p(policy) {}

    void setRecording(bool on) { _recording = on; }
    bool recording() const { return _recording; }

    // Log a byte the renderer just stored into the final page (no-op unless
    // recording).
    void record(uint16_t off, uint8_t page, uint8_t value) {
        if (_recording)
            _ops.push_back({ off, page, value });
    }

    // Log an op13 DELAY of `units` (no-op unless recording, or units == 0).
    void recordDelay(uint8_t units) {
        if (_recording && units)
            _ops.push_back({ kDelayMarker, 0, units });
    }

    // Drop the current image's op list: a fresh page (reset, or a new image)
    // starts a new reveal.  Does NOT touch the page buffers.
    void clear() {
        _ops.clear();
        _opsDrawn = 0;
        _pauseTicks = 0;
    }

    // When not animating, keep the visible page(s) in step with the final
    // page(s) so a later slow-drawn overlay composes onto the right background.
    void syncIfNotRecording() {
        if (!_recording)
            _p.sync();
    }

    // True while there is reveal work left: ops still to paint, or a DELAY pause
    // still owed.
    bool active() const {
        return _recording && (_opsDrawn < _ops.size() || _pauseTicks > 0);
    }

    // Apply up to `budget` recorded ops onto the visible page(s), extending the
    // chunk across adjacent runs.  A DELAY marker halts this tick's reveal and
    // owes a run of pause ticks.  Returns true while any reveal work -- ops or an
    // outstanding pause -- remains.
    bool advance(int budget) {
        if (_pauseTicks > 0) {
            _pauseTicks--;
            return _opsDrawn < _ops.size() || _pauseTicks > 0;
        }
        size_t i = _opsDrawn;
        size_t chunk_end = i + (budget > 0 ? (size_t)budget : 1);
        bool keep_going = false;
        for (; i < _ops.size() && (i < chunk_end || keep_going); i++) {
            if (_ops[i].offset == kDelayMarker) {
                _pauseTicks += _ops[i].value * kDelayTicksPerUnit;
                _opsDrawn = i + 1;            // consume the marker
                return _opsDrawn < _ops.size() || _pauseTicks > 0;
            }
            applyOne(_ops[i]);
            keep_going = (i + 1 < _ops.size()) && _ops[i + 1].offset != kDelayMarker &&
                         _p.adjacent(_ops[i], _ops[i + 1]);
        }
        _opsDrawn = i;
        return _opsDrawn < _ops.size();
    }

    // Reveal everything left at once (resize / cancel).  Leaves the visible
    // page(s) identical to the final page(s).
    void finish() {
        for (; _opsDrawn < _ops.size(); _opsDrawn++) {
            if (_ops[_opsDrawn].offset == kDelayMarker)
                continue;                    // pauses collapse when finishing
            applyOne(_ops[_opsDrawn]);
        }
        _pauseTicks = 0;
    }

    // Hand back the row band touched since the last call (inclusive) and reset
    // it.  Returns false if nothing changed.
    bool consumeDirty(int *y0, int *y1) {
        if (_dirtyMax < 0)
            return false;
        *y0 = _dirtyMin;
        *y1 = _dirtyMax;
        _dirtyMin = 0x7fffffff;
        _dirtyMax = -1;
        return true;
    }

private:
    void applyOne(const SlowWriteOp &op) {
        if (!_p.apply(op))                   // policy skipped this byte
            return;
        const int r = _p.rowOf(op);
        if (r < 0)
            return;
        if (r < _dirtyMin) _dirtyMin = r;
        if (r > _dirtyMax) _dirtyMax = r;
    }

    Policy &_p;
    std::vector<SlowWriteOp> _ops;     // ordered byte writes of this image
    size_t _opsDrawn = 0;              // how many ops are on the visible page(s)
    bool _recording = false;          // record ops for slow reveal?
    int _pauseTicks = 0;              // reveal ticks still owed to a DELAY
    int _dirtyMin = 0x7fffffff;       // changed row band, low ...
    int _dirtyMax = -1;              // ... and high
};

// ---- Flat single-page convenience -------------------------------------------
//
// For renderers whose framebuffer is one flat byte buffer with constant row
// stride (CGA, PCjr): wraps a SlowDrawEngine with a built-in flat policy, so the
// renderer only passes its two page buffers, size and stride.  The two pages are
// owned by the renderer (their pixel layout is model-specific); this only
// records, reveals, and tracks the dirty band.  record() takes a 2-arg form
// since there is only one page.
class SlowDrawPage {
    struct FlatPolicy {
        uint8_t *finalPage;
        uint8_t *slowPage;
        int size;
        int stride;
        bool apply(const SlowWriteOp &op) { slowPage[op.offset] = op.value; return true; }
        int rowOf(const SlowWriteOp &op) const { return (int)(op.offset / stride); }
        // Bytes whose offset is adjacent to, or whose value matches, the previous
        // one belong to the same visual run; revealing them together avoids seams.
        bool adjacent(const SlowWriteOp &a, const SlowWriteOp &b) const {
            return ((int)b.offset >= (int)a.offset - 1 && (int)b.offset <= (int)a.offset + 1) ||
                   b.value == a.value;
        }
        void sync() { std::memcpy(slowPage, finalPage, (size_t)size); }
    };

public:
    // finalPage = the fully-drawn page the renderer paints into;
    // slowPage  = the progressively-revealed page the host blits;
    // size      = bytes in each page;  stride = bytes per row (for dirty rows).
    SlowDrawPage(uint8_t *finalPage, uint8_t *slowPage, int size, int stride)
        : _pol{ finalPage, slowPage, size, stride }, _eng(_pol) {}

    SlowDrawPage(const SlowDrawPage &) = delete;            // _eng holds &_pol
    SlowDrawPage &operator=(const SlowDrawPage &) = delete;

    void setRecording(bool on) { _eng.setRecording(on); }
    bool recording() const { return _eng.recording(); }
    void record(int off, uint8_t value) { _eng.record((uint16_t)off, 0, value); }
    void recordDelay(uint8_t units) { _eng.recordDelay(units); }
    void clear() { _eng.clear(); }
    void syncIfNotRecording() { _eng.syncIfNotRecording(); }
    bool active() const { return _eng.active(); }
    bool advance(int budget) { return _eng.advance(budget); }
    void finish() { _eng.finish(); }
    bool consumeDirty(int *y0, int *y1) { return _eng.consumeDirty(y0, y1); }

private:
    FlatPolicy _pol;
    SlowDrawEngine<FlatPolicy> _eng;
};

} // namespace Comprehend
} // namespace Glk

#endif
