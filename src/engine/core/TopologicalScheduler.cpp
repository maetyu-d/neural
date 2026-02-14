#include "TopologicalScheduler.h"

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <vector>

namespace neurons::engine::core {

ScheduledGraph TopologicalScheduler::buildSchedule(const GraphModel& graph) const {
    ScheduledGraph out;

    std::unordered_map<NodeId, int> indegree;
    std::unordered_map<NodeId, std::vector<NodeId>> adjacency;
    std::vector<NodeId> nodeIds;
    nodeIds.reserve(graph.nodes().size());

    for (const auto& [id, _] : graph.nodes()) {
        nodeIds.push_back(id);
        indegree[id] = 0;
    }
    std::sort(nodeIds.begin(), nodeIds.end());

    for (const auto& c : graph.connections()) {
        adjacency[c.fromNode].push_back(c.toNode);
        ++indegree[c.toNode];
    }
    for (auto& [_, edges] : adjacency) {
        std::sort(edges.begin(), edges.end());
    }

    std::queue<NodeId> q;
    for (const auto id : nodeIds) {
        const auto in = indegree[id];
        if (in == 0) {
            q.push(id);
        }
    }

    while (!q.empty()) {
        const auto current = q.front();
        q.pop();
        out.acyclicOrder.push_back(current);

        for (const auto n : adjacency[current]) {
            --indegree[n];
            if (indegree[n] == 0) {
                q.push(n);
            }
        }
    }

    for (const auto id : nodeIds) {
        const auto in = indegree[id];
        if (in > 0) {
            out.cyclicNodes.push_back(id);
        }
    }

    return out;
}

} // namespace neurons::engine::core
