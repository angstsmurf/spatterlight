/* test_gm_stream.cpp -- unit test for the shared opcode dispatcher (gm_stream.h)
 * against a recording mock renderer.  No fixtures, no renderer .cpp needed. */

#include "gm_stream.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace Glk::Comprehend;

struct Mock {
	std::string log;
	int px = 10, py = 20;
	bool op7Ends = false, resetEnds = false;
	int resetConsume = 0;
	int shift = 0;             // scaleX shift, to model the DHGR x doubling

	uint16_t scaleX(uint16_t x) const { return (uint16_t)(x << shift); }
	int penX() const { return px; }
	int penY() const { return py; }
	void add(const char *fmt, int a = 0, int b = 0) {
		char buf[96];
		snprintf(buf, sizeof(buf), fmt, a, b);
		log += buf;
		log += ';';
	}
	void begin() { log += "begin;"; }
	void end() { log += "end;"; }
	void setTextPos(uint16_t x, uint8_t y) { add("tpos %d,%d", x, y); }
	void drawText(uint8_t ch, bool normal) { add(normal ? "text %d" : "outline %d", ch); }
	void advanceText() { log += "adv;"; }
	void setPen(uint8_t p) { add("pen %d", p); }
	void setShape(uint8_t p) { add("shape %d", p); }
	void setFill(uint8_t b) { add("fill %d", b); }
	bool op7(uint8_t i, uint8_t v) { add("op7 %d,%d", i, v); return op7Ends; }
	void moveTo(uint16_t x, uint8_t y) { add("move %d,%d", x, y); px = x; py = y; }
	void lineTo(uint16_t x, uint8_t y) { add("line %d,%d", x, y); px = x; py = y; }
	void circle(uint8_t r) { add("circle %d", r); }
	void drawBrush(uint16_t x, uint8_t y) { add("brush %d,%d", x, y); }
	void delay(uint8_t u) { add("delay %d", u); }
	void paint(uint16_t x, uint8_t y) { add("paint %d,%d", x, y); }
	bool opReset(uint8_t param, const uint8_t *&p, const uint8_t *) {
		add("reset %d", param);
		p += resetConsume;
		return resetEnds;
	}
};

static int s_fail = 0;
static void check(const char *name, Mock &m, const std::vector<uint8_t> &s, const char *want) {
	gmRunStream(m, s.data(), s.size());
	if (m.log != want) {
		printf("FAIL %s\n  got:  %s\n  want: %s\n", name, m.log.c_str(), want);
		s_fail++;
	}
}

int main() {
	{   // operand decode with the x-high bit, and the one-operand ops
		Mock m;
		check("decode", m, { 0x81, 0x05, 0x07,       // move x=261,y=7
							 0xa0, 0x10, 0x20,       // line 16,32
							 0xc1, 0xff, 0x00,       // brush 511,0
							 0xe0, 0x01, 0x02,       // paint 1,2
							 0xb0, 0x09,             // circle 9
							 0xd0, 0x03,             // delay 3
							 0x23, 0x45, 0x60, 0x2a }, // pen 3; shape 5; fill 42
			  "begin;move 261,7;line 16,32;brush 511,0;paint 1,2;circle 9;delay 3;"
			  "pen 3;shape 5;fill 42;end;");
	}
	{   // DRAW_BOX: four chained edges from the start pen, pen left at the far corner
		Mock m;
		check("box", m, { 0x90, 0x50, 0x60 },       // far corner (80,96); pen (10,20)
			  "begin;line 10,96;line 80,96;line 80,20;line 10,20;move 80,96;end;");
	}
	{   // scaleX applies to every x operand, but never to y or to the pen start
		Mock m;
		m.shift = 1;
		check("scale", m, { 0x10, 0x03, 0x04, 0x80, 0x05, 0x06, 0xe0, 0x07, 0x08 },
			  "begin;tpos 6,4;move 10,6;paint 14,8;end;");
	}
	{   // text: both ops advance the cursor after drawing
		Mock m;
		check("text", m, { 0x30, 'A', 0x50, 'B' },
			  "begin;text 65;adv;outline 66;adv;end;");
	}
	{   // OPCODE_END ends the image, and end() still runs
		Mock m;
		check("end", m, { 0x40, 0x00, 0x41 }, "begin;shape 0;end;");
	}
	{   // op7 may end the image
		Mock m;
		m.op7Ends = true;
		check("op7-end", m, { 0x70, 1, 2, 0x41 }, "begin;op7 1,2;end;");
		Mock n;
		check("op7-skip", n, { 0x70, 1, 2, 0x41 }, "begin;op7 1,2;shape 1;end;");
	}
	{   // RESET: the renderer consumes its own operands and may end the image
		Mock m;
		m.resetConsume = 4;
		check("reset", m, { 0xf2, 1, 2, 3, 4, 0x41 }, "begin;reset 2;shape 1;end;");
		Mock n;
		n.resetEnds = true;
		check("reset-end", n, { 0xf1, 0x41 }, "begin;reset 1;end;");
	}
	{   // empty and runaway streams
		Mock m;
		check("empty", m, {}, "begin;end;");
		Mock n;
		std::vector<uint8_t> big(200000, 0x41);   // 200000 shape ops: capped at 100000
		gmRunStream(n, big.data(), big.size());
		int shapes = 0;
		for (size_t i = 0; i + 6 < n.log.size(); i++)
			if (n.log.compare(i, 7, "shape 1") == 0)
				shapes++;
		if (shapes != 100000) {
			printf("FAIL guard: %d ops ran\n", shapes);
			s_fail++;
		}
	}
	if (s_fail) {
		printf("%d failure(s)\n", s_fail);
		return 1;
	}
	printf("PASS: gm_stream dispatcher\n");
	return 0;
}
