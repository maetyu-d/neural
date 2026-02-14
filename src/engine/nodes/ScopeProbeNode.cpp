#include "ScopeProbeNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void ScopeProbeNode::reset(double) {
    lastPeak_ = 0.0f;
}

void ScopeProbeNode::process(std::span<const float> inA,
                             std::span<const float>,
                             std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    float peak = 0.0f;

    for (std::size_t i = 0; i < n; ++i) {
        const float sample = inA[i];
        out[i] = sample;
        peak = std::max(peak, std::abs(sample));
    }

    lastPeak_ = peak;
}

float ScopeProbeNode::lastPeak() const {
    return lastPeak_;
}

} // namespace neurons::engine::nodes
