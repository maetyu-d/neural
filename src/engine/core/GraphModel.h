#pragma once

#include "GraphTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace neurons::engine::core {

struct ValidationResult {
    bool ok{true};
    std::string error;
    bool requiresExplicitConverter{false};
};

class GraphModel {
public:
    bool addNode(NodeSpec node);
    bool removeNode(NodeId nodeId);
    void clear();
    bool replaceNodeInputs(NodeId nodeId, std::vector<PortSpec> inputs);

    ValidationResult canConnect(const Connection& connection) const;
    ValidationResult addConnection(const Connection& connection);
    bool removeConnection(const Connection& connection);

    const std::unordered_map<NodeId, NodeSpec>& nodes() const;
    const std::vector<Connection>& connections() const;
    const NodeSpec* getNode(NodeId nodeId) const;
    std::uint64_t revision() const;

    std::vector<NodeId> downstream(NodeId source) const;

private:
    std::optional<PortSpec> findOutputPort(NodeId nodeId, PortIndex port) const;
    std::optional<PortSpec> findInputPort(NodeId nodeId, PortIndex port) const;

    std::unordered_map<NodeId, NodeSpec> nodes_;
    std::vector<Connection> connections_;
    std::uint64_t revision_{0};
};

} // namespace neurons::engine::core
