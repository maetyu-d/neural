#include "SlewNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void SlewNode::reset(double) {
    state_ = 0.0f;
}

void SlewNode::process(std::span<const float> inA,
                       std::span<const float>,
                       std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        const float d = std::clamp(inA[i] - state_, -maxStep_, maxStep_);
        state_ += d;
        out[i] = state_;
    }
}

} // namespace neurons::engine::nodes
