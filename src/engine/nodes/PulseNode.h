#pragma once

#include "NodeProcessor.h"

#include <cstdint>

namespace neurons::engine::nodes {

class PulseNode final : public NodeProcessor {
public:
    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    std::uint32_t remaining_{0};
    std::uint32_t pulseSamples_{96};
};

} // namespace neurons::engine::nodes
