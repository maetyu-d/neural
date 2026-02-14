#include "SamplePlayerWavNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

namespace {
constexpr float kSchmittLow = 0.3f;
constexpr float kSchmittHigh = 0.7f;
}

void SamplePlayerWavNode::setBaseRate(float rate) {
    baseRate_ = std::clamp(rate, 0.01f, 8.0f);
}

void SamplePlayerWavNode::setCvOctaves(float octaves) {
    cvOctaves_ = std::clamp(octaves, -4.0f, 4.0f);
}

void SamplePlayerWavNode::setClip(std::vector<float> samples, double sourceRate, std::string name) {
    clip_ = std::move(samples);
    sourceRate_ = std::max(1.0, sourceRate);
    clipName_ = std::move(name);
    position_ = 0.0;
    playing_ = false;
    triggerHigh_ = false;
}

const std::string& SamplePlayerWavNode::clipName() const {
    return clipName_;
}

void SamplePlayerWavNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    position_ = 0.0;
    playing_ = false;
    triggerHigh_ = false;
}

void SamplePlayerWavNode::process(std::span<const float> inA,
                                  std::span<const float> inB,
                                  std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    if (clip_.empty()) {
        std::fill_n(out.begin(), n, 0.0f);
        return;
    }

    const double srRatio = sourceRate_ / sampleRate_;
    const std::size_t clipSize = clip_.size();

    for (std::size_t i = 0; i < n; ++i) {
        const float trig = inA[i];
        if (!triggerHigh_ && trig >= kSchmittHigh) {
            triggerHigh_ = true;
            position_ = 0.0;
            playing_ = true;
        } else if (triggerHigh_ && trig <= kSchmittLow) {
            triggerHigh_ = false;
        }

        if (!playing_) {
            out[i] = 0.0f;
            continue;
        }

        if (position_ >= static_cast<double>(clipSize - 1)) {
            playing_ = false;
            out[i] = 0.0f;
            continue;
        }

        const std::size_t i0 = static_cast<std::size_t>(position_);
        const std::size_t i1 = std::min(i0 + 1, clipSize - 1);
        const float frac = static_cast<float>(position_ - static_cast<double>(i0));
        const float sample = clip_[i0] + (clip_[i1] - clip_[i0]) * frac;
        out[i] = sample;

        const float cv = inB[i];
        const float rate = std::clamp(baseRate_ * std::pow(2.0f, cv * cvOctaves_), 0.01f, 8.0f);
        position_ += srRatio * static_cast<double>(rate);
    }
}

} // namespace neurons::engine::nodes
