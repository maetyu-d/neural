#include "AdaptiveThresholdNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

namespace {
constexpr float kHysteresis = 0.05f;
}

void AdaptiveThresholdNode::setBaseThreshold(float threshold) {
    baseThreshold_ = std::clamp(threshold, -2.0f, 2.0f);
}

void AdaptiveThresholdNode::setAdaptAmount(float amount) {
    adaptAmount_ = std::clamp(amount, 0.0f, 2.0f);
}

void AdaptiveThresholdNode::reset(double) {
    thresholdState_ = baseThreshold_;
    high_ = false;
}

void AdaptiveThresholdNode::process(std::span<const float> inA,
                                    std::span<const float> inB,
                                    std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float dynamicBase = baseThreshold_ + (0.25f * inB[i]);
        thresholdState_ += 0.0015f * (dynamicBase - thresholdState_);

        float spike = 0.0f;
        if (!high_ && inA[i] >= thresholdState_) {
            high_ = true;
            spike = 1.0f;
            thresholdState_ += adaptAmount_;
        } else if (high_ && inA[i] <= (thresholdState_ - kHysteresis)) {
            high_ = false;
        }

        out[i] = spike;
    }
}

} // namespace neurons::engine::nodes
