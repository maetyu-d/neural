#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class WaveshaperNode final : public NodeProcessor {
public:
    void setDrive(float drive);
    void setCurve(float curve);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float drive_{1.0f};
    float curve_{0.5f};
};

} // namespace neurons::engine::nodes
