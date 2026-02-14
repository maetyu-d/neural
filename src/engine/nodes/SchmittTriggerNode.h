#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class SchmittTriggerNode final : public NodeProcessor {
public:
    void setThreshold(float threshold);
    void setHysteresis(float hysteresis);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float threshold_{0.5f};
    float hysteresis_{0.2f};
    bool high_{false};
};

} // namespace neurons::engine::nodes
