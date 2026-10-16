#include "presentation.h"

#include <cassert>

int main() {
    using namespace ykw2::desktop;
    const auto wide = make_presentation(1280, 720, true);
    assert(wide.top.width == 1200 && wide.top.height == 720);
    assert(wide.bottom_visible);
    int x = -1;
    int y = -1;
    assert(map_bottom_touch(wide, wide.bottom.x, wide.bottom.y, x, y));
    assert(x == 0 && y == 0);
    assert(map_bottom_touch(wide, wide.bottom.x + wide.bottom.width - 1,
                            wide.bottom.y + wide.bottom.height - 1, x, y));
    assert(x == 319 && y == 239);
    assert(!map_bottom_touch(wide, 0, 0, x, y));

    const auto portrait = make_presentation(600, 900, false);
    assert(portrait.top.width == 600 && portrait.top.height == 360);
    assert(!portrait.bottom_visible);
    assert(!map_bottom_touch(portrait, portrait.bottom.x, portrait.bottom.y, x, y));
    const auto tiny = make_presentation(1, 1, true);
    assert(tiny.top.width == 1 && tiny.top.height == 1);
}
