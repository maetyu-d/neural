#include "CombFilterNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void CombFilterNode::setDelayMs(float delayMs) {
    delayMs_ = std::clamp(delayMs, 0.2f, 80.0f);
    updateDelay();
}

void CombFilterNode::setFeedback(float feedback) {
    feedback_ = std::clamp(feedback, -0.99f, 0.99f);
}

void CombFilterNode::setDamping(float damping) {
    damping_ = std::clamp(damping, 0.0f, 0.99f);
}

void CombFilterNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    buffer_.assign(8192, 0.0f);
    write_ = 0;
    lpState_ = 0.0f;
    updateDelay();
}

void CombFilterNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    if (buffer_.empty()) {
        buffer_.assign(8192, 0.0f);
    }

    for (std::size_t i = 0; i < n; ++i) {
        const float mod = std::clamp(inB[i], -1.0f, 1.0f);
        const float dynDelayMs = std::clamp(delayMs_ * (1.0f + 0.6f * mod), 0.2f, 80.0f);
        const auto dynSamples = static_cast<std::size_t>((dynDelayMs * 0.001f) * static_cast<float>(sampleRate_));
        const auto tap = std::clamp<std::size_t>(dynSamples, 1, 8191);
        const float dynFeedback = std::clamp(feedback_ + (0.2f * mod), -0.99f, 0.99f);
        const float dynDamping = std::clamp(damping_ + (0.2f * mod), 0.0f, 0.99f);
        const auto read = (write_ + buffer_.size() - tap) % buffer_.size();
        const float delayed = buffer_[read];
        lpState_ += dynDamping * (delayed - lpState_);
        const float y = delayed;
        const float fb = lpState_ * dynFeedback;
        buffer_[write_] = inA[i] + fb;
        out[i] = y;
        write_ = (write_ + 1) % buffer_.size();
    }
}

void CombFilterNode::updateDelay() {
    const auto samples = static_cast<std::size_t>((delayMs_ * 0.001f) * static_cast<float>(sampleRate_));
    delaySamples_ = std::clamp<std::size_t>(samples, 1, 8191);
}

} // namespace neurons::engine::nodes
