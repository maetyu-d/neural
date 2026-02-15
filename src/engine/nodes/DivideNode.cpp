#include "DivideNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

namespace {
constexpr float kMinDenominator = 1.0e-6f;
}

void DivideNode::reset(double) {}

void DivideNode::process(std::span<const float> inA,
                         std::span<const float> inB,
                         std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float d = inB[i];
        if (std::abs(d) < kMinDenominator) {
            out[i] = 0.0f;
        } else {
            out[i] = inA[i] / d;
        }
    }
}

} // namespace neurons::engine::nodes
