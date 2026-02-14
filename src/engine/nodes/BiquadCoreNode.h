#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class BiquadCoreNode final : public NodeProcessor {
public:
    void setCutoffHz(float cutoffHz);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    float cutoffHz_{1200.0f};
    float z1_{0.0f};
    float z2_{0.0f};
};

} // namespace neurons::engine::nodes
