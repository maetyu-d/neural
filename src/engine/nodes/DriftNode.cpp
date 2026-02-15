#include "DriftNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void DriftNode::reset(double) {
    state_ = 0x89ABCDEFu;
    drift_ = 0.0f;
}

void DriftNode::process(std::span<const float> inA,
                        std::span<const float> inB,
                        std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        state_ = state_ * 1103515245u + 12345u;
        const float rnd = static_cast<float>((state_ >> 16) & 0x7FFFu) / 16384.0f - 1.0f;
        const float amt = std::clamp(1.0f + inB[i], 0.05f, 3.0f);
        drift_ = std::clamp(drift_ + rnd * (0.00005f * amt), -0.2f, 0.2f);
        out[i] = inA[i] + drift_;
    }
}

} // namespace neurons::engine::nodes
