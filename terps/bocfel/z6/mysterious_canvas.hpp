// mysterious_canvas.hpp
//
// The 2011 releases of the Mysterious Adventures come without a Blorb file.
// Their pictures are stored in the story file as runs of pixels, and each run
// is drawn by moving window 5 over it and erasing the window to the colour of
// the run. The rule below the status lines is drawn in the same way. This is
// the record of those rectangles, in the size of the picture plus the rule.

#ifndef mysterious_canvas_hpp
#define mysterious_canvas_hpp

#include <cstdint>
#include <vector>

extern "C" {
#include "glk.h"
}

class MysteriousCanvas {
public:
    static constexpr int width = 512;
    static constexpr int picture_height = 192;
    static constexpr int rule_height = 8;
    static constexpr int height = picture_height + rule_height;

    using Paint = void (*)(glui32 color, int x, int y, int w, int h);

    // Add a rectangle, cut to the canvas. It is painted by the next flush().
    void fill(glui32 color, int x, int y, int right, int bottom);

    // The game selects the main window after each rectangle, which sends
    // what has been drawn so far to the screen. A picture consists of
    // thousands of rectangles, and sending them one at a time is slow, so
    // they are held back until the game asks for input.
    void flush(Paint paint);

    // Paint the entire canvas, as runs of pixels of the same colour. Rows
    // which are the same are painted in one go.
    void replay(Paint paint);

    void clear();

    // For autosaves: pairs of a colour and the number of pixels having it.
    std::vector<uint32_t> runs() const;
    // Runs that don't add up to exactly one canvas leave it empty.
    void set_runs(const std::vector<uint32_t> &runs);

private:
    struct Rect {
        glui32 color;
        int x, y, width, height;
    };

    std::vector<glui32> pixels_;
    std::vector<Rect> pending_;
};

#endif /* mysterious_canvas_hpp */
