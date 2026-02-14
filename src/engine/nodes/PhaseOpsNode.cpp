#include "PhaseOpsNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void PhaseOpsNode::reset(double) {}

void PhaseOpsNode::process(std::span<const float> inA,
                           std::span<const float>,
                           std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        const float wrapped = inA[i] - std::floor(inA[i]);
        out[i] = wrapped;
    }
}

} // namespace neurons::engine::nodes
