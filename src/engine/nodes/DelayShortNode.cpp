#include "DelayShortNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void DelayShortNode::setDelayMs(float delayMs) {
    delayMs_ = std::clamp(delayMs, 0.1f, 40.0f);
    const auto samples = static_cast<std::size_t>((delayMs_ * 0.001f) * static_cast<float>(sampleRate_));
    delaySamples_ = std::clamp<std::size_t>(samples, 1, 1023);
}

void DelayShortNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    buffer_.assign(1024, 0.0f);
    write_ = 0;
    setDelayMs(delayMs_);
}

void DelayShortNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    if (buffer_.empty()) {
        buffer_.assign(1024, 0.0f);
    }

    for (std::size_t i = 0; i < n; ++i) {
        const float cvMs = std::clamp(inB[i], -10.0f, 10.0f);
        const float dynamicMs = std::clamp(delayMs_ + cvMs, 0.1f, 40.0f);
        const auto dynamicSamples = static_cast<std::size_t>((dynamicMs * 0.001f) * static_cast<float>(sampleRate_));
        const auto tap = std::clamp<std::size_t>(dynamicSamples, 1, 1023);
        const std::size_t read = (write_ + buffer_.size() - tap) % buffer_.size();
        out[i] = buffer_[read];
        buffer_[write_] = inA[i];
        write_ = (write_ + 1) % buffer_.size();
    }
}

} // namespace neurons::engine::nodes
