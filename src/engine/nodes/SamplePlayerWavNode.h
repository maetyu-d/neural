#pragma once

#include "NodeProcessor.h"

#include <vector>
#include <string>

namespace neurons::engine::nodes {

class SamplePlayerWavNode final : public NodeProcessor {
public:
    void setBaseRate(float rate);
    void setCvOctaves(float octaves);
    void setClip(std::vector<float> samples, double sourceRate, std::string name);

    const std::string& clipName() const;

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    double sampleRate_{48000.0};
    double sourceRate_{48000.0};
    float baseRate_{1.0f};
    float cvOctaves_{1.0f};

    std::vector<float> clip_;
    std::string clipName_;
    double position_{0.0};
    bool playing_{false};
    bool triggerHigh_{false};
};

} // namespace neurons::engine::nodes
