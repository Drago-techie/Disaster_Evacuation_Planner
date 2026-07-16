#include "solvers.hpp"
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace Evac {

float EvacSolver::minHeuristicDistance(const Graph& graph, int nodeId, const std::vector<int>& safeZones) {
    const Node* uNode = graph.getNode(nodeId);
    if (!uNode || safeZones.empty()) return 0.0f;

    float minDist = 1e9f;
    for (int szId : safeZones) {
        const Node* szNode = graph.getNode(szId);
        if (szNode) {
            float dist = uNode->pos.distanceTo(szNode->pos);
            if (dist < minDist) minDist = dist;
        }
    }
    return minDist;
}

void EvacSolver::computeAnalytics(const Graph& graph, PathResult& res) {
    if (!res.found || res.pathNodes.empty()) {
        res.survivalRatePercent = 0.0f;
        res.estimatedClearanceTimeMins = 0.0f;

        std::vector<int> allSafeZones = graph.getSafeZoneIds();
        std::vector<int> validSafeZones = graph.getValidSafeZoneIds();

        if (!allSafeZones.empty() && validSafeZones.empty()) {
            res.bottleneckStatus = "ALL EXITS ENGULFED IN HAZARD";
        } else {
            res.bottleneckStatus = "Trapped - No Route to Exit";
        }
        return;
    }

    float totalHazardRisk = 0.0f;
    float maxCongestion = 0.0f;
    float maxHazard = 0.0f;
    int edgeCount = 0;

    for (size_t i = 0; i < res.pathNodes.size() - 1; i++) {
        int u = res.pathNodes[i];
        int v = res.pathNodes[i+1];

        for (const auto& edge : graph.getEdgesFrom(u)) {
            if (edge.toNode == v) {
                float radiusHeat = graph.getEdgeHazardRadiusInfluence(u, v);
                float combinedHazard = std::min(1.0f, edge.hazardLevel + radiusHeat);

                totalHazardRisk += combinedHazard * 2.0f + edge.congestion * 1.5f;
                if (edge.congestion > maxCongestion) maxCongestion = edge.congestion;
                if (combinedHazard > maxHazard) maxHazard = combinedHazard;
                edgeCount++;
                break;
            }
        }
    }

    // Check if destination safe node itself is compromised
    int destNodeId = res.pathNodes.back();
    if (graph.isNodeCompromisedByHazard(destNodeId)) {
        res.survivalRatePercent = 0.0f;
        res.estimatedClearanceTimeMins = 0.0f;
        res.bottleneckStatus = "DESTINATION EXIT COMPROMISED";
        return;
    }

    // Survival rate percentage (100% minus accumulated hazard/congestion exposure)
    float riskExposure = (edgeCount > 0) ? (totalHazardRisk / edgeCount) : 0.0f;
    res.survivalRatePercent = std::max(5.0f, 100.0f - riskExposure * 55.0f);

    // Clearance time in minutes (walking speed ~ 84m/min adjusted by corridor bottleneck factor)
    float baseTimeMins = res.totalDistance / 84.0f;
    float bottleneckFactor = 1.0f + 3.2f * maxCongestion + 2.8f * maxHazard;
    res.estimatedClearanceTimeMins = baseTimeMins * bottleneckFactor;

    if (maxCongestion > 0.5f || maxHazard > 0.5f) {
        res.bottleneckStatus = "High Bottleneck Risk";
    } else if (maxCongestion > 0.2f || maxHazard > 0.2f) {
        res.bottleneckStatus = "Moderate Flow";
    } else {
        res.bottleneckStatus = "Optimal Flow";
    }
}

// 1. BFS: Pure unweighted hop count search (ignores edge weights/hazards completely, but filters compromised safe exits)
PathResult EvacSolver::solveBFS(const Graph& graph, int startNodeId) {
    PathResult res;
    res.algorithmName = "BFS (Shortest Hop Count)";
    auto startTime = std::chrono::high_resolution_clock::now();

    const Node* startNode = graph.getNode(startNodeId);
    if (!startNode || startNode->type == NodeType::Blocked) {
        res.found = false;
        computeAnalytics(graph, res);
        return res;
    }

    std::vector<int> validSafeZones = graph.getValidSafeZoneIds();
    if (validSafeZones.empty()) {
        res.found = false;
        computeAnalytics(graph, res);
        auto endTime = std::chrono::high_resolution_clock::now();
        res.executionMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        return res;
    }

    std::unordered_set<int> safeZoneSet(validSafeZones.begin(), validSafeZones.end());

    if (safeZoneSet.count(startNodeId)) {
        res.found = true;
        res.pathNodes = {startNodeId};
        computeAnalytics(graph, res);
        auto endTime = std::chrono::high_resolution_clock::now();
        res.executionMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        return res;
    }

    std::queue<int> q;
    std::unordered_map<int, int> parent;
    std::unordered_map<int, const Edge*> edgeUsed;
    std::unordered_set<int> visited;

    q.push(startNodeId);
    visited.insert(startNodeId);

    int targetExit = -1;

    while (!q.empty()) {
        int curr = q.front();
        q.pop();
        res.nodesVisited++;

        SearchStep step;
        step.currentNodeId = curr;
        step.visitedNodes = std::vector<int>(visited.begin(), visited.end());

        std::queue<int> tempQ = q;
        while (!tempQ.empty()) {
            step.frontierNodes.push_back(tempQ.front());
            tempQ.pop();
        }

        int pCurr = curr;
        while (pCurr != startNodeId) {
            step.currentPath.push_back(pCurr);
            pCurr = parent[pCurr];
        }
        step.currentPath.push_back(startNodeId);
        std::reverse(step.currentPath.begin(), step.currentPath.end());

        res.steps.push_back(step);

        if (safeZoneSet.count(curr)) {
            targetExit = curr;
            break;
        }

        for (const auto& edge : graph.getEdgesFrom(curr)) {
            if (edge.isBlocked) continue;
            
            int neighbor = edge.toNode;
            const Node* neighborNode = graph.getNode(neighbor);
            if (!neighborNode || neighborNode->type == NodeType::Blocked) continue;

            if (visited.find(neighbor) == visited.end()) {
                visited.insert(neighbor);
                parent[neighbor] = curr;
                edgeUsed[neighbor] = &edge;
                q.push(neighbor);
            }
        }
    }

    if (targetExit != -1) {
        res.found = true;
        int curr = targetExit;
        float totalHazard = 0.0f;
        int edgeCount = 0;

        while (curr != startNodeId) {
            res.pathNodes.push_back(curr);
            int p = parent[curr];
            const Edge* e = edgeUsed[curr];
            if (e) {
                float radiusHeat = graph.getEdgeHazardRadiusInfluence(p, curr);
                res.totalDistance += e->distance;
                res.totalCost += e->getEffectiveCost(radiusHeat);
                totalHazard += std::min(1.0f, e->hazardLevel + radiusHeat);
                edgeCount++;
            }
            curr = p;
        }
        res.pathNodes.push_back(startNodeId);
        std::reverse(res.pathNodes.begin(), res.pathNodes.end());

        if (edgeCount > 0) res.avgHazardLevel = totalHazard / edgeCount;
    }

    computeAnalytics(graph, res);

    auto endTime = std::chrono::high_resolution_clock::now();
    res.executionMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
    return res;
}

// 2. Greedy Best-First Search: Guided purely by Euclidean distance to nearest uncompromised safe exit
PathResult EvacSolver::solveGreedy(const Graph& graph, int startNodeId) {
    PathResult res;
    res.algorithmName = "Greedy Best-First Search";
    auto startTime = std::chrono::high_resolution_clock::now();

    const Node* startNode = graph.getNode(startNodeId);
    if (!startNode || startNode->type == NodeType::Blocked) {
        res.found = false;
        computeAnalytics(graph, res);
        return res;
    }

    std::vector<int> validSafeZones = graph.getValidSafeZoneIds();
    if (validSafeZones.empty()) {
        res.found = false;
        computeAnalytics(graph, res);
        auto endTime = std::chrono::high_resolution_clock::now();
        res.executionMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        return res;
    }

    std::unordered_set<int> safeZoneSet(validSafeZones.begin(), validSafeZones.end());

    if (safeZoneSet.count(startNodeId)) {
        res.found = true;
        res.pathNodes = {startNodeId};
        computeAnalytics(graph, res);
        auto endTime = std::chrono::high_resolution_clock::now();
        res.executionMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        return res;
    }

    struct PQNode {
        int nodeId;
        float heuristic;
        bool operator>(const PQNode& other) const {
            return heuristic > other.heuristic;
        }
    };

    std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> pq;
    std::unordered_map<int, int> parent;
    std::unordered_map<int, const Edge*> edgeUsed;
    std::unordered_set<int> visited;

    float startH = minHeuristicDistance(graph, startNodeId, validSafeZones);
    pq.push({startNodeId, startH});
    visited.insert(startNodeId);

    int targetExit = -1;

    while (!pq.empty()) {
        auto top = pq.top();
        pq.pop();
        int curr = top.nodeId;
        res.nodesVisited++;

        SearchStep step;
        step.currentNodeId = curr;
        step.visitedNodes = std::vector<int>(visited.begin(), visited.end());

        std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> tempPQ = pq;
        while (!tempPQ.empty()) {
            step.frontierNodes.push_back(tempPQ.top().nodeId);
            tempPQ.pop();
        }

        int pCurr = curr;
        while (pCurr != startNodeId) {
            step.currentPath.push_back(pCurr);
            pCurr = parent[pCurr];
        }
        step.currentPath.push_back(startNodeId);
        std::reverse(step.currentPath.begin(), step.currentPath.end());

        res.steps.push_back(step);

        if (safeZoneSet.count(curr)) {
            targetExit = curr;
            break;
        }

        for (const auto& edge : graph.getEdgesFrom(curr)) {
            if (edge.isBlocked) continue;

            int neighbor = edge.toNode;
            const Node* neighborNode = graph.getNode(neighbor);
            if (!neighborNode || neighborNode->type == NodeType::Blocked) continue;

            if (visited.find(neighbor) == visited.end()) {
                visited.insert(neighbor);
                parent[neighbor] = curr;
                edgeUsed[neighbor] = &edge;

                float h = minHeuristicDistance(graph, neighbor, validSafeZones);
                pq.push({neighbor, h});
            }
        }
    }

    if (targetExit != -1) {
        res.found = true;
        int curr = targetExit;
        float totalHazard = 0.0f;
        int edgeCount = 0;

        while (curr != startNodeId) {
            res.pathNodes.push_back(curr);
            int p = parent[curr];
            const Edge* e = edgeUsed[curr];
            if (e) {
                float radiusHeat = graph.getEdgeHazardRadiusInfluence(p, curr);
                res.totalDistance += e->distance;
                res.totalCost += e->getEffectiveCost(radiusHeat);
                totalHazard += std::min(1.0f, e->hazardLevel + radiusHeat);
                edgeCount++;
            }
            curr = p;
        }
        res.pathNodes.push_back(startNodeId);
        std::reverse(res.pathNodes.begin(), res.pathNodes.end());

        if (edgeCount > 0) res.avgHazardLevel = totalHazard / edgeCount;
    }

    computeAnalytics(graph, res);

    auto endTime = std::chrono::high_resolution_clock::now();
    res.executionMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
    return res;
}

// 3. Dijkstra: Complete optimal safety path solver (evaluates distance, congestion & spreading hazard heat to uncompromised exits)
PathResult EvacSolver::solveDijkstra(const Graph& graph, int startNodeId) {
    PathResult res;
    res.algorithmName = "Dijkstra (Weighted Safety Optimal)";
    auto startTime = std::chrono::high_resolution_clock::now();

    const Node* startNode = graph.getNode(startNodeId);
    if (!startNode || startNode->type == NodeType::Blocked) {
        res.found = false;
        computeAnalytics(graph, res);
        return res;
    }

    std::vector<int> validSafeZones = graph.getValidSafeZoneIds();
    if (validSafeZones.empty()) {
        res.found = false;
        computeAnalytics(graph, res);
        auto endTime = std::chrono::high_resolution_clock::now();
        res.executionMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        return res;
    }

    std::unordered_set<int> safeZoneSet(validSafeZones.begin(), validSafeZones.end());

    if (safeZoneSet.count(startNodeId)) {
        res.found = true;
        res.pathNodes = {startNodeId};
        computeAnalytics(graph, res);
        auto endTime = std::chrono::high_resolution_clock::now();
        res.executionMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        return res;
    }

    struct DistNode {
        int nodeId;
        float cost;
        bool operator>(const DistNode& other) const {
            return cost > other.cost;
        }
    };

    std::priority_queue<DistNode, std::vector<DistNode>, std::greater<DistNode>> pq;
    std::unordered_map<int, float> minCost;
    std::unordered_map<int, int> parent;
    std::unordered_map<int, const Edge*> edgeUsed;
    std::unordered_set<int> visited;

    minCost[startNodeId] = 0.0f;
    pq.push({startNodeId, 0.0f});

    int targetExit = -1;

    while (!pq.empty()) {
        auto top = pq.top();
        pq.pop();
        int curr = top.nodeId;
        float currentCost = top.cost;

        if (currentCost > minCost[curr]) continue;
        visited.insert(curr);
        res.nodesVisited++;

        SearchStep step;
        step.currentNodeId = curr;
        step.visitedNodes = std::vector<int>(visited.begin(), visited.end());

        std::priority_queue<DistNode, std::vector<DistNode>, std::greater<DistNode>> tempPQ = pq;
        while (!tempPQ.empty()) {
            step.frontierNodes.push_back(tempPQ.top().nodeId);
            tempPQ.pop();
        }

        int pCurr = curr;
        while (pCurr != startNodeId) {
            step.currentPath.push_back(pCurr);
            pCurr = parent[pCurr];
        }
        step.currentPath.push_back(startNodeId);
        std::reverse(step.currentPath.begin(), step.currentPath.end());

        res.steps.push_back(step);

        if (safeZoneSet.count(curr)) {
            targetExit = curr;
            break;
        }

        for (const auto& edge : graph.getEdgesFrom(curr)) {
            if (edge.isBlocked) continue;

            int neighbor = edge.toNode;
            const Node* neighborNode = graph.getNode(neighbor);
            if (!neighborNode || neighborNode->type == NodeType::Blocked) continue;

            float radiusHeat = graph.getEdgeHazardRadiusInfluence(curr, neighbor);
            float newCost = currentCost + edge.getEffectiveCost(radiusHeat);

            if (minCost.find(neighbor) == minCost.end() || newCost < minCost[neighbor]) {
                minCost[neighbor] = newCost;
                parent[neighbor] = curr;
                edgeUsed[neighbor] = &edge;
                pq.push({neighbor, newCost});
            }
        }
    }

    if (targetExit != -1) {
        res.found = true;
        int curr = targetExit;
        float totalHazard = 0.0f;
        int edgeCount = 0;

        while (curr != startNodeId) {
            res.pathNodes.push_back(curr);
            int p = parent[curr];
            const Edge* e = edgeUsed[curr];
            if (e) {
                float radiusHeat = graph.getEdgeHazardRadiusInfluence(p, curr);
                res.totalDistance += e->distance;
                res.totalCost += e->getEffectiveCost(radiusHeat);
                totalHazard += std::min(1.0f, e->hazardLevel + radiusHeat);
                edgeCount++;
            }
            curr = p;
        }
        res.pathNodes.push_back(startNodeId);
        std::reverse(res.pathNodes.begin(), res.pathNodes.end());

        if (edgeCount > 0) res.avgHazardLevel = totalHazard / edgeCount;
    }

    computeAnalytics(graph, res);

    auto endTime = std::chrono::high_resolution_clock::now();
    res.executionMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
    return res;
}

} // namespace Evac
