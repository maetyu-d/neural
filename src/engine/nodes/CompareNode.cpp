#include "CompareNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void CompareNode::setGreaterMode(bool greaterMode) {
    greaterMode_ = greaterMode;
}

void CompareNode::reset(double) {}

void CompareNode::process(std::span<const float> inA,
                          std::span<const float> inB,
                          std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const bool result = greaterMode_ ? (inA[i] > inB[i]) : (inA[i] < inB[i]);
        out[i] = result ? 1.0f : 0.0f;
    }
}

} // namespace neurons::engine::nodes
