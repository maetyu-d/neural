#include "AllpassNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void AllpassNode::setDelayMs(float delayMs) {
    delayMs_ = std::clamp(delayMs, 0.1f, 40.0f);
    updateDelaySamples();
}

void AllpassNode::setFeedback(float feedback) {
    feedback_ = std::clamp(feedback, -0.99f, 0.99f);
}

void AllpassNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    buffer_.assign(4096, 0.0f);
    write_ = 0;
    updateDelaySamples();
}

void AllpassNode::process(std::span<const float> inA,
                          std::span<const float>,
                          std::span<float> out) {
    if (buffer_.empty()) {
        buffer_.assign(4096, 0.0f);
        write_ = 0;
        updateDelaySamples();
    }

    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        const auto read = (write_ + buffer_.size() - delaySamples_) % buffer_.size();
        const float delayed = buffer_[read];
        const float x = inA[i];

        const float y = -feedback_ * x + delayed;
        buffer_[write_] = x + feedback_ * y;

        out[i] = y;
        write_ = (write_ + 1) % buffer_.size();
    }
}

void AllpassNode::updateDelaySamples() {
    const auto samples = static_cast<std::size_t>((delayMs_ * 0.001f) * static_cast<float>(sampleRate_));
    delaySamples_ = std::clamp<std::size_t>(samples, 1, 4095);
}

} // namespace neurons::engine::nodes
