#pragma once

#include "NodeProcessor.h"

#include <array>
#include <vector>

namespace neurons::engine::nodes {

class AllpassBankNode final : public NodeProcessor {
public:
    void setBaseDelayMs(float delayMs);
    void setFeedback(float feedback);
    void setSpread(float spread);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    static constexpr std::size_t kStages = 4;

    void updateDelays();

    double sampleRate_{48000.0};
    float baseDelayMs_{4.0f};
    float feedback_{0.6f};
    float spread_{0.35f};
    std::array<std::vector<float>, kStages> buffers_{};
    std::array<std::size_t, kStages> writes_{};
    std::array<std::size_t, kStages> delays_{};
};

} // namespace neurons::engine::nodes
