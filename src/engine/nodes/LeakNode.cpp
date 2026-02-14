#include "LeakNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void LeakNode::reset(double) {
    state_ = 0.0f;
}

void LeakNode::process(std::span<const float> inA,
                       std::span<const float>,
                       std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        state_ += 0.25f * (inA[i] - state_);
        state_ *= (1.0f - leak_);
        out[i] = state_;
    }
}

} // namespace neurons::engine::nodes
