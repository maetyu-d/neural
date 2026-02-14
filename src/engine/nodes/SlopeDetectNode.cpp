#include "SlopeDetectNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void SlopeDetectNode::setThreshold(float threshold) {
    threshold_ = std::max(std::abs(threshold), 1.0e-8f);
}

void SlopeDetectNode::reset(double) {
    previous_ = 0.0f;
}

void SlopeDetectNode::process(std::span<const float> inA,
                              std::span<const float>,
                              std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        const float delta = inA[i] - previous_;
        if (delta > threshold_) {
            out[i] = 1.0f;
        } else if (delta < -threshold_) {
            out[i] = -1.0f;
        } else {
            out[i] = 0.0f;
        }
        previous_ = inA[i];
    }
}

} // namespace neurons::engine::nodes
