#include "SpikeGeneratorNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

namespace {
constexpr float kHysteresis = 0.1f;
}

void SpikeGeneratorNode::setThreshold(float threshold) {
    threshold_ = std::clamp(threshold, -2.0f, 2.0f);
}

void SpikeGeneratorNode::setPulseMs(float ms) {
    pulseMs_ = std::max(ms, 0.02f);
    pulseSamples_ = std::max<std::uint32_t>(
        1, static_cast<std::uint32_t>(sampleRate_ * static_cast<double>(pulseMs_) * 0.001));
}

void SpikeGeneratorNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    remaining_ = 0;
    high_ = false;
    setPulseMs(pulseMs_);
}

void SpikeGeneratorNode::process(std::span<const float> inA,
                                 std::span<const float>,
                                 std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (!high_ && inA[i] >= threshold_) {
            high_ = true;
            remaining_ = pulseSamples_;
        } else if (high_ && inA[i] <= (threshold_ - kHysteresis)) {
            high_ = false;
        }

        out[i] = (remaining_ > 0) ? 1.0f : 0.0f;
        if (remaining_ > 0) {
            --remaining_;
        }
    }
}

} // namespace neurons::engine::nodes
