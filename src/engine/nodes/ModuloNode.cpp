#include "ModuloNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

namespace {
constexpr float kMinModulus = 1.0e-6f;
}

void ModuloNode::setModulus(float modulus) {
    modulus_ = std::max(modulus, kMinModulus);
}

void ModuloNode::reset(double) {}

void ModuloNode::process(std::span<const float> inA,
                         std::span<const float> inB,
                         std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float modInput = std::abs(inB[i]) > kMinModulus ? std::abs(inB[i]) : modulus_;
        const float wrapped = inA[i] - modInput * std::floor(inA[i] / modInput);
        out[i] = wrapped;
    }
}

} // namespace neurons::engine::nodes
