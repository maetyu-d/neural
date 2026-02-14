#pragma once

#include "NodeProcessor.h"

#include <cstdint>

namespace neurons::engine::nodes {

class BurstNeuronNode final : public NodeProcessor {
public:
    void setCount(float count);
    void setIntervalMs(float ms);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    float count_{3.0f};
    float intervalMs_{8.0f};
    std::uint32_t intervalSamples_{384};
    bool inputHigh_{false};
    int burstsRemaining_{0};
    std::uint32_t nextIn_{0};
};

} // namespace neurons::engine::nodes
