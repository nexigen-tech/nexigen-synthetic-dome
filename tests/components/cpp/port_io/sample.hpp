#pragma once

#include <cstdint>

namespace nxd_test {

struct PortIoSample {
    std::uint64_t sequence;
    std::int64_t value;
};

} // namespace nxd_test
