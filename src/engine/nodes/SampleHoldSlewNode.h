#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class SampleHoldSlewNode final : public NodeProcessor {
public:
    void setLowHigh(float low, float high);
    void setSlewMs(float slewMs);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    float low_{0.3f};
    float high_{0.7f};
    float slewMs_{8.0f};
    float alpha_{0.02f};
    bool armed_{true};
    float target_{0.0f};
    float held_{0.0f};

    void updateAlpha();
};

} // namespace neurons::engine::nodes
