#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class OscillatorNode final : public NodeProcessor {
public:
    enum class Waveform : int {
        Sine = 0,
        Triangle = 1,
        Saw = 2,
        Square = 3,
    };

    void setFrequencyHz(float hz);
    void setWaveform(float waveformIndex);
    void processWithWaveSelect(std::span<const float> inFreq,
                               std::span<const float> inPhase,
                               std::span<const float> inWaveSelect,
                               std::span<float> out);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float renderSample() const;

    double sampleRate_{48000.0};
    float frequencyHz_{220.0f};
    float phase_{0.0f};
    Waveform waveform_{Waveform::Sine};
    bool waveSelectHigh_{false};
};

} // namespace neurons::engine::nodes
