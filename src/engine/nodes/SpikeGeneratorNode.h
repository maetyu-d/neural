#pragma once

#include "NodeProcessor.h"

#include <cstdint>

namespace neurons::engine::nodes {

class SpikeGeneratorNode final : public NodeProcessor {
public:
    void setThreshold(float threshold);
    void setPulseMs(float ms);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    float threshold_{0.5f};
    float pulseMs_{1.0f};
    std::uint32_t pulseSamples_{48};
    std::uint32_t remaining_{0};
    bool high_{false};
};

} // namespace neurons::engine::nodes
