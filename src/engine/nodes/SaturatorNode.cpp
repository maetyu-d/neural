#include "SaturatorNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void SaturatorNode::setDrive(float drive) {
    drive_ = std::max(0.001f, drive);
}

void SaturatorNode::reset(double) {}

void SaturatorNode::process(std::span<const float> inA,
                            std::span<const float>,
                            std::span<float> out) {
    const auto n = std::min(inA.size(), out.size());
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = std::tanh(inA[i] * drive_);
    }
}

} // namespace neurons::engine::nodes
