#include "DendriteSumNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void DendriteSumNode::setGains(float gainA, float gainB) {
    gainA_ = std::clamp(gainA, -8.0f, 8.0f);
    gainB_ = std::clamp(gainB, -8.0f, 8.0f);
}

void DendriteSumNode::reset(double) {}

void DendriteSumNode::process(std::span<const float> inA,
                              std::span<const float> inB,
                              std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = (inA[i] * gainA_) + (inB[i] * gainB_);
    }
}

} // namespace neurons::engine::nodes
