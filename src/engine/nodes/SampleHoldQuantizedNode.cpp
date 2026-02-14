#include "SampleHoldQuantizedNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void SampleHoldQuantizedNode::setThreshold(float threshold) {
    threshold_ = std::clamp(threshold, -1.0f, 1.0f);
}

void SampleHoldQuantizedNode::setSteps(float steps) {
    steps_ = std::clamp(static_cast<int>(std::lround(steps)), 2, 128);
}

void SampleHoldQuantizedNode::reset(double) {
    held_ = 0.0f;
}

void SampleHoldQuantizedNode::process(std::span<const float> inA,
                                      std::span<const float> inB,
                                      std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    const float denom = static_cast<float>(std::max(1, steps_ - 1));
    for (std::size_t i = 0; i < n; ++i) {
        if (inB[i] >= threshold_) {
            const float normalized = 0.5f * (std::clamp(inA[i], -1.0f, 1.0f) + 1.0f);
            const float q = std::round(normalized * denom) / denom;
            held_ = q * 2.0f - 1.0f;
        }
        out[i] = held_;
    }
}

} // namespace neurons::engine::nodes
