#include "BurstNeuronNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

namespace {
constexpr float kSchmittHigh = 0.7f;
constexpr float kSchmittLow = 0.3f;
}

void BurstNeuronNode::setCount(float count) {
    count_ = std::clamp(count, 1.0f, 32.0f);
}

void BurstNeuronNode::setIntervalMs(float ms) {
    intervalMs_ = std::max(ms, 0.02f);
    intervalSamples_ = std::max<std::uint32_t>(
        1, static_cast<std::uint32_t>(sampleRate_ * static_cast<double>(intervalMs_) * 0.001));
}

void BurstNeuronNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    inputHigh_ = false;
    burstsRemaining_ = 0;
    nextIn_ = 0;
    setIntervalMs(intervalMs_);
}

void BurstNeuronNode::process(std::span<const float> inA,
                              std::span<const float>,
                              std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        const float x = inA[i];
        if (!inputHigh_ && x >= kSchmittHigh) {
            inputHigh_ = true;
            burstsRemaining_ = static_cast<int>(count_);
            nextIn_ = 0;
        } else if (inputHigh_ && x <= kSchmittLow) {
            inputHigh_ = false;
        }

        float spike = 0.0f;
        if (burstsRemaining_ > 0) {
            if (nextIn_ == 0) {
                spike = 1.0f;
                --burstsRemaining_;
                nextIn_ = intervalSamples_;
            } else {
                --nextIn_;
            }
        }
        out[i] = spike;
    }
}

} // namespace neurons::engine::nodes
