#include "ykw2/core/runtime_save.hpp"

#include <cassert>
#include <vector>

namespace {

void test_round_trip() {
    const ykw2::core::RuntimeSaveState expected{
        true, "data/map/e100g002/e100g002.xc", 12.5f, -3.25f, 4.75, 42};
    const auto bytes = ykw2::core::serialize_runtime_save(expected);
    assert(!bytes.empty());
    const auto parsed = ykw2::core::parse_runtime_save(bytes);
    assert(parsed.has_value());
    assert(parsed->active);
    assert(parsed->map_resource == expected.map_resource);
    assert(parsed->player_x == expected.player_x);
    assert(parsed->player_y == expected.player_y);
    assert(parsed->simulation_seconds == expected.simulation_seconds);
    assert(parsed->frame == expected.frame);
}

void test_rejects_truncated_and_traversal() {
    const ykw2::core::RuntimeSaveState expected{
        true, "data/map/e100g002/e100g002.xc", 0.0f, 0.0f, 0.0, 0};
    auto bytes = ykw2::core::serialize_runtime_save(expected);
    bytes.pop_back();
    assert(!ykw2::core::parse_runtime_save(bytes).has_value());

    assert(ykw2::core::serialize_runtime_save(
               {true, "../outside.xc", 0.0f, 0.0f, 0.0, 0})
               .empty());
}

}  // namespace

int main() {
    test_round_trip();
    test_rejects_truncated_and_traversal();
    return 0;
}
