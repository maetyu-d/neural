#include "RandomGateNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

namespace {
constexpr float kSchmittHigh = 0.7f;
constexpr float kSchmittLow = 0.3f;
}

void RandomGateNode::setProbability(float probability) {
    probability_ = std::clamp(probability, 0.0f, 1.0f);
}

void RandomGateNode::setPulseMs(float pulseMs) {
    const float clampedMs = std::max(pulseMs, 0.02f);
    pulseSamples_ = static_cast<std::uint32_t>(sampleRate_ * static_cast<double>(clampedMs) * 0.001);
    pulseSamples_ = std::max<std::uint32_t>(pulseSamples_, 1);
}

void RandomGateNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    remaining_ = 0;
    clockHigh_ = false;
    setPulseMs(2.0f);
}

float RandomGateNode::nextUnit() {
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    return static_cast<float>((rngState_ >> 8) & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

void RandomGateNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float clock = inA[i];
        const float pCv = inB[i];
        const float probability = (std::abs(pCv) > 1.0e-6f) ? std::clamp(pCv, 0.0f, 1.0f) : probability_;
        if (!clockHigh_ && clock >= kSchmittHigh) {
            clockHigh_ = true;
            if (nextUnit() <= probability) {
                remaining_ = pulseSamples_;
            }
        } else if (clockHigh_ && clock <= kSchmittLow) {
            clockHigh_ = false;
        }

        out[i] = (remaining_ > 0U) ? 1.0f : 0.0f;
        if (remaining_ > 0U) {
            --remaining_;
        }
    }
}

} // namespace neurons::engine::nodes
