#pragma once

#include "GraphModel.h"

namespace neurons::engine::core {

class TopologicalScheduler {
public:
    ScheduledGraph buildSchedule(const GraphModel& graph) const;
};

} // namespace neurons::engine::core
