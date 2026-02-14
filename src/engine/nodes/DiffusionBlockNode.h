#pragma once

#include "NodeProcessor.h"

#include <vector>

namespace neurons::engine::nodes {

class DiffusionBlockNode final : public NodeProcessor {
public:
    void setSizeMs(float sizeMs);
    void setFeedback(float feedback);
    void setMix(float mix);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    void updateDelays();

    double sampleRate_{48000.0};
    float sizeMs_{12.0f};
    float feedback_{0.6f};
    float mix_{0.5f};
    std::vector<float> delayA_;
    std::vector<float> delayB_;
    std::size_t writeA_{0};
    std::size_t writeB_{0};
    std::size_t delaySamplesA_{1};
    std::size_t delaySamplesB_{1};
};

} // namespace neurons::engine::nodes
