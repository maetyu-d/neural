#pragma once

#include "NodeProcessor.h"

namespace neurons::engine::nodes {

class DendriteNonlinearityNode final : public NodeProcessor {
public:
    void setDrive(float drive);
    void setBias(float bias);

    void reset(double sampleRate) override;
    void process(std::span<const float> inA,
                 std::span<const float> inB,
                 std::span<float> out) override;

private:
    float drive_{1.0f};
    float bias_{0.0f};
};

} // namespace neurons::engine::nodes
