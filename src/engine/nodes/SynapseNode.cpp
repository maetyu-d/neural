#include "SynapseNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void SynapseNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    state_ = 0.0f;
}

void SynapseNode::process(std::span<const float> inA,
                          std::span<const float> inB,
                          std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    const float tau = std::max(1.0f, lagMs_ * 0.001f * static_cast<float>(sampleRate_));
    const float a = std::clamp(1.0f / tau, 0.0001f, 1.0f);

    for (std::size_t i = 0; i < n; ++i) {
        const float target = (inA[i] * weight_) + (0.2f * inB[i]);
        state_ += a * (target - state_);
        out[i] = state_;
    }
}

} // namespace neurons::engine::nodes
