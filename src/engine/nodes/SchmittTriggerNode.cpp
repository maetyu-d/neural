#include "SchmittTriggerNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void SchmittTriggerNode::setThreshold(float threshold) {
    threshold_ = std::clamp(threshold, -1.0f, 1.0f);
}

void SchmittTriggerNode::setHysteresis(float hysteresis) {
    hysteresis_ = std::clamp(hysteresis, 0.0f, 1.5f);
}

void SchmittTriggerNode::reset(double) {
    high_ = false;
}

void SchmittTriggerNode::process(std::span<const float> inA,
                                 std::span<const float> inB,
                                 std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float t = threshold_ + inB[i];
        const float hi = t + 0.5f * hysteresis_;
        const float lo = t - 0.5f * hysteresis_;
        const float x = inA[i];
        if (!high_ && x >= hi) {
            high_ = true;
        } else if (high_ && x <= lo) {
            high_ = false;
        }
        out[i] = high_ ? 1.0f : 0.0f;
    }
}

} // namespace neurons::engine::nodes
