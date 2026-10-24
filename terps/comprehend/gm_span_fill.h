/* gm_span_fill.h -- the Graphics Magician op14 flood fill, shared by the
 * Apple II standard hi-res, double hi-res and DOS CGA renderers.
 *
 * op14 fills the connected region of "set" (white) pixels reachable from a seed,
 * replacing them with the current fill-colour dither pattern and stopping at clear
 * (black) pixels and the clip rectangle.
 *
 * It is a queue-based span fill: painting one horizontal run of pixels spawns
 * child runs on the rows immediately above and below it, which are processed in
 * turn from a 32-entry circular queue. A merge step folds together runs that an
 * opposite-direction span has already claimed so no pixel is painted twice.
 * Painting modifies the very screen bits that later boundary tests read, so the
 * traversal/merge order is part of the algorithm and is preserved here. The 6502
 * and the 8086 interpreters walk the queue identically; they differ in how a row
 * is scanned and painted, and in a few edge rules, which the Geom policy supplies.
 *
 * A pixel position is an ordinary integer along the row (column*7 + bit on the
 * Apple II, a global pixel column on CGA): comparisons are <, == and stepping one
 * pixel is ++/--. Rows are int while the cursor moves (CGA probes row -1 above
 * row 0, which reads black) and are stored and compared as uint8_t, as both
 * originals do.
 *
 * GmSpanFill<Geom> is the traversal. Geom provides, for the row last selected:
 *
 *   void setRow(int row);                           // select the row to address
 *   bool isSet(int pos);                            // is the pixel at pos white?
 *   void scanPaint(int pos, int &left, int &right); // paint the white run through
 *                                                   //  pos; return its inclusive edges
 *   bool acceptSpan(uint8_t row, int left, int right, uint8_t dir);
 *                                                   // queue this span? (clip rows,
 *                                                   //  DHGR single-pixel rule)
 *   void notePush(uint8_t row, int left, int right, uint8_t dir,
 *                 uint8_t head, uint8_t tail);      // trace hook, before acceptSpan
 *   static const bool kAbortOnOverflow;             // CGA stops the whole fill when
 *                                                   //  the queue fills; the 6502
 *                                                   //  just wraps
 *
 * GmHiresSpanFill<Page> binds it to the Apple II 7-pixel byte geometry; the CGA
 * renderer has its own policy in graphics_magician_cga.cpp.
 *
 * Header-only, in the style of graphics_magician_vector.h and slow_draw_page.h.
 */

#ifndef GLK_COMPREHEND_GM_SPAN_FILL_H
#define GLK_COMPREHEND_GM_SPAN_FILL_H

#include <cstdint>

namespace Glk {
namespace Comprehend {

// Direction of a queued span: the row it continues onto.
enum {
	GM_SPAN_UP = 0,           // child run continues onto the row above
	GM_SPAN_DOWN = 1,         // child run continues onto the row below
	GM_SPAN_DISABLED = 0xff   // span was absorbed by a merge; skip it
};

template <typename Geom>
class GmSpanFill {
public:
	explicit GmSpanFill(Geom &geom) : _geom(geom) {}

	// Paint the seed run through (pos, row), queue its up/down children, then
	// drain the queue.
	void fill(int pos, int row) {
		_row = row;
		_pos = pos;
		_geom.setRow(_row);
		if (!_geom.isSet(_pos))
			return;
		int left, right;
		_geom.scanPaint(_pos, left, right);

		_head = _tail = 0;
		push(left, right, GM_SPAN_UP);
		push(left, right, GM_SPAN_DOWN);

		long guard = 0;
		while (_head != _tail) {
			if (++guard > kMaxSteps)       // safety net against pathological input
				return;
			if (_queue[_head].dir != GM_SPAN_DISABLED && walkParent() == STEP_ABORT)
				return;
			_head = next(_head);
		}
	}

private:
	enum { QUEUE_SIZE = 32, QUEUE_MASK = 0x1f };
	enum { kMaxSteps = 2000000 };   // queue entries processed before giving up

	struct Span {
		int     left, right;   // inclusive pixel-coordinate edges
		uint8_t row;
		uint8_t dir;
	};

	// What the walk over a parent span does after a white pixel is handled.
	enum Step {
		STEP_ADVANCE,   // step the cursor on and keep walking
		STEP_DONE,      // the parent entry is finished
		STEP_NEWSPAN,   // (merge only) nothing queued covers the pixel: paint a run
		STEP_ABORT      // the queue overflowed (kAbortOnOverflow): stop the fill
	};

	Geom   &_geom;
	Span    _queue[QUEUE_SIZE];
	uint8_t _head, _tail;          // circular-queue head / tail indices
	int     _row;                  // current row
	int     _pos;                  // current pixel position

	static uint8_t next(uint8_t i) { return static_cast<uint8_t>((i + 1) & QUEUE_MASK); }

	// Append a span of the current row going `dir`, unless the policy rejects it.
	// Returns false when the queue overflowed and the policy aborts on that.
	bool push(int left, int right, uint8_t dir) {
		const uint8_t row = static_cast<uint8_t>(_row);
		_geom.notePush(row, left, right, dir, _head, _tail);
		if (!_geom.acceptSpan(row, left, right, dir))
			return true;
		Span &s = _queue[_tail];
		s.left = left;
		s.right = right;
		s.row = row;
		s.dir = dir;
		_tail = next(_tail);
		return !(Geom::kAbortOnOverflow && _tail == _head);
	}

	// Paint the run at the current position and queue its follow-ups: a same-
	// direction continuation covering the whole run, plus opposite-direction spans
	// for any part of the run overhanging the parent span on the left or right.
	Step paintRun(const Span &parent) {
		int left, right;
		_geom.scanPaint(_pos, left, right);
		if (!push(left, right, parent.dir))
			return STEP_ABORT;

		// Left overhang: the part of the run reaching left of the parent's left edge
		// is re-scanned in the opposite vertical direction.
		if (left < parent.left - 1)
			if (!push(left, parent.left - 2, parent.dir ^ 1))
				return STEP_ABORT;

		// Right overhang, likewise. If there is one we are finished with this run; if
		// the run ended inside the parent we keep walking from its right edge.
		_pos = right;
		if (parent.right + 1 < right) {
			if (!push(parent.right + 2, right, parent.dir ^ 1))
				return STEP_ABORT;
			return STEP_DONE;
		}
		return parent.right + 1 == right ? STEP_DONE : STEP_ADVANCE;
	}

	// Handle a white pixel found while walking the parent span. If an opposite-
	// direction span already queued covers this run, merge with it instead of
	// painting twice; otherwise tell the caller to paint the run.
	Step merge(const Span &parent) {
		const uint8_t row = static_cast<uint8_t>(_row);
		for (uint8_t i = next(_head); i != _tail; i = next(i)) {
			Span &other = _queue[i];
			if (other.row != row)        continue;
			if (other.dir == parent.dir) continue;
			if (_pos < other.left || _pos > other.right)
				continue;                              // run not inside `other`'s extent

			// Extend `other` left to cover the parent run reaching past its left edge.
			if (_pos != other.left && other.left < parent.left - 1)
				if (!push(other.left, parent.left - 2, other.dir))
					return STEP_ABORT;

			if (other.right <= parent.right) {
				// `other` lies entirely within the parent run: absorb it and keep
				// walking from its right edge.
				other.dir = GM_SPAN_DISABLED;
				_pos = other.right;
				return STEP_ADVANCE;
			}

			// `other` extends past the parent run: trim its left edge to just past
			// the parent's right edge and stop (it will be painted on its own turn).
			other.left = parent.right + 1;
			return STEP_DONE;
		}
		return STEP_NEWSPAN;
	}

	// Process the queue head: walk the parent span on the adjacent row, painting
	// (or merging) every connected run of white pixels it overlaps. The parent is
	// copied because a wrapped queue (no abort) may overwrite its slot.
	Step walkParent() {
		const Span parent = _queue[_head];
		_row = (parent.dir == GM_SPAN_UP) ? parent.row - 1 : parent.row + 1;
		_pos = parent.left;
		_geom.setRow(_row);

		for (;;) {
			if (_geom.isSet(_pos)) {
				Step step = merge(parent);
				if (step == STEP_NEWSPAN)
					step = paintRun(parent);
				if (step != STEP_ADVANCE)
					return step;
			}
			if (_pos == parent.right)
				return STEP_DONE;
			_pos++;
		}
	}
};

/* The Apple II geometry. A pixel position is column*7 + bit, where `column`
 * selects the hi-res byte (each byte holds 7 visible pixels) and `bit` is 0..6
 * within it. Stepping left of column 0 yields a negative index, which correctly
 * sorts below every on-screen pixel (matching the signed-byte boundary tests of
 * the 6502 original) and is never used to address the screen.
 *
 * The standard and double hi-res renderers differ only in how a column of the
 * current row is read, written and patterned, which the Page policy supplies:
 *
 *   void    setRow(uint8_t row);        // select the row the calls below address
 *   uint8_t read(int col) const;        // screen byte at `col`
 *   void    write(int col, uint8_t v);  // store a screen byte at `col`
 *   uint8_t pattern(int col) const;     // fill-pattern byte for `col`
 *   static const bool kDropSinglePixelSpans;
 *
 * kDropSinglePixelSpans is a real behavioural difference, not a tuning knob: the
 * double hi-res interpreter's span fill ($0F50) rejects a span whose left and
 * right edges coincide, and the standard hi-res one does not.
 */
template <typename Page>
class GmHiresSpanGeom {
public:
	static const bool kAbortOnOverflow = false;   // never expected to overflow 32 spans

	explicit GmHiresSpanGeom(Page &page) : _page(page) {}

	// Clip rectangle: left/right in screen columns, top/bottom in rows.
	void setClip(uint8_t clipLeft, uint8_t clipRight, uint8_t clipTop, uint8_t clipBottom) {
		_clipLeft = clipLeft;
		_clipRight = clipRight;
		_clipTop = clipTop;
		_clipBottom = clipBottom;
	}

	void setRow(int row) { _page.setRow(static_cast<uint8_t>(row)); }

	bool isSet(int pos) const {
		return (bitMask(bitOf(pos)) & _page.read(colOf(pos))) != 0;
	}

	// Paint the maximal run of set pixels through the seed `pos` on the current
	// row, bounded by clear pixels or the clip edges.
	void scanPaint(int pos, int &left, int &right) const {
		int col = colOf(pos);
		const uint8_t seed_bit = static_cast<uint8_t>(bitOf(pos));

		// The boundary bits the two scans find. 0xff / 7 mean "none yet": the run
		// continues into the next column.
		uint8_t left_bit = 0xff;
		uint8_t bit = 7;

		// Seed byte: intersect the right-edge and left-edge masks so we paint only
		// the run that actually touches the seed bit.
		uint8_t existing = _page.read(col);
		uint8_t right_mask = rightEdgeMask(static_cast<uint8_t>(existing | rightMask(seed_bit)), left_bit);
		uint8_t left_input = static_cast<uint8_t>(leftMask(seed_bit) | existing);
		uint8_t mask = static_cast<uint8_t>(leftEdgeMask(left_input, bit) & right_mask);

		// Walk left across whole columns, painting each, until a right-edge boundary
		// turns up or we reach the clip edge (left_bit stays 0xff).
		for (;;) {
			blendByte(mask, col);
			if (left_bit != 0xff) break;
			if (--col < _clipLeft) { col++; break; }
			mask = rightEdgeMask(_page.read(col), left_bit);
		}

		// The left edge sits one pixel past the boundary bit.
		uint8_t edge_bit = left_bit + 1;
		if (edge_bit == 7) { edge_bit = 0; col++; }
		left = toPos(col, edge_bit);

		// Walk right from the seed, painting whole columns, until the run ends (the
		// bit index runs off the left of a column with no continuation) or we reach
		// the clip edge.
		col = colOf(pos);
		for (;;) {
			if (bit == 7 && col != _clipRight) {
				col++;
				blendByte(leftEdgeMask(_page.read(col), bit), col);
				continue;
			}
			bit--;
			if (bit & 0x80) { bit = 6; col--; }
			right = toPos(col, bit);
			return;
		}
	}

	// Drop a span on the clip edge in its direction, and single-pixel spans where
	// the page says so.
	bool acceptSpan(uint8_t row, int left, int right, uint8_t dir) const {
		if (Page::kDropSinglePixelSpans && left == right)
			return false;
		return row != ((dir == GM_SPAN_UP) ? _clipTop : _clipBottom);
	}

	void notePush(uint8_t, int, int, uint8_t, uint8_t, uint8_t) const {}

private:
	enum { BITS_PER_COL = 7 };

	Page   &_page;
	uint8_t _clipLeft, _clipRight, _clipTop, _clipBottom;

	static uint8_t bitMask(int i) {
		static const uint8_t t[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
		return t[i];
	}
	static uint8_t leftMask(int i) {
		static const uint8_t t[7] = {0x80, 0x81, 0x83, 0x87, 0x8f, 0x9f, 0xbf};
		return t[i];
	}
	static uint8_t rightMask(int i) {
		static const uint8_t t[7] = {0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};
		return t[i];
	}

	static int colOf(int pos) { return pos / BITS_PER_COL; }
	static int bitOf(int pos) { return pos % BITS_PER_COL; }
	static int toPos(int col, int bit) { return col * BITS_PER_COL + bit; }

	// Locate the right / left edge of a run of set bits within byte `p`, returning
	// the paint mask and storing the boundary bit index in `edge`. When all seven
	// bits are set the run continues past this byte (no edge here): return 0xff
	// and leave `edge` alone, as the 6502 leaves its zero-page copy.
	static uint8_t rightEdgeMask(uint8_t p, uint8_t &edge) {
		if ((p & 0x7f) == 0x7f)
			return 0xff;
		uint8_t i = 7;
		uint8_t bit;
		do {
			i--;
			bit = static_cast<uint8_t>(p & 0x7f);
			p = static_cast<uint8_t>(p << 1);
		} while (bit >> 6);
		edge = i;
		return rightMask(i);
	}
	static uint8_t leftEdgeMask(uint8_t p, uint8_t &edge) {
		p &= 0x7f;
		if (p == 0x7f)
			return 0xff;
		uint8_t i = 0xff;
		uint8_t bit;
		do {
			i++;
			bit = static_cast<uint8_t>(p & 1);
			p = static_cast<uint8_t>(p >> 1);
		} while (bit);
		edge = i;
		return leftMask(i);
	}

	// Blend `mask` worth of the current fill pattern into screen column `col`,
	// keeping the screen's existing pixels wherever the mask is clear.
	void blendByte(uint8_t mask, int col) const {
		uint8_t kept = static_cast<uint8_t>((mask ^ _page.read(col)) & 0x7f);
		uint8_t pattern = _page.pattern(col);
		_page.write(col, static_cast<uint8_t>((pattern & mask) | kept));
	}
};

// The engine bound to an Apple II page, with the clip rectangle passed per fill.
template <typename Page>
class GmHiresSpanFill {
public:
	explicit GmHiresSpanFill(Page &page) : _geom(page), _engine(_geom) {}

	// Fill from the seed pixel (x, y), clipped to the given rectangle
	// (left/right in screen columns, top/bottom in rows).
	void fill(int x, uint8_t y, uint8_t clipLeft, uint8_t clipRight,
			  uint8_t clipTop, uint8_t clipBottom) {
		_geom.setClip(clipLeft, clipRight, clipTop, clipBottom);
		_engine.fill(x, y);
	}

private:
	GmHiresSpanGeom<Page> _geom;
	GmSpanFill<GmHiresSpanGeom<Page> > _engine;
};

} // namespace Comprehend
} // namespace Glk

#endif
