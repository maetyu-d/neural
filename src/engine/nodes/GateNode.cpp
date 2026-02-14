#include "GateNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void GateNode::reset(double) {}

void GateNode::process(std::span<const float> inA,
                       std::span<const float> inB,
                       std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = (inB[i] >= threshold_) ? inA[i] : 0.0f;
    }
}

} // namespace neurons::engine::nodes
