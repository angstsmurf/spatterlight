/* gm_stream.h -- shared Graphics Magician opcode-stream dispatcher.
 *
 * Every graphics_magician*.cpp renderer decodes the same byte stream: the high
 * nibble of each command byte is the opcode (graphics_magician.h `Opcode`), the
 * low nibble is `param`, and bit 0 of `param` is the high bit (+256) of the x
 * operand for coordinate opcodes.  The loop, the operand decode and the order of
 * the DRAW_BOX lines used to be written out once per renderer; they live here
 * once, in gmRunStream().  What genuinely differs between renderers is the leaf
 * behaviour of each opcode, which the renderer supplies as a small object `R`
 * (held by reference, so it can wrap the renderer's file statics or its context
 * struct; no virtuals, the template inlines it).
 *
 * `R` must provide (x operands are *source* coordinates 0..511; the dispatcher
 * passes them through scaleX() first, so everything below the box/line level
 * works in the renderer's native x units; y is never scaled):
 *
 *   uint16_t scaleX(uint16_t x);   source x -> native x (identity, or x << 1 on DHGR)
 *   int  penX(); int penY();       current pen in native units (DRAW_BOX start)
 *
 *   void begin();                  per-image init (pen, brush, fill, text, clip)
 *   void end();                    runs on EVERY exit, incl. OPCODE_END
 *
 *   void setTextPos(x, y);         op1  (x already scaled)
 *   void drawText(ch, normal);     op3 (normal = true) / op5 (false)
 *   void advanceText();            after each text op
 *   void setPen(param);            op2
 *   void setShape(param);          op4
 *   void setFill(b);               op6
 *   bool op7(i, v);                op7 operands; return true to end the image
 *   void moveTo(x, y);             op8  (x scaled)
 *   void lineTo(x, y);             op10 and each DRAW_BOX edge: draw from the
 *                                  pen to (x,y), pen left at (x,y) (x scaled)
 *   void circle(radius);           op11, centred on the pen
 *   void drawBrush(x, y);          op12 (x scaled)
 *   void delay(units);             op13
 *   void paint(x, y);              op14 (x scaled)
 *   bool opReset(param, p, end);   op15: consume its own operands from `p`;
 *                                  return true to end the image
 *
 *   void beforeOp(p, end, op, param);  trace hook, after the command byte is
 *                                  read (`p` points at its first operand)
 *   void afterOp(op, param);       trace hook, after every op that did not end
 *                                  the image
 *
 * The two trace hooks are empty for a renderer that has no debug tracing.
 *
 * Header-only so every renderer .cpp stays a self-contained translation unit
 * for the offline test harnesses.
 */

#ifndef GLK_COMPREHEND_GM_STREAM_H
#define GLK_COMPREHEND_GM_STREAM_H

#include <cstdint>
#include <cstddef>
#include "graphics_magician.h"   // Opcode

namespace Glk {
namespace Comprehend {

// Read a coordinate operand pair: x (+256 when bit 0 of `param` is set), then y.
inline void gmReadXY(const uint8_t *&p, uint8_t param, uint16_t &x, uint8_t &y) {
	x = (uint16_t)(*p++ + ((param & 1) ? 256 : 0));
	y = *p++;
}

template <class R>
inline void gmRunStream(R &r, const uint8_t *data, size_t size) {
	const uint8_t *p = data;
	const uint8_t *e = data + size;
	r.begin();
	int guard = 0;
	while (p < e && guard++ < 100000) {
		const uint8_t cmd = *p++;
		const uint8_t param = cmd & 0xf;
		const uint8_t op = cmd >> 4;
		uint16_t x;
		uint8_t y;
		bool done = false;

		r.beforeOp(p, e, op, param);
		switch (op) {
		case OPCODE_END:
			done = true;
			break;

		case OPCODE_OP7: {
			const uint8_t i = *p++;
			const uint8_t v = *p++;
			done = r.op7(i, v);
			break;
		}

		case OPCODE_SET_TEXT_POS:
			gmReadXY(p, param, x, y);
			r.setTextPos(r.scaleX(x), y);
			break;

		case OPCODE_SET_PEN_COLOR:
			r.setPen(param);
			break;

		case OPCODE_TEXT_CHAR:
			r.drawText(*p++, true);
			r.advanceText();
			break;

		case OPCODE_TEXT_OUTLINE:
			r.drawText(*p++, false);
			r.advanceText();
			break;

		case OPCODE_SET_SHAPE:
			r.setShape(param);
			break;

		case OPCODE_SET_FILL_COLOR:
			r.setFill(*p++);
			break;

		case OPCODE_MOVE_TO:
			gmReadXY(p, param, x, y);
			r.moveTo(r.scaleX(x), y);
			break;

		case OPCODE_DRAW_BOX: {
			// Four chained edges from the pen's start corner; the pen is left at the
			// FAR corner (a,b), not the start (Talisman RA #1: 11 wrong bytes along
			// one diagonal edge when it was left at the start).
			gmReadXY(p, param, x, y);
			const uint16_t a = r.scaleX(x);
			const int x0 = r.penX(), y0 = r.penY();
			r.lineTo((uint16_t)x0, y);          // left edge   (x0,y0)->(x0,b)
			r.lineTo(a, y);                     // bottom edge ->(a,b)
			r.lineTo(a, (uint8_t)y0);           // right edge  ->(a,y0)
			r.lineTo((uint16_t)x0, (uint8_t)y0);// top edge    ->(x0,y0)
			r.moveTo(a, y);
			break;
		}

		case OPCODE_DRAW_LINE:
			gmReadXY(p, param, x, y);
			r.lineTo(r.scaleX(x), y);
			break;

		case OPCODE_DRAW_CIRCLE:
			r.circle(*p++);
			break;

		case OPCODE_DRAW_SHAPE:
			gmReadXY(p, param, x, y);
			r.drawBrush(r.scaleX(x), y);
			break;

		case OPCODE_DELAY:
			r.delay(*p++);
			break;

		case OPCODE_PAINT:
			gmReadXY(p, param, x, y);
			r.paint(r.scaleX(x), y);
			break;

		case OPCODE_RESET:
			done = r.opReset(param, p, e);
			break;
		}
		if (done)
			break;
		r.afterOp(op, param);
	}
	r.end();
}

} // namespace Comprehend
} // namespace Glk

#endif
