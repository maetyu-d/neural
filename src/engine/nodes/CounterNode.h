#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class CounterNode final : public NodeProcessor {
public:
    void setRange(float minValue, float maxValue);
    void setWrapMode(bool wrapMode);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    void step();

    float minValue_{0.0f};
    float maxValue_{15.0f};
    float current_{0.0f};
    bool wrapMode_{true};
    bool clockHigh_{false};
    bool resetHigh_{false};
};

} // namespace neurons::engine::nodes
