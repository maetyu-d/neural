#include "IntegratorNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void IntegratorNode::reset(double) {
    state_ = 0.0f;
}

void IntegratorNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        state_ += inA[i] + (0.1f * inB[i]);
        state_ *= (1.0f - leak_);
        out[i] = std::tanh(state_ * 0.2f);
    }
}

} // namespace neurons::engine::nodes
