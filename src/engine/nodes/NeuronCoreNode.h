#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class NeuronCoreNode final : public NodeProcessor {
public:
    struct Params {
        float gain{1.0f};
        float bias{0.0f};
        float tauMs{20.0f};
        float leak{0.05f};
        float threshold{0.25f};
        float hysteresis{0.05f};
        float satDrive{1.0f};
    };

    void setParams(const Params& params);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    Params params_{};
    double sampleRate_{48000.0};
    float state_{0.0f};
    bool thresholdLatched_{false};
};

} // namespace neurons::engine::nodes
