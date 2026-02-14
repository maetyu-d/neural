#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class MembraneLeakCapNode final : public NodeProcessor {
public:
    void setTauMs(float tauMs);
    void setLeak(float leak);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    float tauMs_{20.0f};
    float leak_{0.01f};
    float alpha_{0.001f};
    float state_{0.0f};
};

} // namespace neurons::engine::nodes
