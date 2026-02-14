#include "AnalogNandNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void AnalogNandNode::reset(double) {}

void AnalogNandNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float a = 0.5f * (std::clamp(inA[i], -1.0f, 1.0f) + 1.0f);
        const float b = 0.5f * (std::clamp(inB[i], -1.0f, 1.0f) + 1.0f);
        out[i] = 1.0f - std::min(a, b);
    }
}

} // namespace neurons::engine::nodes
