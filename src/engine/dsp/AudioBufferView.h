#pragma once

#include <span>

namespace neurons::engine::dsp {

struct AudioBufferView {
    std::span<float> left;
    std::span<float> right;
    int numSamples{};
};

} // namespace neurons::engine::dsp
