// mysterious_canvas.cpp
//
// See mysterious_canvas.hpp.

#include <algorithm>

#include "mysterious_canvas.hpp"

void MysteriousCanvas::fill(glui32 color, int x, int y, int right, int bottom)
{
    x = std::max(x, 0);
    y = std::max(y, 0);
    right = std::min(right, width);
    bottom = std::min(bottom, height);

    if (x >= right || y >= bottom) {
        return;
    }

    if (pixels_.empty()) {
        pixels_.assign(width * height, zcolor_Default);
    }

    for (int row = y; row < bottom; row++) {
        auto start = pixels_.begin() + row * width;
        std::fill(start + x, start + right, color);
    }

    pending_.push_back({color, x, y, right - x, bottom - y});
}

void MysteriousCanvas::flush(Paint paint)
{
    for (const auto &rect : pending_) {
        paint(rect.color, rect.x, rect.y, rect.width, rect.height);
    }

    pending_.clear();
}

void MysteriousCanvas::replay(Paint paint)
{
    pending_.clear();

    if (pixels_.empty()) {
        return;
    }

    for (int y = 0; y < height; ) {
        auto row = pixels_.begin() + y * width;
        int limit = y < picture_height ? picture_height : height;
        int rows = 1;

        while (y + rows < limit && std::equal(row, row + width, row + rows * width)) {
            rows++;
        }

        for (int x = 0; x < width; ) {
            int run = 1;

            while (x + run < width && row[x + run] == row[x]) {
                run++;
            }

            paint(row[x], x, y, run, rows);
            x += run;
        }

        y += rows;
    }
}

void MysteriousCanvas::clear()
{
    pixels_.clear();
    pending_.clear();
}

std::vector<uint32_t> MysteriousCanvas::runs() const
{
    std::vector<uint32_t> runs;

    for (size_t i = 0; i < pixels_.size(); ) {
        size_t run = 1;
        while (i + run < pixels_.size() && pixels_[i + run] == pixels_[i]) {
            run++;
        }
        runs.push_back(pixels_[i]);
        runs.push_back(static_cast<uint32_t>(run));
        i += run;
    }

    return runs;
}

void MysteriousCanvas::set_runs(const std::vector<uint32_t> &runs)
{
    const size_t size = width * height;

    clear();

    for (size_t i = 0; i + 1 < runs.size(); i += 2) {
        size_t run = runs[i + 1];
        if (run > size - pixels_.size()) {
            pixels_.clear();
            return;
        }
        pixels_.insert(pixels_.end(), run, runs[i]);
    }

    if (pixels_.size() != size) {
        pixels_.clear();
    }
}
