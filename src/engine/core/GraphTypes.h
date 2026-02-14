#pragma once

#include "SignalTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace neurons::engine::core {

using NodeId = std::uint32_t;
using PortIndex = std::uint16_t;

struct PortSpec {
    PortIndex index{};
    SignalType type{};
    std::string name;
};

struct NodeSpec {
    NodeId id{};
    std::string typeName;
    std::vector<PortSpec> inputs;
    std::vector<PortSpec> outputs;
};

struct Connection {
    NodeId fromNode{};
    PortIndex fromPort{};
    NodeId toNode{};
    PortIndex toPort{};
};

struct ScheduledGraph {
    std::vector<NodeId> acyclicOrder;
    std::vector<NodeId> cyclicNodes;
};

} // namespace neurons::engine::core
