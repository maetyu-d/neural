#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class SampleHoldQuantizedNode final : public NodeProcessor {
public:
    void setThreshold(float threshold);
    void setSteps(float steps);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float threshold_{0.5f};
    int steps_{12};
    float held_{0.0f};
};

} // namespace neurons::engine::nodes
