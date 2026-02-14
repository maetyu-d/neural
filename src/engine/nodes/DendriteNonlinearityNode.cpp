#include "DendriteNonlinearityNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void DendriteNonlinearityNode::setDrive(float drive) {
    drive_ = std::clamp(drive, 0.01f, 20.0f);
}

void DendriteNonlinearityNode::setBias(float bias) {
    bias_ = std::clamp(bias, -4.0f, 4.0f);
}

void DendriteNonlinearityNode::reset(double) {}

void DendriteNonlinearityNode::process(std::span<const float> inA,
                                       std::span<const float> inB,
                                       std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float x = inA[i] + inB[i] + bias_;
        out[i] = std::tanh(drive_ * x);
    }
}

} // namespace neurons::engine::nodes
