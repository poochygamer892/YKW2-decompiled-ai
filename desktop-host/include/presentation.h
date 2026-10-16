#ifndef YKW2_DESKTOP_PRESENTATION_H
#define YKW2_DESKTOP_PRESENTATION_H

namespace ykw2::desktop {

struct Rect {
    int x;
    int y;
    int width;
    int height;
};

struct Presentation {
    Rect top;
    Rect bottom;
    bool bottom_visible;
};

// Fit the 5:3 top screen in the window and put the 4:3 touch screen in a
// compact, clickable picture-in-picture. Set show_bottom=false for an
// unobstructed gameplay view; the same layout is recomputed on resize.
Presentation make_presentation(int window_width, int window_height,
                               bool show_bottom);

// Map a window point inside the bottom-screen picture-in-picture to 3DS
// touch coordinates (320x240). Returns false when outside or hidden.
bool map_bottom_touch(const Presentation& layout, int window_x, int window_y,
                      int& touch_x, int& touch_y);

}  // namespace ykw2::desktop

#endif
