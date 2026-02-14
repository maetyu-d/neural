#include "FeedbackTapNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

void FeedbackTapNode::setLoopMs(float loopMs) {
    loopMs_ = std::clamp(loopMs, 5.0f, 2000.0f);
    updateLoopSamples();
}

void FeedbackTapNode::setReinject(float reinject) {
    reinject_ = std::clamp(reinject, -1.0f, 1.0f);
}

void FeedbackTapNode::setFreeze(bool freeze) {
    freezeManual_ = freeze;
}

void FeedbackTapNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    buffer_.assign(static_cast<std::size_t>(sampleRate_ * 4.0), 0.0f);
    head_ = 0;
    updateLoopSamples();
}

void FeedbackTapNode::process(std::span<const float> inA,
                              std::span<const float> inB,
                              std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    if (buffer_.empty()) {
        reset(sampleRate_);
    }

    for (std::size_t i = 0; i < n; ++i) {
        const bool freezeCv = inB[i] >= 0.5f;
        const bool freeze = freezeManual_ || freezeCv;
        const std::size_t read = (head_ + buffer_.size() - loopSamples_) % buffer_.size();
        const float tapped = buffer_[read];
        const float dry = inA[i];
        out[i] = dry + reinject_ * tapped;

        if (!freeze) {
            buffer_[head_] = dry;
        }
        head_ = (head_ + 1) % buffer_.size();
    }
}

void FeedbackTapNode::updateLoopSamples() {
    const auto samples = static_cast<std::size_t>((loopMs_ * 0.001f) * static_cast<float>(sampleRate_));
    const std::size_t maxLoop = std::max<std::size_t>(1, buffer_.empty() ? 1 : buffer_.size() - 1);
    loopSamples_ = std::clamp<std::size_t>(samples, 1, maxLoop);
}

} // namespace neurons::engine::nodes
