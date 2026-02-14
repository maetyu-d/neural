#pragma once

#include "NodeProcessor.h"

#include <juce_core/juce_core.h>
#include <juce_javascript/juce_javascript.h>

#include <memory>
#include <string>

namespace neurons::engine::nodes {

class BytebeatJsNode final : public NodeProcessor {
public:
    void setExpression(const std::string& expression);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    void compileExpression();

    double sampleRate_{48000.0};
    std::string expression_{"(t * ((t >> 5) | (t >> 8))) & 255"};
    bool compiled_{false};
    bool triggerHigh_{false};
    double tCounter_{0.0};
    std::unique_ptr<juce::JavascriptEngine> js_;
};

} // namespace neurons::engine::nodes
