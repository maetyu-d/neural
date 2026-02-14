#include "SampleHoldSlewNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void SampleHoldSlewNode::setLowHigh(float low, float high) {
    low_ = std::clamp(low, -1.0f, 1.0f);
    high_ = std::clamp(high, -1.0f, 1.0f);
    if (low_ > high_) {
        std::swap(low_, high_);
    }
}

void SampleHoldSlewNode::setSlewMs(float slewMs) {
    slewMs_ = std::clamp(slewMs, 0.02f, 200.0f);
    updateAlpha();
}

void SampleHoldSlewNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    armed_ = true;
    target_ = 0.0f;
    held_ = 0.0f;
    updateAlpha();
}

void SampleHoldSlewNode::process(std::span<const float> inA,
                                 std::span<const float> inB,
                                 std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float clk = inB[i];
        if (armed_ && clk >= high_) {
            target_ = inA[i];
            armed_ = false;
        } else if (!armed_ && clk <= low_) {
            armed_ = true;
        }
        held_ += alpha_ * (target_ - held_);
        out[i] = held_;
    }
}

void SampleHoldSlewNode::updateAlpha() {
    const double tau = std::max(2.0e-5, static_cast<double>(slewMs_) * 0.001);
    alpha_ = static_cast<float>(1.0 - std::exp(-1.0 / (sampleRate_ * tau)));
}

} // namespace neurons::engine::nodes
