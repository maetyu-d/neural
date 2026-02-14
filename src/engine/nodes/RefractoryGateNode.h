#pragma once

#include "NodeProcessor.h"

#include <cstdint>

namespace neurons::engine::nodes {

class RefractoryGateNode final : public NodeProcessor {
public:
    void setRefractoryMs(float ms);
    void setPulseMs(float ms);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    float refractoryMs_{30.0f};
    float pulseMs_{1.0f};
    std::uint32_t refractorySamples_{1440};
    std::uint32_t pulseSamples_{48};
    std::uint32_t refractoryRemaining_{0};
    std::uint32_t pulseRemaining_{0};
    bool inputHigh_{false};
};

} // namespace neurons::engine::nodes
