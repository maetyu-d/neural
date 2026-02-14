#pragma once

#include "NodeProcessor.h"

#include <cstdint>

namespace neurons::engine::nodes {

class RandomGateNode final : public NodeProcessor {
public:
    void setProbability(float probability);
    void setPulseMs(float pulseMs);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float nextUnit();

    double sampleRate_{48000.0};
    float probability_{0.5f};
    std::uint32_t pulseSamples_{1};
    std::uint32_t remaining_{0};
    bool clockHigh_{false};
    std::uint32_t rngState_{0x12345678u};
};

} // namespace neurons::engine::nodes
