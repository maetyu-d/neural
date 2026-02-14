#include "MixNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void MixNode::setWeights(float a, float b) {
    weightA_ = a;
    weightB_ = b;
}

void MixNode::reset(double) {}

void MixNode::process(std::span<const float> inA,
                      std::span<const float> inB,
                      std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = (inA[i] * weightA_) + (inB[i] * weightB_);
    }
}

} // namespace neurons::engine::nodes
