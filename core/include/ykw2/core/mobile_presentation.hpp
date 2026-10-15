#pragma once

#include <cstdint>
#include <string>

namespace ykw2::core {

// Presentation-only settings for the independent YW2 mobile client. The
// reference_style records the UX family being reproduced; it is not a
// dependency on the YW1 executable or its assets.
struct MobilePresentationProfile {
    std::string id = "ykw2_mobile";
    std::string reference_style = "yw1_smartphone";
    std::uint16_t scene_height_percent = 58;
    std::uint16_t dialogue_top_percent = 55;
    bool portrait_preferred = true;
    bool touch_controls = true;
};

MobilePresentationProfile mobile_yw1_smartphone_profile();

}  // namespace ykw2::core
