#include "GraphModel.h"

#include <algorithm>
#include <unordered_set>

namespace neurons::engine::core {

bool GraphModel::addNode(NodeSpec node) {
    const auto id = node.id;
    const auto inserted = nodes_.emplace(id, std::move(node)).second;
    if (inserted) {
        ++revision_;
    }
    return inserted;
}

bool GraphModel::removeNode(NodeId nodeId) {
    const auto erased = nodes_.erase(nodeId);
    if (erased == 0U) {
        return false;
    }

    connections_.erase(std::remove_if(connections_.begin(),
                                      connections_.end(),
                                      [nodeId](const Connection& c) {
                                          return c.fromNode == nodeId || c.toNode == nodeId;
                                      }),
                       connections_.end());
    ++revision_;
    return true;
}

void GraphModel::clear() {
    if (nodes_.empty() && connections_.empty()) {
        return;
    }
    nodes_.clear();
    connections_.clear();
    ++revision_;
}

bool GraphModel::replaceNodeInputs(NodeId nodeId, std::vector<PortSpec> inputs) {
    auto nodeIt = nodes_.find(nodeId);
    if (nodeIt == nodes_.end()) {
        return false;
    }

    bool changed = nodeIt->second.inputs.size() != inputs.size();
    if (!changed) {
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            const auto& a = nodeIt->second.inputs[i];
            const auto& b = inputs[i];
            if (a.index != b.index || a.type != b.type || a.name != b.name) {
                changed = true;
                break;
            }
        }
    }

    std::unordered_set<PortIndex> validPorts;
    validPorts.reserve(inputs.size());
    for (const auto& p : inputs) {
        validPorts.insert(p.index);
    }

    const auto oldSize = connections_.size();
    connections_.erase(std::remove_if(connections_.begin(),
                                      connections_.end(),
                                      [&](const Connection& c) {
                                          return c.toNode == nodeId &&
                                                 validPorts.find(c.toPort) == validPorts.end();
                                      }),
                       connections_.end());
    if (connections_.size() != oldSize) {
        changed = true;
    }

    if (!changed) {
        return false;
    }

    nodeIt->second.inputs = std::move(inputs);
    ++revision_;
    return true;
}

ValidationResult GraphModel::canConnect(const Connection& connection) const {
    if (connection.fromNode == connection.toNode) {
        return {false, "self-connection is not allowed", false};
    }

    const auto fromPort = findOutputPort(connection.fromNode, connection.fromPort);
    const auto toPort = findInputPort(connection.toNode, connection.toPort);

    if (!fromPort.has_value() || !toPort.has_value()) {
        return {false, "invalid source or destination port index", false};
    }

    if (fromPort->type == toPort->type || canImplicitlyConvert(fromPort->type, toPort->type)) {
        return {true, {}, false};
    }

    const auto fromType = std::string(toString(fromPort->type));
    const auto toType = std::string(toString(toPort->type));
    return {false, fromType + " -> " + toType + " requires explicit converter", true};
}

ValidationResult GraphModel::addConnection(const Connection& connection) {
    const auto result = canConnect(connection);
    if (!result.ok) {
        return result;
    }

    const auto exists = std::any_of(connections_.begin(), connections_.end(), [&](const Connection& c) {
        return c.fromNode == connection.fromNode && c.fromPort == connection.fromPort &&
               c.toNode == connection.toNode && c.toPort == connection.toPort;
    });

    if (!exists) {
        connections_.push_back(connection);
        ++revision_;
    }

    return {true, {}, false};
}

bool GraphModel::removeConnection(const Connection& connection) {
    const auto oldSize = connections_.size();
    connections_.erase(std::remove_if(connections_.begin(),
                                      connections_.end(),
                                      [&](const Connection& c) {
                                          return c.fromNode == connection.fromNode &&
                                                 c.fromPort == connection.fromPort &&
                                                 c.toNode == connection.toNode &&
                                                 c.toPort == connection.toPort;
                                      }),
                       connections_.end());
    const auto changed = connections_.size() != oldSize;
    if (changed) {
        ++revision_;
    }
    return changed;
}

const std::unordered_map<NodeId, NodeSpec>& GraphModel::nodes() const {
    return nodes_;
}

const std::vector<Connection>& GraphModel::connections() const {
    return connections_;
}

const NodeSpec* GraphModel::getNode(NodeId nodeId) const {
    const auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::uint64_t GraphModel::revision() const {
    return revision_;
}

std::vector<NodeId> GraphModel::downstream(NodeId source) const {
    std::vector<NodeId> out;
    for (const auto& c : connections_) {
        if (c.fromNode == source) {
            out.push_back(c.toNode);
        }
    }
    return out;
}

std::optional<PortSpec> GraphModel::findOutputPort(NodeId nodeId, PortIndex port) const {
    const auto nodeIt = nodes_.find(nodeId);
    if (nodeIt == nodes_.end()) {
        return std::nullopt;
    }

    const auto& outputs = nodeIt->second.outputs;
    const auto it = std::find_if(outputs.begin(), outputs.end(), [port](const PortSpec& p) {
        return p.index == port;
    });
    if (it == outputs.end()) {
        return std::nullopt;
    }

    return *it;
}

std::optional<PortSpec> GraphModel::findInputPort(NodeId nodeId, PortIndex port) const {
    const auto nodeIt = nodes_.find(nodeId);
    if (nodeIt == nodes_.end()) {
        return std::nullopt;
    }

    const auto& inputs = nodeIt->second.inputs;
    const auto it = std::find_if(inputs.begin(), inputs.end(), [port](const PortSpec& p) {
        return p.index == port;
    });
    if (it == inputs.end()) {
        return std::nullopt;
    }

    return *it;
}

} // namespace neurons::engine::core
