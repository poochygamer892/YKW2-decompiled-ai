#include "ykw2/core/mobile_presentation.hpp"

namespace ykw2::core {

MobilePresentationProfile mobile_yw1_smartphone_profile() {
    // These values describe the mobile presentation shell only. Game state,
    // resources and event semantics remain supplied by the YW2 runtime.
    return {
        "ykw2_mobile",
        "yw1_smartphone",
        58,
        55,
        true,
        true,
    };
}

}  // namespace ykw2::core
