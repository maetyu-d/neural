#include "InvertNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void InvertNode::reset(double) {}

void InvertNode::process(std::span<const float> inA,
                         std::span<const float>,
                         std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = -inA[i];
    }
}

} // namespace neurons::engine::nodes
