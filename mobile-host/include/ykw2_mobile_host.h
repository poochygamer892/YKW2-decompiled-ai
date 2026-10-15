#ifndef YKW2_MOBILE_HOST_H
#define YKW2_MOBILE_HOST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MobileRuntime MobileRuntime;

enum {
    YKW2_TOP_WIDTH = 400,
    YKW2_BOTTOM_WIDTH = 320,
    YKW2_SCREEN_HEIGHT = 240,
    YKW2_TOP_RGBA_BYTES = YKW2_TOP_WIDTH * YKW2_SCREEN_HEIGHT * 4,
    YKW2_BOTTOM_RGBA_BYTES = YKW2_BOTTOM_WIDTH * YKW2_SCREEN_HEIGHT * 4,
};

MobileRuntime* ykw2_mobile_open(const char* rom_path,
                               const char* recompiled_library,
                               const char* data_directory,
                               char* error_out,
                               size_t error_capacity);
int32_t ykw2_mobile_run_frames(MobileRuntime* runtime, uint32_t frames);
int32_t ykw2_mobile_set_input(MobileRuntime* runtime, uint32_t buttons,
                              float circle_x, float circle_y,
                              int32_t touch_x, int32_t touch_y);
int32_t ykw2_mobile_copy_screens(MobileRuntime* runtime,
                                 uint8_t* top_out, size_t top_capacity,
                                 uint8_t* bottom_out, size_t bottom_capacity);
int32_t ykw2_mobile_take_audio(MobileRuntime* runtime, int16_t* samples_out,
                               size_t sample_capacity);
void ykw2_mobile_close(MobileRuntime* runtime);
int32_t ykw2_mobile_screen_dimensions(uint32_t* top_width,
                                      uint32_t* bottom_width,
                                      uint32_t* height);

#ifdef __cplusplus
}
#endif

#endif
