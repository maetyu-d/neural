#include "SampleHoldClockedNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void SampleHoldClockedNode::setLowHigh(float low, float high) {
    low_ = std::clamp(low, -1.0f, 1.0f);
    high_ = std::clamp(high, -1.0f, 1.0f);
    if (low_ > high_) {
        std::swap(low_, high_);
    }
}

void SampleHoldClockedNode::reset(double) {
    armed_ = true;
    held_ = 0.0f;
}

void SampleHoldClockedNode::process(std::span<const float> inA,
                                    std::span<const float> inB,
                                    std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float clk = inB[i];
        if (armed_ && clk >= high_) {
            held_ = inA[i];
            armed_ = false;
        } else if (!armed_ && clk <= low_) {
            armed_ = true;
        }
        out[i] = held_;
    }
}

} // namespace neurons::engine::nodes
