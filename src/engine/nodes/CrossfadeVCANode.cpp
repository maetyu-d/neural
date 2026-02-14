#include "CrossfadeVCANode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void CrossfadeVCANode::reset(double) {}

void CrossfadeVCANode::process(std::span<const float> inA,
                               std::span<const float> inB,
                               std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float mix = std::clamp((inB[i] + 1.0f) * 0.5f, 0.0f, 1.0f);
        const float gA = std::sqrt(1.0f - mix);
        const float gB = std::sqrt(mix);
        out[i] = (inA[i] * gA) + (inB[i] * gB * 0.6f);
    }
}

} // namespace neurons::engine::nodes
