#include "ThresholdNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void ThresholdNode::reset(double) {
    high_ = false;
}

void ThresholdNode::process(std::span<const float> inA,
                            std::span<const float> inB,
                            std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float t = threshold_ + (0.5f * inB[i]);
        if (!high_ && inA[i] >= (t + hysteresis_)) {
            high_ = true;
        } else if (high_ && inA[i] <= (t - hysteresis_)) {
            high_ = false;
        }
        out[i] = high_ ? 1.0f : 0.0f;
    }
}

} // namespace neurons::engine::nodes
