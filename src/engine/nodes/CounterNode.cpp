#include "CounterNode.h"

#include <algorithm>

namespace neurons::engine::nodes {

namespace {
constexpr float kSchmittHigh = 0.7f;
constexpr float kSchmittLow = 0.3f;
}

void CounterNode::setRange(float minValue, float maxValue) {
    if (maxValue < minValue) {
        std::swap(minValue, maxValue);
    }
    minValue_ = minValue;
    maxValue_ = maxValue;
    current_ = std::clamp(current_, minValue_, maxValue_);
}

void CounterNode::setWrapMode(bool wrapMode) {
    wrapMode_ = wrapMode;
}

void CounterNode::reset(double) {
    current_ = minValue_;
    clockHigh_ = false;
    resetHigh_ = false;
}

void CounterNode::step() {
    const float next = current_ + 1.0f;
    if (wrapMode_) {
        current_ = (next > maxValue_) ? minValue_ : next;
    } else {
        current_ = std::min(next, maxValue_);
    }
}

void CounterNode::process(std::span<const float> inA,
                          std::span<const float> inB,
                          std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float clock = inA[i];
        const float reset = inB[i];
        bool resetTriggered = false;

        if (!resetHigh_ && reset >= kSchmittHigh) {
            resetHigh_ = true;
            current_ = minValue_;
            resetTriggered = true;
        } else if (resetHigh_ && reset <= kSchmittLow) {
            resetHigh_ = false;
        }

        if (!resetTriggered && !clockHigh_ && clock >= kSchmittHigh) {
            clockHigh_ = true;
            step();
        } else if (clockHigh_ && clock <= kSchmittLow) {
            clockHigh_ = false;
        }

        out[i] = current_;
    }
}

} // namespace neurons::engine::nodes
