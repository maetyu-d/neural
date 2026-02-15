#pragma once

#include "NodeProcessor.h"

#include <cstdint>

namespace neurons::engine::nodes {

class ADEnvelopeNode final : public NodeProcessor {
public:
    void setAttackMs(float ms);
    void setDecayMs(float ms);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    enum class Stage : std::uint8_t {
        Idle = 0,
        Attack,
        Decay
    };

    double sampleRate_{48000.0};
    float attackMs_{8.0f};
    float decayMs_{120.0f};
    float level_{0.0f};
    bool inputHigh_{false};
    Stage stage_{Stage::Idle};
};

} // namespace neurons::engine::nodes
