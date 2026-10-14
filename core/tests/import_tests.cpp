#include "ykw2/core/import_contract.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void test_import_plan() {
    std::vector<std::uint8_t> header(0x200);
    header[0x100] = 'N';
    header[0x101] = 'C';
    header[0x102] = 'S';
    header[0x103] = 'D';
    put32(header, 0x104, 0x2000);  // 4 MiB media size
    put32(header, 0x120, 1);       // application partition offset: 0x200
    put32(header, 0x124, 0x100);   // application partition size: 0x20000
    put32(header, 0x128, 0x101);   // second partition offset
    put32(header, 0x12c, 1);       // second partition size

    const ykw2::core::ByteView view(header.data(), header.size());
    const auto no_consent = ykw2::core::build_import_plan(view, 0x400000, false);
    assert(!no_consent);
    assert(no_consent.error->code == "rights_confirmation_required");

    const auto plan = ykw2::core::build_import_plan(view, 0x400000, true);
    assert(plan);
    assert(plan.value->active_partitions.size() == 2);
    assert(plan.value->application_partition.index == 0);
    assert(plan.value->application_partition.offset == 0x200);
    assert(plan.value->application_partition.size == 0x20000);
}

void test_invalid_partition() {
    std::vector<std::uint8_t> header(0x200);
    header[0x100] = 'N';
    header[0x101] = 'C';
    header[0x102] = 'S';
    header[0x103] = 'D';
    put32(header, 0x104, 1);
    put32(header, 0x120, 2);
    put32(header, 0x124, 2);
    const auto parsed = ykw2::core::parse_ncsd_header(
        ykw2::core::ByteView(header.data(), header.size()), 0x400);
    assert(!parsed);
    assert(parsed.error->code == "ncsd_partition_out_of_bounds");
}

}  // namespace

int main() {
    test_import_plan();
    test_invalid_partition();
    return 0;
}
