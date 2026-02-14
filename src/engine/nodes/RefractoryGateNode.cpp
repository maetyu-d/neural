#include "RefractoryGateNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

namespace {
constexpr float kSchmittHigh = 0.7f;
constexpr float kSchmittLow = 0.3f;
}

void RefractoryGateNode::setRefractoryMs(float ms) {
    refractoryMs_ = std::max(ms, 0.0f);
    refractorySamples_ = static_cast<std::uint32_t>(sampleRate_ * static_cast<double>(refractoryMs_) * 0.001);
}

void RefractoryGateNode::setPulseMs(float ms) {
    pulseMs_ = std::max(ms, 0.02f);
    pulseSamples_ = std::max<std::uint32_t>(
        1, static_cast<std::uint32_t>(sampleRate_ * static_cast<double>(pulseMs_) * 0.001));
}

void RefractoryGateNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    refractoryRemaining_ = 0;
    pulseRemaining_ = 0;
    inputHigh_ = false;
    setRefractoryMs(refractoryMs_);
    setPulseMs(pulseMs_);
}

void RefractoryGateNode::process(std::span<const float> inA,
                                 std::span<const float>,
                                 std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        const float x = inA[i];
        if (!inputHigh_ && x >= kSchmittHigh) {
            inputHigh_ = true;
            if (refractoryRemaining_ == 0) {
                pulseRemaining_ = pulseSamples_;
                refractoryRemaining_ = refractorySamples_;
            }
        } else if (inputHigh_ && x <= kSchmittLow) {
            inputHigh_ = false;
        }

        out[i] = (pulseRemaining_ > 0) ? 1.0f : 0.0f;

        if (pulseRemaining_ > 0) {
            --pulseRemaining_;
        }
        if (refractoryRemaining_ > 0) {
            --refractoryRemaining_;
        }
    }
}

} // namespace neurons::engine::nodes
