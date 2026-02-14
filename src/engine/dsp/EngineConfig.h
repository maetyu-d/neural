#pragma once

#include <cstdint>

namespace neurons::engine::dsp {

struct CycleConfig {
    std::uint8_t defaultCap{8};
    std::uint8_t highPrecisionCap{16};
    bool highPrecisionEnabled{false};

    std::uint8_t activeCap() const {
        return highPrecisionEnabled ? highPrecisionCap : defaultCap;
    }
};

} // namespace neurons::engine::dsp
