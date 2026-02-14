#pragma once

#include "EngineConfig.h"

#include <cstdint>

namespace neurons::engine::dsp {

struct CycleSolveStats {
    std::uint32_t capHits{};
    std::uint32_t samplesProcessed{};
};

class CycleSolver {
public:
    explicit CycleSolver(CycleConfig config = {});

    const CycleConfig& config() const;
    void setHighPrecisionEnabled(bool enabled);

    CycleSolveStats processSamples(int numSamples, const std::uint8_t* convergedMask);

private:
    CycleConfig config_;
};

} // namespace neurons::engine::dsp
