#pragma once

#include <cstdint>
#include <string_view>

namespace neurons::engine::core {

enum class SignalType : std::uint8_t {
    BipolarAudio,
    UnipolarAudio,
    GateAudio,
    TriggerAudio,
    PhaseAudio,
    HzAudio,
    TimeAudio
};

inline std::string_view toString(SignalType type) {
    switch (type) {
    case SignalType::BipolarAudio:
        return "BipolarAudio";
    case SignalType::UnipolarAudio:
        return "UnipolarAudio";
    case SignalType::GateAudio:
        return "GateAudio";
    case SignalType::TriggerAudio:
        return "TriggerAudio";
    case SignalType::PhaseAudio:
        return "PhaseAudio";
    case SignalType::HzAudio:
        return "HzAudio";
    case SignalType::TimeAudio:
        return "TimeAudio";
    }
    return "Unknown";
}

inline bool canImplicitlyConvert(SignalType from, SignalType to) {
    if (from == to) {
        return true;
    }

    if ((from == SignalType::GateAudio || from == SignalType::TriggerAudio) &&
        to == SignalType::UnipolarAudio) {
        return true;
    }

    if ((from == SignalType::UnipolarAudio && to == SignalType::BipolarAudio) ||
        (from == SignalType::BipolarAudio && to == SignalType::UnipolarAudio)) {
        return true;
    }

    return false;
}

} // namespace neurons::engine::core
