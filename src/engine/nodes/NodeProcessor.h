#pragma once

#include <span>

namespace neurons::engine::nodes {

class NodeProcessor {
public:
    virtual ~NodeProcessor() = default;

    virtual void reset(double sampleRate) = 0;
    virtual void process(std::span<const float> inA,
                         std::span<const float> inB,
                         std::span<float> out) = 0;
};

} // namespace neurons::engine::nodes
