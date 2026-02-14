#include "SampleHoldNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void SampleHoldNode::reset(double) {
    held_ = 0.0f;
}

void SampleHoldNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        if (inB[i] > 0.5f) {
            held_ = inA[i];
        }
        out[i] = held_;
    }
}

} // namespace neurons::engine::nodes
