#include "PulseNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void PulseNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    remaining_ = 0;
    pulseSamples_ = static_cast<std::uint32_t>(sampleRate_ * 0.002);
    if (pulseSamples_ < 1) {
        pulseSamples_ = 1;
    }
}

void PulseNode::process(std::span<const float> inA,
                        std::span<const float>,
                        std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (inA[i] > 0.8f) {
            remaining_ = pulseSamples_;
        }
        out[i] = (remaining_ > 0) ? 1.0f : 0.0f;
        if (remaining_ > 0) {
            --remaining_;
        }
    }
}

} // namespace neurons::engine::nodes
