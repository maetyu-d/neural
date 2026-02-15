#include "LeakNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void LeakNode::reset(double) {
    state_ = 0.0f;
}

void LeakNode::process(std::span<const float> inA,
                       std::span<const float> inB,
                       std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float leakCv = std::clamp(0.5f * (inB[i] + 1.0f), 0.0f, 1.0f);
        const float dynamicLeak = std::clamp(leak_ + (0.45f * leakCv), 0.0f, 0.99f);
        state_ += 0.25f * (inA[i] - state_);
        state_ *= (1.0f - dynamicLeak);
        out[i] = state_;
    }
}

} // namespace neurons::engine::nodes
