#include "presentation.h"

#include <algorithm>

namespace ykw2::desktop {

Presentation make_presentation(int window_width, int window_height,
                               bool show_bottom) {
    const int width = std::max(window_width, 1);
    const int height = std::max(window_height, 1);
    const double scale = std::min(width / 400.0, height / 240.0);
    const int top_width = std::max(1, static_cast<int>(400.0 * scale));
    const int top_height = std::max(1, static_cast<int>(240.0 * scale));
    const Rect top{(width - top_width) / 2, (height - top_height) / 2,
                   top_width, top_height};

    // The inset occupies 30% of the fitted top-screen width and preserves
    // the original 4:3 aspect ratio. Keep a modest margin from window edges.
    const int inset_width = std::max(1, std::min(
        static_cast<int>(top_width * 0.30), width / 2));
    const int inset_height = std::max(1, inset_width * 3 / 4);
    const int margin = std::max(8, static_cast<int>(top_width * 0.018));
    const Rect bottom{std::max(0, width - inset_width - margin),
                      std::max(0, height - inset_height - margin),
                      inset_width, inset_height};
    return {top, bottom, show_bottom};
}

bool map_bottom_touch(const Presentation& layout, int window_x, int window_y,
                      int& touch_x, int& touch_y) {
    const Rect& r = layout.bottom;
    if (!layout.bottom_visible || r.width <= 0 || r.height <= 0 ||
        window_x < r.x || window_y < r.y ||
        window_x >= r.x + r.width || window_y >= r.y + r.height) {
        return false;
    }
    touch_x = (window_x - r.x) * 320 / r.width;
    touch_y = (window_y - r.y) * 240 / r.height;
    return true;
}

}  // namespace ykw2::desktop
