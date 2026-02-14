#include "CycleSolver.h"

namespace neurons::engine::dsp {

CycleSolver::CycleSolver(CycleConfig config)
    : config_(config) {}

const CycleConfig& CycleSolver::config() const {
    return config_;
}

void CycleSolver::setHighPrecisionEnabled(bool enabled) {
    config_.highPrecisionEnabled = enabled;
}

CycleSolveStats CycleSolver::processSamples(int numSamples, const std::uint8_t* convergedMask) {
    CycleSolveStats stats;
    stats.samplesProcessed = static_cast<std::uint32_t>(numSamples);

    if (convergedMask == nullptr) {
        return stats;
    }

    for (int i = 0; i < numSamples; ++i) {
        if (convergedMask[i] == 0U) {
            ++stats.capHits;
        }
    }

    return stats;
}

} // namespace neurons::engine::dsp
