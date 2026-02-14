#include "BytebeatJsNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

namespace {
constexpr float kSchmittLow = 0.3f;
constexpr float kSchmittHigh = 0.7f;
}

void BytebeatJsNode::setExpression(const std::string& expression) {
    expression_ = expression.empty() ? "(t * ((t >> 5) | (t >> 8))) & 255" : expression;
    compileExpression();
}

void BytebeatJsNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    triggerHigh_ = false;
    tCounter_ = 0.0;
    compileExpression();
}

void BytebeatJsNode::process(std::span<const float> inA,
                             std::span<const float> inB,
                             std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    if (!compiled_) {
        std::fill_n(out.begin(), n, 0.0f);
        return;
    }

    for (std::size_t i = 0; i < n; ++i) {
        const float trig = inA[i];
        if (!triggerHigh_ && trig >= kSchmittHigh) {
            triggerHigh_ = true;
            tCounter_ = 0.0;
        } else if (triggerHigh_ && trig <= kSchmittLow) {
            triggerHigh_ = false;
        }

        const float rate = std::clamp(std::pow(2.0f, inB[i] * 2.0f), 0.0625f, 16.0f);
        const auto ti = static_cast<std::uint64_t>(std::max(0.0, tCounter_));
        const auto v = js_->evaluate("bb(" + juce::String(static_cast<juce::int64>(ti)) + "," +
                                     juce::String(sampleRate_, 1) + ")");

        float y = 0.0f;
        if (v.isInt() || v.isInt64() || v.isDouble()) {
            const double d = static_cast<double>(v);
            const double wrapped = std::fmod(d, 256.0);
            const double u8 = wrapped < 0.0 ? (wrapped + 256.0) : wrapped;
            y = static_cast<float>((u8 / 127.5) - 1.0);
        }

        out[i] = std::clamp(y, -1.0f, 1.0f);
        tCounter_ += rate;
    }
}

void BytebeatJsNode::compileExpression() {
    compiled_ = false;
    js_ = std::make_unique<juce::JavascriptEngine>();
    const juce::String script = "function bb(t,sr){ return (" + juce::String(expression_) + "); }";
    if (js_->execute(script).failed()) {
        return;
    }
    compiled_ = true;
}

} // namespace neurons::engine::nodes
