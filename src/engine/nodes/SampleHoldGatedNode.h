#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class SampleHoldGatedNode final : public NodeProcessor {
public:
    void setThreshold(float threshold);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float threshold_{0.5f};
    float held_{0.0f};
};

} // namespace neurons::engine::nodes
