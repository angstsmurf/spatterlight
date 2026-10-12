/* gm_span_fill.h -- the Graphics Magician op14 flood fill, shared by the
 * Apple II standard hi-res and double hi-res renderers.
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
 * traversal/merge order is part of the algorithm and is preserved here.
 *
 * A pixel position is its horizontal coordinate, column*7 + bit, where `column`
 * selects the hi-res byte (each byte holds 7 visible pixels) and `bit` is 0..6
 * within it. Positions are therefore ordinary integers: comparisons are <, == and
 * stepping one pixel is ++/--. Stepping left of column 0 yields a negative index,
 * which correctly sorts below every on-screen pixel (matching the signed-byte
 * boundary tests of the 6502 original) and is never used to address the screen.
 *
 * The two renderers differ only in how a column of the current row is read,
 * written and patterned, which the Page policy supplies:
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
 *
 * Header-only, in the style of graphics_magician_vector.h and slow_draw_page.h.
 */

#ifndef GLK_COMPREHEND_GM_SPAN_FILL_H
#define GLK_COMPREHEND_GM_SPAN_FILL_H

#include <cstdint>

namespace Glk {
namespace Comprehend {

template <typename Page>
class GmSpanFill {
public:
	explicit GmSpanFill(Page &page) : _page(page) {}

	// Fill from the seed pixel (x, y), clipped to the given rectangle
	// (left/right in screen columns, top/bottom in rows).
	void fill(int x, uint8_t y, uint8_t clipLeft, uint8_t clipRight,
			  uint8_t clipTop, uint8_t clipBottom) {
		_clipLeft = clipLeft;
		_clipRight = clipRight;
		_clipTop = clipTop;
		_clipBottom = clipBottom;
		_row = y;
		_pos = x;
		run();
	}

private:
	enum {
		BITS_PER_COL = 7,
		QUEUE_SIZE = 32,
		QUEUE_MASK = 0x1f,
		DIR_UP = 0,           // child run continues onto the row above
		DIR_DOWN = 1,         // child run continues onto the row below
		DIR_DISABLED = 0xff   // span was absorbed by a merge; skip it
	};

	struct Span {
		uint8_t row;
		int     left, right;   // inclusive pixel-coordinate edges
		uint8_t dir;
	};

	enum WalkAction {
		WALK_ADVANCE,   // keep walking the parent span for more runs
		WALK_DONE       // this entry is finished
	};

	Page &_page;
	Span    _queue[QUEUE_SIZE];
	uint8_t _head, _tail;          // circular-queue head / tail indices
	uint8_t _row;                  // current row
	int     _pos;                  // current pixel position (column*7 + bit)
	int     _edge;                 // left edge produced by paintSpan()
	uint8_t _bit, _edgeBit;        // paintSpan / edge-mask scratch
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

	// Is the pixel at the current position set?
	bool pixelIsSet() const {
		return (bitMask(bitOf(_pos)) & _page.read(colOf(_pos))) != 0;
	}

	// Locate the right / left edge of a run of set bits within byte `p`, returning
	// the paint mask and storing the boundary bit index (rightEdgeMask -> _edgeBit,
	// leftEdgeMask -> _bit). When all seven bits are set the run continues past
	// this byte (no edge here): return 0xff.
	uint8_t rightEdgeMask(uint8_t p) {
		if ((p & 0x7f) == 0x7f)
			return 0xff;
		_edgeBit = 7;
		uint8_t bit;
		do {
			_edgeBit--;
			bit = static_cast<uint8_t>(p & 0x7f);
			p = static_cast<uint8_t>(p << 1);
		} while (bit >> 6);
		return rightMask(_edgeBit);
	}
	uint8_t leftEdgeMask(uint8_t p) {
		p &= 0x7f;
		if (p == 0x7f)
			return 0xff;
		_bit = 0xff;
		uint8_t bit;
		do {
			_bit++;
			bit = static_cast<uint8_t>(p & 1);
			p = static_cast<uint8_t>(p >> 1);
		} while (bit);
		return leftMask(_bit);
	}

	// Blend `mask` worth of the current fill pattern into screen column `col`,
	// keeping the screen's existing pixels wherever the mask is clear.
	void blendByte(uint8_t mask, int col) {
		uint8_t kept = static_cast<uint8_t>((mask ^ _page.read(col)) & 0x7f);
		uint8_t pattern = _page.pattern(col);
		_page.write(col, static_cast<uint8_t>((pattern & mask) | kept));
	}

	// Paint the maximal run of set pixels through the seed _pos on row _row,
	// bounded by clear pixels or the clip edges. Leaves the run's left edge in
	// _edge and its right edge in _pos.
	void paintSpan() {
		int col = colOf(_pos);
		_bit = static_cast<uint8_t>(bitOf(_pos));

		// Seed byte: intersect the right-edge and left-edge masks so we paint only
		// the run that actually touches the seed bit.
		_edgeBit = 0xff;
		uint8_t existing = _page.read(col);
		uint8_t right_mask = rightEdgeMask(static_cast<uint8_t>(existing | rightMask(_bit)));
		uint8_t left_input = static_cast<uint8_t>(leftMask(_bit) | existing);
		_bit = 7;
		uint8_t mask = static_cast<uint8_t>(leftEdgeMask(left_input) & right_mask);

		// Walk left across whole columns, painting each, until a right-edge boundary
		// turns up or we reach the clip edge.
		uint8_t left_bit;
		for (;;) {
			blendByte(mask, col);
			if (_edgeBit != 0xff) { left_bit = _edgeBit; break; }
			if (--col < _clipLeft) { col++; left_bit = 0xff; break; }
			mask = rightEdgeMask(_page.read(col));
		}

		// The left edge sits one pixel past the boundary bit.
		uint8_t edge_bit = left_bit + 1;
		if (edge_bit == 7) { edge_bit = 0; col++; }
		_edge = toPos(col, edge_bit);

		// Walk right from the seed, painting whole columns, until the run ends (the
		// bit index runs off the left of a column with no continuation) or we reach
		// the clip edge.
		col = colOf(_pos);
		for (;;) {
			if (_bit == 7 && col != _clipRight) {
				col++;
				blendByte(leftEdgeMask(_page.read(col)), col);
				continue;
			}
			_bit--;
			if (_bit & 0x80) { _bit = 6; col--; }
			_pos = toPos(col, _bit);
			return;
		}
	}

	// Append a span (edges, row, direction) to the work queue, unless its row is the
	// clip edge in that direction.
	void enqueueSpan(uint8_t row, int left, int right, uint8_t dir) {
		if (Page::kDropSinglePixelSpans && left == right)
			return;
		uint8_t clip_row = (dir == DIR_UP) ? _clipTop : _clipBottom;
		if (row == clip_row)
			return;

		Span &s = _queue[_tail];
		s.row = row;
		s.left = left;
		s.right = right;
		s.dir = dir;
		_tail = (_tail + 1) & QUEUE_MASK;   // never expected to overflow 32 spans
	}

	// Paint the run at the current position and queue its follow-ups: a same-
	// direction continuation covering the whole run, plus opposite-direction spans
	// for any part of the run overhanging the parent span on the left or right.
	WalkAction paintRun(const Span &parent) {
		paintSpan();
		enqueueSpan(_row, _edge, _pos, parent.dir);
		int run_right = _pos;

		// Left overhang: the part of the run reaching left of the parent's left edge
		// is re-scanned in the opposite vertical direction.
		if (_edge < parent.left - 1)
			enqueueSpan(_row, _edge, parent.left - 2, parent.dir ^ 1);

		// Right overhang, likewise. If there is one we are finished with this run; if
		// the run ended inside the parent we keep walking from its right edge.
		if (parent.right + 1 < run_right) {
			enqueueSpan(_row, parent.right + 2, run_right, parent.dir ^ 1);
			return WALK_DONE;
		}
		if (parent.right + 1 == run_right)
			return WALK_DONE;

		_pos = run_right;
		return WALK_ADVANCE;
	}

	// Handle a set pixel found while walking the parent span. If an opposite-
	// direction span already queued covers this run, merge with it instead of
	// painting twice; otherwise paint the run.
	WalkAction handleRun(const Span &parent) {
		for (uint8_t i = (_head + 1) & QUEUE_MASK; i != _tail; i = (i + 1) & QUEUE_MASK) {
			Span &other = _queue[i];
			if (other.row != _row)       continue;
			if (other.dir == parent.dir) continue;
			if (_pos < other.left || _pos > other.right)
				continue;                              // run not inside `other`'s extent

			// Extend `other` left to cover the parent run reaching past its left edge.
			if (_pos != other.left && other.left < parent.left - 1)
				enqueueSpan(_row, other.left, parent.left - 2, other.dir);

			if (other.right <= parent.right) {
				// `other` lies entirely within the parent run: absorb it and keep
				// walking from its right edge.
				other.dir = DIR_DISABLED;
				_pos = other.right;
				return WALK_ADVANCE;
			}

			// `other` extends past the parent run: trim its left edge to just past
			// the parent's right edge and stop (it will be painted on its own turn).
			other.left = parent.right + 1;
			return WALK_DONE;
		}

		return paintRun(parent);
	}

	// Process one queued span: walk the parent span on the adjacent row, painting
	// (or merging) every connected run of set pixels it overlaps.
	void processQueueHead() {
		Span parent = _queue[_head];
		if (parent.dir == DIR_DISABLED)    // absorbed by an earlier merge
			return;

		_row = (parent.dir == DIR_UP) ? parent.row - 1 : parent.row + 1;
		_pos = parent.left;
		_page.setRow(_row);

		for (;;) {
			if (pixelIsSet() && handleRun(parent) == WALK_DONE)
				return;
			if (_pos == parent.right)
				return;
			_pos++;
		}
	}

	// Paint the seed run, queue its up/down children, then drain the queue.
	void run() {
		_page.setRow(_row);
		if (!pixelIsSet())
			return;
		paintSpan();

		_head = _tail = 0;
		enqueueSpan(_row, _edge, _pos, DIR_UP);
		enqueueSpan(_row, _edge, _pos, DIR_DOWN);

		long guard = 0;
		while (_head != _tail) {
			if (++guard > 2000000)         // safety net against pathological input
				return;
			processQueueHead();
			_head = (_head + 1) & QUEUE_MASK;
		}
	}
};

} // namespace Comprehend
} // namespace Glk

#endif
