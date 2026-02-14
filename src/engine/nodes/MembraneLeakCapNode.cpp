#include "MembraneLeakCapNode.h"

#include <algorithm>
#include <cmath>

namespace neurons::engine::nodes {

void MembraneLeakCapNode::setTauMs(float tauMs) {
    tauMs_ = std::max(tauMs, 0.1f);
    const double tauSamples = std::max(1.0, (sampleRate_ * static_cast<double>(tauMs_) * 0.001));
    alpha_ = static_cast<float>(1.0 - std::exp(-1.0 / tauSamples));
}

void MembraneLeakCapNode::setLeak(float leak) {
    leak_ = std::clamp(leak, 0.0f, 1.0f);
}

void MembraneLeakCapNode::reset(double sampleRate) {
    sampleRate_ = std::max(1.0, sampleRate);
    state_ = 0.0f;
    setTauMs(tauMs_);
}

void MembraneLeakCapNode::process(std::span<const float> inA,
                                  std::span<const float> inB,
                                  std::span<float> out) {
    const auto n = std::min({inA.size(), inB.size(), out.size()});
    for (std::size_t i = 0; i < n; ++i) {
        const float drive = inA[i] + (0.5f * inB[i]);
        state_ += alpha_ * (drive - state_);
        state_ *= (1.0f - leak_);
        out[i] = state_;
    }
}

} // namespace neurons::engine::nodes
