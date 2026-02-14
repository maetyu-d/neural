#include "SampleHoldGatedNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void SampleHoldGatedNode::setThreshold(float threshold) {
    threshold_ = std::clamp(threshold, -1.0f, 1.0f);
}

void SampleHoldGatedNode::reset(double) {
    held_ = 0.0f;
}

void SampleHoldGatedNode::process(std::span<const float> inA,
                                  std::span<const float> inB,
                                  std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        if (inB[i] >= threshold_) {
            held_ = inA[i];
        }
        out[i] = held_;
    }
}

} // namespace neurons::engine::nodes
