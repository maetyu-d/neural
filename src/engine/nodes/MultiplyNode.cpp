#include "MultiplyNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void MultiplyNode::reset(double) {}

void MultiplyNode::process(std::span<const float> inA,
                           std::span<const float> inB,
                           std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = inA[i] * inB[i];
    }
}

} // namespace neurons::engine::nodes
